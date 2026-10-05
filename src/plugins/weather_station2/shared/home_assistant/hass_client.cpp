/**
 * Author: sascha_lammers@gmx.de
 */

#include "hass_client.h"

#include <HTTPClient.h>
#include <JPEGDEC.h>
#include <StrView.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#ifndef DEBUG_WEATHER_STATION2
#    define DEBUG_WEATHER_STATION2 1
#endif

#if DEBUG_WEATHER_STATION2
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace WeatherStation2 {
namespace HomeAssistant {

// ------------------------------------------------------------------------------------------
// helpers
// ------------------------------------------------------------------------------------------
namespace {

// appends "<entity>.<key>":<prefix><entity><suffix> to a JSON object. The entity is the only
// value of the templates of this module, so the literal parts around it are appended as they are -
// a printf would parse a format for nothing but one string. The frame of the entry is a plain
// append for the same reason
void _appendJsonEntry(String &output, bool &first, const char *entity, const char *key, const char *prefix, const char *suffix)
{
    const auto entityLength = strlen(entity);
    const auto keyLength = strlen(key);
    const auto prefixLength = strlen(prefix);
    const auto suffixLength = strlen(suffix);
    const size_t length = (first ? 0 : 1) + 1 + entityLength + 1 + keyLength + 2 + prefixLength + entityLength + suffixLength;
    output.reserve(static_cast<unsigned int>(output.length() + length));
    if (!first) {
        output += ',';
    }
    first = false;
    output += '"';
    output.concat(entity, entityLength);
    output += '.';
    output.concat(key, keyLength);
    output.concat("\":", 2);
    output.concat(prefix, prefixLength);
    output.concat(entity, entityLength);
    output.concat(suffix, suffixLength);
}

// Attributes that describe what an entity offers (which buttons its panel has, the effects). They
// are part of the subscribed template, so the panel of a tile is complete the moment it is opened
void _appendCapabilityEntries(String &output, bool &first, const char *entity)
{
    _appendJsonEntry(output, first, entity, "color_modes", "{{ (state_attr('", "','supported_color_modes') or []) | join(',') | tojson }}");
    _appendJsonEntry(output, first, entity, "has_level", "{{ 1 if state_attr('", "','brightness') is not none else 0 }}");
    _appendJsonEntry(output, first, entity, "effect_list", "{{ (state_attr('", "','effect_list') or []) | join(',') | tojson }}");
    _appendJsonEntry(output, first, entity, "effect", "{{ state_attr('", "','effect') | tojson }}");
    _appendJsonEntry(output, first, entity, "color_temp", "{{ state_attr('", "','color_temp_kelvin') | int(0) }}");
    _appendJsonEntry(output, first, entity, "min_color_temp", "{{ state_attr('", "','min_color_temp_kelvin') | int(0) }}");
    _appendJsonEntry(output, first, entity, "max_color_temp", "{{ state_attr('", "','max_color_temp_kelvin') | int(0) }}");
    _appendJsonEntry(output, first, entity, "hs_color", "{{ (state_attr('", "','hs_color') or [0,0]) | join(',') | tojson }}");
}

// The attributes of the entity of the panel that is open. They are part of the subscribed template
// while the panel is open, so a change of the entity updates the control of the panel with the
// same message that carries the values of the tiles (the state is rendered for every tile already)
void _appendDetailEntries(String &output, bool &first, const char *entity)
{
    _appendJsonEntry(output, first, entity, "preset_mode", "{{ state_attr('", "','preset_mode') | tojson }}");
    _appendJsonEntry(output, first, entity, "preset_modes", "{{ (state_attr('", "','preset_modes') or []) | join(',') | tojson }}");
    _appendJsonEntry(output, first, entity, "fan_mode", "{{ state_attr('", "','fan_mode') | tojson }}");
    _appendJsonEntry(output, first, entity, "fan_modes", "{{ (state_attr('", "','fan_modes') or []) | join(',') | tojson }}");
    _appendJsonEntry(output, first, entity, "hvac_modes", "{{ (state_attr('", "','hvac_modes') or []) | join(',') | tojson }}");
    _appendJsonEntry(output, first, entity, "color_mode", "{{ state_attr('", "','color_mode') | tojson }}");
    _appendJsonEntry(output, first, entity, "brightness", "{{ state_attr('", "','brightness') | int(0) }}");
    _appendCapabilityEntries(output, first, entity);
}

// ------------------------------------------------------------------------------------------
// camera image of a picture tile
// ------------------------------------------------------------------------------------------
//
// The camera proxy of Home Assistant answers one JPEG per request (GET
// /api/camera_proxy/<entity>?width=W&height=H with the bearer token). It scales the image of the
// camera with a fixed factor ladder and never crops it, so the response is usually larger than
// the requested box and keeps the aspect ratio of the camera. The tile is filled with the centre
// crop of that image, which is written into the LVGL buffer straight from the decoder - no
// temporary frame buffer and no copy.
//
// The decoder (bitbank2/JPEGDEC) converts the image block by block and hands every block to a
// callback. The blocks arrive in output (scaled) pixels, left to right, top to bottom, each one
// `iWidth` pixels wide (the pitch of the pixel buffer) with `iWidthUsed` valid pixels per row.
// The callback of this module maps the crop window to the tile with a nearest neighbour
// lookup table, so a block only touches the destination pixels it really covers.

// The crop window (the tile's aspect ratio, centred) and the mapping of a tile of
// `destWidth` x `destHeight` pixels.
//
// The two lookup tables are file static and not members by value: they are 1.9 KB and would sit
// on the stack of the request task (`_decodeImage()`), which only has 10 KB. One image is decoded
// at a time (see the comment of kMaxImageTiles), so a single pair of tables is enough
static uint16_t _imageColMap[Client::kMaxImageWidth];
static uint16_t _imageRowMap[Client::kMaxImageHeight];
struct ImageJob {
    ImageJob() :
        dest(nullptr),
        destWidth(0),
        destHeight(0),
        cropX(0),
        cropY(0),
        cropW(0),
        cropH(0),
        colMap(_imageColMap),
        rowMap(_imageRowMap)
    {
    }
    uint16_t *dest;
    uint16_t destWidth;
    uint16_t destHeight;
    // first column/row of the scaled source image
    uint16_t cropX;
    uint16_t cropY;
    // size of the crop window inside the scaled source image
    uint16_t cropW;
    uint16_t cropH;
    // scaled source pixel of every destination pixel
    uint16_t *colMap;
    uint16_t *rowMap;
};

// Scale factor of the decoder: the largest down scaling (1/2, 1/4, 1/8) whose result still
// covers the tile in both axes, so the decoder does as little work as possible without
// upscaling. 1 (no scaling) when even the full image is smaller than the tile
int _imageScale(int sourceWidth, int sourceHeight, int width, int height)
{
    if (sourceWidth / 8 >= width && sourceHeight / 8 >= height) {
        return JPEG_SCALE_EIGHTH;
    }
    if (sourceWidth / 4 >= width && sourceHeight / 4 >= height) {
        return JPEG_SCALE_QUARTER;
    }
    if (sourceWidth / 2 >= width && sourceHeight / 2 >= height) {
        return JPEG_SCALE_HALF;
    }
    return 0;
}

// number of bits of a scale factor of JPEGDEC
int _imageScaleShift(int scale)
{
    return (scale == JPEG_SCALE_HALF) ? 1 : (scale == JPEG_SCALE_QUARTER) ? 2 : (scale == JPEG_SCALE_EIGHTH) ? 3 : 0;
}

// The crop window is the largest rect with the aspect ratio of the tile that fits into the
// scaled source image, centred ("cover": the tile is filled, the ratio of the camera is kept)
void _imageLayout(int sourceWidth, int sourceHeight, ImageJob &job)
{
    int width = sourceHeight * job.destWidth / job.destHeight;
    int height = sourceHeight;
    if (width > sourceWidth) {
        width = sourceWidth;
        height = sourceWidth * job.destHeight / job.destWidth;
    }
    if (width < 1) {
        width = 1;
    }
    if (height < 1) {
        height = 1;
    }
    job.cropW = static_cast<uint16_t>(width);
    job.cropH = static_cast<uint16_t>(height);
    job.cropX = static_cast<uint16_t>((sourceWidth - width) / 2);
    job.cropY = static_cast<uint16_t>((sourceHeight - height) / 2);
    // nearest neighbour, the centre of a destination pixel decides (the tables are small enough
    // to be computed once instead of dividing per pixel)
    for (uint16_t x = 0; x < job.destWidth; x++) {
        auto source = job.cropX + (static_cast<uint32_t>(x) * width + (width / 2)) / job.destWidth;
        job.colMap[x] = static_cast<uint16_t>((source < sourceWidth) ? source : (sourceWidth - 1));
    }
    for (uint16_t y = 0; y < job.destHeight; y++) {
        auto source = job.cropY + (static_cast<uint32_t>(y) * height + (height / 2)) / job.destHeight;
        job.rowMap[y] = static_cast<uint16_t>((source < sourceHeight) ? source : (sourceHeight - 1));
    }
}

// one block of the decoder, block by block
int _imageDrawCallback(JPEGDRAW *draw)
{
    auto job = static_cast<ImageJob *>(draw->pUser);
    if (!job) {
        return 0;
    }
    // the block covers [draw->x, draw->x + width) x [draw->y, draw->y + draw->iHeight) of the
    // scaled image, `iWidth` is the pitch of the pixel buffer (iWidthUsed valid pixels per row)
    const int blockX = draw->x;
    const int blockY = draw->y;
    const int blockW = (draw->iWidthUsed > 0 && draw->iWidthUsed <= draw->iWidth) ? draw->iWidthUsed : draw->iWidth;
    const int blockH = draw->iHeight;
    const int pitch = draw->iWidth;
    if (blockX + blockW <= job->cropX || blockX >= job->cropX + job->cropW || blockY + blockH <= job->cropY || blockY >= job->cropY + job->cropH) {
        return 1; // outside the crop window, the tile does not show any of it
    }
    for (uint16_t y = 0; y < job->destHeight; y++) {
        const int sourceY = job->rowMap[y];
        if (sourceY < blockY || sourceY >= blockY + blockH) {
            continue;
        }
        const uint16_t *source = draw->pPixels + (sourceY - blockY) * pitch;
        uint16_t *dest = job->dest + static_cast<uint32_t>(y) * job->destWidth;
        for (uint16_t x = 0; x < job->destWidth; x++) {
            const int sourceX = job->colMap[x];
            if (sourceX < blockX || sourceX >= blockX + blockW) {
                continue;
            }
            dest[x] = source[sourceX - blockX];
        }
    }
    return 1; // continue decoding
}

} // namespace

// ------------------------------------------------------------------------------------------
// lifecycle
// ------------------------------------------------------------------------------------------
Client::Client() :
    _config(nullptr),
    _templatePage(kNoPage),
    _responsePage(kNoPage),
    _detailValid(false),
    _detailTile(kNoDetailTile),
    _responseValid(false),
    _statusCode(0),
    _duration(0),
    _responseTime(0),
    _requestCount(0),
    _actionCount(0),
    _active(false),
    _refreshRequested(false),
    _stop(false),
    _visiblePage(0),
    _templateDirty(false),
    _task(nullptr),
    _resubscribe(false),
    _queue{},
    _queueHead(0),
    _queueTail(0),
    _images{},
    _image(nullptr),
    _imageWidth(0),
    _imageHeight(0),
    _imageTile(0),
    _imageStamp(0),
    _imageValid(false),
    _imageCount(0),
    _imageFailureCount(0),
    _statsTile(kNoStatsTile),
    _statsHours(kDefaultStatsHours),
    _statsRequest(false),
    _statsPoints{},
    _statsCount(0),
    _statsResultTile(kNoStatsTile),
    _statsResultHours(0),
    _statsStart(0),
    _statsEnd(0),
    _statsGeneration(0),
    _statsUntaken(false)
{
}

Client::~Client()
{
    stop();
}

bool Client::begin(const Config &config)
{
    stop();
    if (_task) {
        // the previous task is still inside a request, a second one would double the traffic
        _setError("the request task is still running");
        return false;
    }

    _config = &config;
    _buildTemplate();
    // one bit per tile, the failures are handed to the dashboard by takeActionFailure()
    _actionFailed.assign((config.getTileCount() + 31) / 32, 0);
    _response = String();
    _error = String();
    _responseValid = false;
    _statusCode = 0;
    _duration = 0;
    _responseTime = 0;
    _requestCount = 0;
    _actionCount = 0;
    _queueHead = 0;
    _queueTail = 0;
    for (auto &slot : _images) {
        slot.used = false;
    }
    _refreshRequested = true;
    _stop = false;
    _templateDirty = false;
    _resubscribe = false;

    // The task rebuilds the template when the visible page changes, so the length is read before
    // it can touch it
    const auto templateLength = _template.length();
    TaskHandle_t handle = nullptr;
    if (xTaskCreate(_taskEntry, "hass-ws", kTaskStack, this, 1, &handle) == pdPASS) {
        _task = handle;
        __LDBG_printf("request task started (%u tile(s), page %u, template %u bytes, stack %u)", static_cast<unsigned>(config.getTileCount()),
                      static_cast<unsigned>(_visiblePage), static_cast<unsigned>(templateLength), static_cast<unsigned>(kTaskStack));
        return true;
    }
    _setError("cannot start the request task");
    return false;
}

void Client::stop()
{
    if (!_task) {
        return;
    }
    _stop = true;
    // a request can block for the timeout of the configuration, give it time to finish. The task
    // is not touched while it runs, it only reads the configuration
    uint32_t waitMs = 10000;
    if (_config) {
        waitMs += _config->getTimeout() * 1000;
    }
    for (uint32_t waited = 0; waited < waitMs && _task; waited += 50) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    if (_task) {
        __LDBG_printf("request task did not stop within %ums", static_cast<unsigned>(waitMs));
        return;
    }
    MUTEX_LOCK_BLOCK(_lock) {
        _queueHead = 0;
        _queueTail = 0;
        _response = String();
        _error = String();
        _responseValid = false;
        // a frame that the LVGL task did not collect yet
        free(_image);
        _image = nullptr;
        _imageValid = false;
        for (auto &slot : _images) {
            slot.used = false;
        }
    }
    _template = String();
    _active = false;
    _refreshRequested = false;
    _socket.close();
}

// The connection is only subscribed while the screen is visible: the values of the page that is
// shown are pushed by Home Assistant and nothing is requested while the screen is off. Leaving the
// screen drops the subscription (the task unsubscribes and closes the connection) and opening it
// subscribes the values of the visible page again
void Client::setActive(bool active)
{
    _active = active;
    if (active) {
        // the template is subscribed and the values arrive right away, this only makes sure that
        // the task does not wait for the first resync interval
        _templateDirty = true;
    }
}

// Only the tiles of the page that is visible are subscribed: the template grows with the number of
// tiles, and a page that is not shown does not have to be up to date (it is subscribed the moment
// it is opened, see Dashboard::setVisiblePage()). The template is built by the request task, which
// is the only writer of _template while it runs.
void Client::setVisiblePage(PageIndex page)
{
    if (_visiblePage == page) {
        return;
    }
    _visiblePage = page;
    _templateDirty = true;
}

// Requests the values of the visible page again (one render on the Home Assistant side, the answer
// is pushed like any other change). Used when the screen is opened and by the safety net of the
// subscription (see kResyncInterval)
void Client::requestRefresh()
{
    _resubscribe = true;
}

void Client::requestDetail(TileIndex tile)
{
    if (_detailTile != tile) {
        _detailTile = tile;
        // the template of the visible page carries the attributes of the entity of the panel, the
        // task builds and subscribes it again
        _templateDirty = true;
    }
}

void Client::closeDetail()
{
    if (_detailTile == kNoDetailTile) {
        return;
    }
    _detailTile = kNoDetailTile;
    _templateDirty = true;
}

bool Client::queueAction(const Action &action)
{
    bool queued = false;
    MUTEX_LOCK_BLOCK(_lock) {
        const auto next = static_cast<uint8_t>((_queueTail + 1) % kQueueSize);
        if (next != _queueHead) {
            _queue[_queueTail] = action;
            _queueTail = next;
            queued = true;
        }
    }
    __LDBG_printf("hass> queued action %u of tile %u: %s (%u in queue)", static_cast<unsigned>(action.type),
                  static_cast<unsigned>(action.tile), queued ? "ok" : "FULL",
                  static_cast<unsigned>((_queueTail + kQueueSize - _queueHead) % kQueueSize));
    return queued;
}

bool Client::takeActionFailure(TileIndex &tile)
{
    MUTEX_LOCK_BLOCK(_lock) {
        for (size_t word = 0; word < _actionFailed.size(); word++) {
            if (!_actionFailed[word]) {
                continue;
            }
            for (uint8_t bit = 0; bit < 32; bit++) {
                if (_actionFailed[word] & (1u << bit)) {
                    _actionFailed[word] &= ~(1u << bit);
                    tile = static_cast<TileIndex>(word * 32 + bit);
                    return true;
                }
            }
        }
    }
    return false;
}

bool Client::hasPendingAction()
{
    bool pending = false;
    MUTEX_LOCK_BLOCK(_lock) {
        pending = (_queueHead != _queueTail);
    }
    return pending;
}
// ------------------------------------------------------------------------------------------
// camera images of the picture tiles
// ------------------------------------------------------------------------------------------
void Client::requestImage(TileIndex tile, uint16_t width, uint16_t height, uint16_t interval)
{
    if (!_config || tile >= _config->getTileCount() || !width || !height) {
        return;
    }
    const auto &t = _config->getTile(tile);
    if (t.type != TileType::PICTURE || !t.entity[0]) {
        return;
    }
    if (width > kMaxImageWidth || height > kMaxImageHeight) {
        __LDBG_printf("hass> the box of tile %u is too large (%ux%u, the maximum is %ux%u)", static_cast<unsigned>(tile),
                      static_cast<unsigned>(width), static_cast<unsigned>(height), static_cast<unsigned>(kMaxImageWidth), static_cast<unsigned>(kMaxImageHeight));
        return;
    }
    if (!interval) {
        interval = t.refresh;
    }
    bool accepted = false;
    MUTEX_LOCK_BLOCK(_lock) {
        // the tile is registered already, the dashboard calls this on every update
        ImageRequest *slot = nullptr;
        for (auto &entry : _images) {
            if (entry.used && entry.tile == tile) {
                slot = &entry;
                break;
            }
        }
        if (slot) {
            if (slot->width != width || slot->height != height) {
                // the tile has a different box than in the last version of the screen, the
                // frame that is stored is useless
                __LDBG_printf("hass> the box of tile %u changed from %ux%u to %ux%u", static_cast<unsigned>(tile),
                              static_cast<unsigned>(slot->width), static_cast<unsigned>(slot->height),
                              static_cast<unsigned>(width), static_cast<unsigned>(height));
                slot->width = width;
                slot->height = height;
                slot->next = 0;
            }
            slot->interval = interval;
            accepted = true;
        }
        else {
            for (auto &entry : _images) {
                if (!entry.used) {
                    __LDBG_printf("hass> tile %u requests a camera image (%ux%u, every %us)", static_cast<unsigned>(tile),
                                  static_cast<unsigned>(width), static_cast<unsigned>(height), static_cast<unsigned>(interval));
                    entry.tile = tile;
                    entry.width = width;
                    entry.height = height;
                    entry.interval = interval;
                    entry.next = 0; // fetch as soon as possible (a new page was opened)
                    entry.used = true;
                    accepted = true;
                    break;
                }
            }
        }
    }
    if (!accepted) {
        __LDBG_printf("hass> no free image request slot for tile %u (the maximum is %u)", static_cast<unsigned>(tile), static_cast<unsigned>(kMaxImageTiles));
    }
}

void Client::clearImages()
{
    bool dropped = false;
    MUTEX_LOCK_BLOCK(_lock) {
        for (auto &slot : _images) {
            dropped |= slot.used;
            slot.used = false;
        }
    }
    if (dropped) {
        // the page that is left, or a screen that is not visible any more
        __LDBG_printf("hass> camera image requests dropped");
    }
}

bool Client::takeImage(TileIndex &tile, uint16_t *&data, uint16_t &width, uint16_t &height, uint32_t &stamp)
{
    bool available = false;
    MUTEX_LOCK_BLOCK(_lock) {
        if (_imageValid) {
            tile = _imageTile;
            data = _image;
            width = _imageWidth;
            height = _imageHeight;
            stamp = _imageStamp;
            _image = nullptr;
            _imageValid = false;
            available = true;
        }
    }
    return available;
}

uint32_t Client::_imageRetryDelay(uint16_t interval)
{
    const auto requested = static_cast<uint32_t>(interval) * 1000;
    return requested > kMinImageRetry ? requested : kMinImageRetry;
}

// ------------------------------------------------------------------------------------------
// history graph of a sensor panel
// ------------------------------------------------------------------------------------------
void Client::requestStats(TileIndex tile, uint8_t hours)
{
    if (!_config || tile >= _config->getTileCount()) {
        return;
    }
    if (hours < kMinStatsHours || hours > kMaxStatsHours) {
        hours = kDefaultStatsHours;
    }
    MUTEX_LOCK_BLOCK(_lock) {
        _statsTile = tile;
        _statsHours = hours;
        _statsRequest = true;
    }
    __LDBG_printf("hass> statistics of tile %u requested (%u hours)", static_cast<unsigned>(tile), static_cast<unsigned>(hours));
}

void Client::_fetchStats(TileIndex tile, uint8_t hours)
{
    if (!_config) {
        return;
    }
    const auto &t = _config->getTile(tile);
    const auto started = millis();
    uint32_t start = 0;
    uint32_t end = 0;
    uint16_t count = 0;
    String error;
    const auto history = t.hasStateHistory();
    const auto ok = _socket.fetchStats(t.entity, hours, history, _statsPoints, count, start, end, error);
    const auto duration = millis() - started;
    _statsCount = count;
    MUTEX_LOCK_BLOCK(_lock) {
        _statsResultTile = tile;
        _statsResultHours = hours;
        _statsStart = start;
        _statsEnd = end;
        _statsError = error;
        _statsGeneration++;
    }
    __LDBG_printf("hass> %s of tile %u (%s, %u hours) -> %u %s in %ums%s", history ? "history" : "statistics", static_cast<unsigned>(tile),
                  t.entity, static_cast<unsigned>(hours), static_cast<unsigned>(count), history ? "change(s)" : "bucket(s)",
                  static_cast<unsigned>(duration), ok ? "" : ":");
    if (!ok) {
        __LDBG_printf("hass>     %s", error.c_str());
    }
}

bool Client::takeStats(uint32_t &generation, TileIndex &tile, uint8_t &hours, uint32_t &start, uint32_t &end, Socket::Point *points,
                       uint16_t &count, String &error)
{
    bool available = false;
    MUTEX_LOCK_BLOCK(_lock) {
        if (_statsGeneration != generation) {
            generation = _statsGeneration;
            tile = _statsResultTile;
            hours = _statsResultHours;
            start = _statsStart;
            end = _statsEnd;
            count = _statsCount;
            error = _statsError;
            if (points && count) {
                memcpy(points, _statsPoints, static_cast<size_t>(count) * sizeof(Socket::Point));
            }
            // the buckets are copied, the task may request the next window now
            _statsUntaken = false;
            available = true;
        }
    }
    return available;
}

bool Client::_popAction(Action &action)
{
    bool popped = false;
    MUTEX_LOCK_BLOCK(_lock) {
        if (_queueHead != _queueTail) {
            action = _queue[_queueHead];
            _queueHead = static_cast<uint8_t>((_queueHead + 1) % kQueueSize);
            popped = true;
        }
    }
    return popped;
}

// ------------------------------------------------------------------------------------------
// task
// ------------------------------------------------------------------------------------------
void Client::_taskEntry(void *arg)
{
    auto self = static_cast<Client *>(arg);
    self->_loop();
    self->_task = nullptr;
    vTaskDelete(nullptr);
}

void Client::_loop()
{
    uint32_t nextConnect = 0;
    uint32_t connectDelay = kConnectRetryMin;
    uint32_t nextResync = 0;
    while (!_stop) {
        // the page that is visible changed, or a panel was opened or closed: the template is built
        // from the new tiles and subscribed again
        if (_templateDirty) {
            _templateDirty = false;
            _buildTemplate();
            _resubscribe = true;
            __LDBG_printf("hass> template rebuilt for page %u, panel %u (%u bytes)", static_cast<unsigned>(_visiblePage),
                          static_cast<unsigned>(_detailTile), static_cast<unsigned>(_template.length()));
        }
        // The connection: one websocket per screen, and only while it is visible. Home Assistant
        // closes an idle connection and it is gone while it updates itself, so the task opens it
        // again with a backoff - the subscription below is renewed with the new connection
        if (!_socket.isOpen()) {
            if (!_active) {
                vTaskDelay(pdMS_TO_TICKS(kIdleDelay));
                continue;
            }
            if (static_cast<int32_t>(millis() - nextConnect) < 0) {
                vTaskDelay(pdMS_TO_TICKS(kIdleDelay));
                continue;
            }
            if (!_socket.open(_config->getUrl(), _config->getToken())) {
                _setError(_socket.getError());
                _statusCode = -1;
                __LDBG_printf("hass> cannot connect (%s), next try in %ums", _socket.getError(), static_cast<unsigned>(connectDelay));
                nextConnect = millis() + connectDelay;
                connectDelay = (connectDelay * 2 > kConnectRetryMax) ? kConnectRetryMax : (connectDelay * 2);
                continue;
            }
            connectDelay = kConnectRetryMin;
            nextResync = millis() + ((_config ? _config->getPollInterval() : 60) * 1000);
            _setError(String());
            _resubscribe = true;
            __LDBG_printf("hass> connected (heap %u, stack high water %u)", static_cast<unsigned>(ESP.getFreeHeap()),
                          static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        }
        // (re)subscribe the values of the visible page. A connection that was opened again, a page
        // change, an open panel and the resync of the values all end up here. The template has to
        // be the one of the page that is visible now: the template of the page that is left is not
        // subscribed (the stale subscription would be dropped again one iteration later, and its
        // response would be applied to the tiles of the new page)
        if ((_resubscribe || !_socket.isSubscribed()) && _template.length() && _templatePage == _visiblePage) {
            _resubscribe = false;
            if (_socket.subscribeTemplate(_template, _templatePage)) {
                _statusCode = 200;
                __LDBG_printf("hass> subscribed page %u (%u bytes of template)", static_cast<unsigned>(_templatePage),
                              static_cast<unsigned>(_template.length()));
            }
            else {
                _setError(_socket.getError());
                __LDBG_printf("hass> cannot subscribe the page (%s)", _socket.getError());
            }
        }
        // The values are pushed now, this is only the safety net: a subscription cannot report a
        // value that changed while the connection was down (or while the screen was off), and it
        // proves that the subscription is still alive
        if (static_cast<int32_t>(millis() - nextResync) >= 0) {
            nextResync = millis() + ((_config ? _config->getPollInterval() : 60) * 1000);
            _resubscribe = true;
        }
        Action action;
        if (_popAction(action)) {
            _sendAction(action);
            continue;
        }
        // The history graph of an open sensor panel. It is fetched after the actions and the pushed
        // values (a state or an action is more urgent) and one at a time, like a camera image. The
        // main loop requests it when the panel is opened and once a minute. A result that was not
        // copied out yet (takeStats()) is not overwritten: the request waits for it
        if (_statsRequest && !_statsUntaken) {
            TileIndex statsTile = kNoStatsTile;
            uint8_t statsHours = kDefaultStatsHours;
            MUTEX_LOCK_BLOCK(_lock) {
                if (_statsRequest && !_statsUntaken) {
                    _statsRequest = false;
                    // set before the buckets are written, takeStats() clears it
                    _statsUntaken = true;
                    statsTile = _statsTile;
                    statsHours = _statsHours;
                }
            }
            if (statsTile != kNoStatsTile && _config && statsTile < _config->getTileCount()) {
                _fetchStats(statsTile, statsHours);
                continue;
            }
            _statsUntaken = false;
        }
        // A picture tile of the visible page. The image is the least urgent request of the task
        // (it is a preview), so it comes after everything else. One image at a time: the response
        // is 40-80 KB and the decode takes the better part of the task's time
        ImageRequest image;
        bool hasImage = false;
        MUTEX_LOCK_BLOCK(_lock) {
            for (auto &slot : _images) {
                if (slot.used && static_cast<int32_t>(millis() - slot.next) >= 0) {
                    image = slot;
                    slot.next = millis() + static_cast<uint32_t>(slot.interval) * 1000;
                    hasImage = true;
                    break;
                }
            }
        }
        if (hasImage) {
            if (!_fetchImage(image)) {
                _imageFailureCount++;
                MUTEX_LOCK_BLOCK(_lock) {
                    for (auto &slot : _images) {
                        if (slot.used && slot.tile == image.tile) {
                            slot.next = millis() + _imageRetryDelay(slot.interval);
                            break;
                        }
                    }
                }
            }
            continue;
        }
        // The events of the subscription, the answers of the requests and the pings of Home
        // Assistant. The task is idle most of the time and the events arrive a few times per minute
        if (!_socket.pump(kPumpInterval)) {
            const auto *reason = _socket.getError();
            _setError((reason && reason[0]) ? String(reason) : String("the connection was closed"));
            _statusCode = -1;
            continue;
        }
        // the values of the visible page
        _takePushedTemplate();
        // a service call that Home Assistant rejected
        TileIndex failedTile = 0;
        while (_socket.takeActionFailure(failedTile)) {
            MUTEX_LOCK_BLOCK(_lock) {
                _actionFailed[failedTile >> 5] |= (1u << (failedTile & 31));
            }
        }
    }
    _socket.close();
    __LDBG_printf("request task ended");
}

// The rendered template of the subscription, handed over to the main loop. It is sent when one of
// the entities the template reads changes and only when the result differs from the previous one
void Client::_takePushedTemplate()
{
    String text;
    String error;
    PageIndex page = kNoPage;
    if (!_socket.takeTemplateResult(text, error, page)) {
        return;
    }
    if (error.length()) {
        __LDBG_printf("hass> the template reported an error: %s", error.c_str());
        String message("template: ");
        message += error;
        _setError(message);
        return;
    }
    if (!text.length()) {
        return;
    }
    _requestCount++;
    MUTEX_LOCK_BLOCK(_lock) {
        _response = text;
        _responsePage = page;
        _responseTime = millis();
        _responseValid = true;
        // the panel that is open reads the same message: the attributes of its entity are part of
        // the subscribed template
        if (_detailTile != kNoDetailTile) {
            _detailResponse = text;
            _detailValid = true;
        }
    }
    __LDBG_printf("hass> %u bytes pushed for page %u", static_cast<unsigned>(text.length()), static_cast<unsigned>(page));
}

void Client::_sendAction(const Action &action)
{
    if (!_config) {
        return;
    }
    String domain;
    String service;
    String data;
    _buildAction(action, domain, service, data);
    if (!domain.length() || !service.length() || !data.length()) {
        __LDBG_printf("cannot build action %u", static_cast<unsigned>(action.type));
        return;
    }
    // The answer of the call confirms it, a failed one is reported to the dashboard (which reverts
    // the optimistic state of the tile). The entity reports its new state by itself: the
    // subscription pushes it, no additional request is needed
    if (_socket.callService(action.tile, domain.c_str(), service.c_str(), data.c_str())) {
        _actionCount++;
    }
    __LDBG_printf("hass> action %u of tile %u: %s/%s %s", static_cast<unsigned>(action.type), static_cast<unsigned>(action.tile),
                  domain.c_str(), service.c_str(), data.c_str());
}

// ------------------------------------------------------------------------------------------
// detail attributes of an open panel
// ------------------------------------------------------------------------------------------
bool Client::takeDetailResponse(String &response)
{
    bool available = false;
    MUTEX_LOCK_BLOCK(_lock) {
        if (_detailValid) {
            response = _detailResponse;
            _detailResponse = String();
            _detailValid = false;
            available = true;
        }
    }
    return available;
}

// ------------------------------------------------------------------------------------------
// requests
// ------------------------------------------------------------------------------------------
bool Client::_isHttps() const
{
    return _config && _config->getUrl() && !strncasecmp(_config->getUrl(), "https://", 8);
}

// camera image of a picture tile
// ------------------------------------------------------------------------------------------
bool Client::_fetchImage(const ImageRequest &request)
{
    if (!_config) {
        return false;
    }
    const auto &tile = _config->getTile(request.tile);
    if (!tile.entity[0]) {
        return false;
    }
    // The box of the tile is what Home Assistant gets as width/height: it returns the smallest
    // of its scaling factors that still covers the box (and never crops), so asking for exactly
    // the box of the tile is the smallest possible transfer
    String url(_config->getUrl());
    url += "/api/camera_proxy/";
    url += tile.entity;
    StrWrapper(url).printf("?width=%u&height=%u", static_cast<unsigned>(request.width), static_cast<unsigned>(request.height));

    uint8_t *jpeg = nullptr;
    size_t length = 0;
    int16_t statusCode = 0;
    const auto started = millis();
    if (!_get(url, jpeg, length, statusCode)) {
        free(jpeg);
        __LDBG_printf("hass> camera %s: request failed after %ums (status %d)", tile.entity, static_cast<unsigned>(millis() - started), static_cast<int>(statusCode));
        return false;
    }
    uint16_t *pixels = nullptr;
    String info;
    const auto ok = _decodeImage(jpeg, length, request.width, request.height, pixels, info);
    const auto duration = millis() - started;
    free(jpeg);
    if (!ok) {
        free(pixels);
        __LDBG_printf("hass> camera %s: %s (after %ums)", tile.entity, info.c_str(), static_cast<unsigned>(duration));
        return false;
    }
    _imageCount++;
    __LDBG_printf("hass> camera %s: %s, %ums", tile.entity, info.c_str(), static_cast<unsigned>(duration));
    // hand the frame over to the LVGL task. A frame that nobody collected is replaced (and
    // released), the screen always shows the newest image
    uint16_t *collected = nullptr;
    MUTEX_LOCK_BLOCK(_lock) {
        collected = _image;
        _image = pixels;
        _imageWidth = request.width;
        _imageHeight = request.height;
        _imageTile = request.tile;
        _imageStamp++;
        _imageValid = true;
    }
    free(collected);
    return true;
}

bool Client::_decodeImage(const uint8_t *data, size_t length, uint16_t width, uint16_t height, uint16_t *&pixels, String &info)
{
    // The decoder state is about 18 KB (the quantization, Huffman and pixel buffers of the
    // library), too much for the stack of the request task - and it is not needed permanent
    auto jpeg = static_cast<JPEGDEC *>(ps_malloc(sizeof(JPEGDEC)));
    if (!jpeg) {
        info = "out of memory (decoder)";
        return false;
    }
    bool ok = false;
    if (!jpeg->openRAM(const_cast<uint8_t *>(data), static_cast<int>(length), _imageDrawCallback)) {
        StrWrapper(info).printf("not a JPEG (error %d)", jpeg->getLastError());
    }
    else {
        const auto sourceWidth = jpeg->getWidth();
        const auto sourceHeight = jpeg->getHeight();
        // A progressive JPEG is decoded from the DC coefficients of the first scan only, the
        // library always returns 1/8 of the image in that case (see DecodeJPEG() of JPEGDEC)
        const auto progressive = (jpeg->getJPEGType() == JPEG_MODE_PROGRESSIVE);
        const auto scale = progressive ? JPEG_SCALE_EIGHTH : _imageScale(sourceWidth, sourceHeight, width, height);
        const auto shift = _imageScaleShift(scale);
        // the size of the scaled image, rounded up the same way the decoder does
        const auto scaledWidth = (sourceWidth + ((1 << shift) - 1)) >> shift;
        const auto scaledHeight = (sourceHeight + ((1 << shift) - 1)) >> shift;

        ImageJob job;
        job.destWidth = width;
        job.destHeight = height;
        _imageLayout(scaledWidth, scaledHeight, job);
        job.dest = static_cast<uint16_t *>(ps_malloc(static_cast<size_t>(width) * height * sizeof(uint16_t)));
        if (!job.dest) {
            info = "out of memory (image)";
        }
        else {
            // RGB565 with the bytes in big endian order: this is what LVGL expects with
            // LV_COLOR_16_SWAP 1 (see lv_conf.h)
            jpeg->setPixelType(RGB565_BIG_ENDIAN);
            jpeg->setUserPointer(&job);
            const auto started = millis();
            ok = jpeg->decode(0, 0, scale) != 0;
            const auto decodeTime = millis() - started;
            if (ok) {
                StrWrapper(info).printf("jpeg %dx%d %u bytes%s, crop %ux%u at (%u,%u) -> %ux%u in %ums",
                                   sourceWidth, sourceHeight, static_cast<unsigned>(length),
                                   progressive ? " progressive (DC only)" : "",
                                   static_cast<unsigned>(job.cropW), static_cast<unsigned>(job.cropH),
                                   static_cast<unsigned>(job.cropX), static_cast<unsigned>(job.cropY),
                                   static_cast<unsigned>(width), static_cast<unsigned>(height),
                                   static_cast<unsigned>(decodeTime));
                pixels = job.dest;
            }
            else {
                free(job.dest);
                StrWrapper(info).printf("decode failed (error %d)", jpeg->getLastError());
            }
        }
    }
    free(jpeg);
    return ok;
}

bool Client::_get(const String &url, uint8_t *&data, size_t &length, int16_t &statusCode)
{
    data = nullptr;
    length = 0;
    statusCode = 0;
    if (!_config) {
        _setError("not configured");
        return false;
    }
    if (!WiFi.isConnected()) {
        _setError("WiFi is not connected");
        return false;
    }

    const auto timeout = _config->getTimeout() * 1000;
    bool ok = false;
    if (_isHttps()) {
        WiFiClientSecure client;
        // the device has no certificate store and the panel runs in the local network
        client.setInsecure();
        HTTPClient http;
        if (http.begin(client, url)) {
            ok = _getBody(http, data, length, statusCode, timeout);
            http.end();
        }
        else {
            statusCode = -1;
        }
    }
    else {
        WiFiClient client;
        HTTPClient http;
        if (http.begin(client, url)) {
            ok = _getBody(http, data, length, statusCode, timeout);
            http.end();
        }
        else {
            statusCode = -1;
        }
    }
    if (!ok && !_error.length()) {
        if (statusCode <= 0) {
            _setError("connection failed");
        }
        else {
            String message;
            StrWrapper(message).printf("HTTP %d", static_cast<int>(statusCode));
            _setError(message);
        }
    }
    return ok;
}

bool Client::_getBody(HTTPClient &http, uint8_t *&data, size_t &length, int16_t &statusCode, uint32_t timeout)
{
    http.setTimeout(timeout);
    http.setConnectTimeout(timeout);
    http.setReuse(false);
    String authorization("Bearer ");
    authorization += _config->getToken();
    http.addHeader(F("Authorization"), authorization);
    http.addHeader(F("Accept"), F("image/jpeg"));

    const auto status = http.GET();
    statusCode = static_cast<int16_t>(status);
    if (status != HTTP_CODE_OK) {
        return false;
    }
    auto stream = http.getStreamPtr();
    if (!stream) {
        return false;
    }
    // the content length is the size of the JPEG and known in advance (Home Assistant sends it)
    auto capacity = static_cast<size_t>(http.getSize());
    if (capacity > kMaxImageSize) {
        __LDBG_printf("hass> image is too large (%u bytes, the maximum is %u)", static_cast<unsigned>(capacity), static_cast<unsigned>(kMaxImageSize));
        return false;
    }
    if (capacity == 0 || capacity == static_cast<size_t>(-1)) {
        capacity = kMaxImageSize;
    }
    auto buffer = static_cast<uint8_t *>(ps_malloc(capacity));
    if (!buffer) {
        __LDBG_printf("hass> out of memory (%u bytes for the image)", static_cast<unsigned>(capacity));
        return false;
    }
    const auto deadline = millis() + timeout;
    size_t received = 0;
    bool complete = false;
    while (!_stop) {
        const auto available = stream->available();
        if (available > 0) {
            if (received >= capacity) {
                break; // longer than the announced length, the JPEG is complete
            }
            auto size = static_cast<size_t>(available);
            if (size > capacity - received) {
                size = capacity - received;
            }
            const auto bytes = stream->readBytes(buffer + received, size);
            if (!bytes) {
                break;
            }
            received += bytes;
            continue;
        }
        if (!http.connected()) {
            complete = true;
            break;
        }
        if (static_cast<int32_t>(millis() - deadline) >= 0) {
            break;
        }
        delay(1);
    }
    if (received < 128) {
        __LDBG_printf("hass> image is too short (%u bytes)", static_cast<unsigned>(received));
        free(buffer);
        return false;
    }
    if (!complete && received < capacity) {
        __LDBG_printf("hass> image timed out after %ums (%u of %u bytes)", static_cast<unsigned>(timeout), static_cast<unsigned>(received), static_cast<unsigned>(capacity));
        free(buffer);
        return false;
    }
    data = buffer;
    length = received;
    return true;
}

void Client::_setError(const String &error)
{
    MUTEX_LOCK_BLOCK(_lock) {
        _error = error;
    }
    if (error.length()) {
        __LDBG_printf("%s", error.c_str());
    }
}

// ------------------------------------------------------------------------------------------
// request body and status
// ------------------------------------------------------------------------------------------
void Client::takeStatus(int16_t &statusCode, String &error, uint32_t &duration, uint32_t &responseTime)
{
    MUTEX_LOCK_BLOCK(_lock) {
        statusCode = _statusCode;
        error = _error;
        duration = _duration;
        responseTime = _responseTime;
    }
}

bool Client::takeResponse(String &response, PageIndex &page)
{
    bool available = false;
    MUTEX_LOCK_BLOCK(_lock) {
        if (_responseValid) {
            response = _response;
            page = _responsePage;
            _response = String();
            _responseValid = false;
            _responsePage = kNoPage;
            available = true;
        }
    }
    return available;
}

// The template the connection subscribes: one entry per tile of the page that is visible and the
// attributes of the entity of the open panel (if there is one). The result is the escaped JSON
// string of the template, which is what `render_template` expects
//
// Only the tiles of the page that is visible are subscribed: the template grows with the number of
// tiles, and a page that is not shown does not have to be up to date (it is subscribed the moment
// it is opened, see setVisiblePage()).
void Client::_buildTemplate()
{
    _template = String();
    _templatePage = kNoPage;
    if (!_config) {
        return;
    }
    _templatePage = _visiblePage;
    String body;
    // The template is one JSON entry per value of every tile of the page (an entry is between 60
    // and 240 bytes, a dimmer has nine of them). The buffer is estimated from the number of tiles:
    // the String would otherwise reallocate on every append while the template is built
    size_t tiles = 0;
    for (TileIndex i = 0; i < _config->getTileCount(); i++) {
        const auto &tile = _config->getTile(i);
        if (tile.entity[0] && tile.page == _visiblePage) {
            tiles++;
        }
    }
    body.reserve(64 + tiles * 384);
    body += '{';
    bool first = true;
    for (TileIndex i = 0; i < _config->getTileCount(); i++) {
        const auto &tile = _config->getTile(i);
        if (!tile.entity[0]) {
            // a spacer or an area has no entity, it is not subscribed
            continue;
        }
        if (tile.page != _visiblePage) {
            // the tiles of the other pages are subscribed when they are shown
            continue;
        }
        _appendJsonEntry(body, first, tile.entity, "state", "{{ states('", "') | tojson }}");
        switch (tile.type) {
        case TileType::SENSOR:
            _appendJsonEntry(body, first, tile.entity, "value", "{{ states('", "') | float(0) }}");
            _appendJsonEntry(body, first, tile.entity, "unit", "{{ state_attr('", "','unit_of_measurement') | tojson }}");
            _appendJsonEntry(body, first, tile.entity, "class", "{{ state_attr('", "','device_class') | tojson }}");
            break;
        case TileType::DIMMER:
            // the capabilities of the entity are part of the subscription of the page: the panel
            // of a tile has to be complete the moment it is opened
            _appendJsonEntry(body, first, tile.entity, "brightness", "{{ state_attr('", "','brightness') | int(0) }}");
            _appendCapabilityEntries(body, first, tile.entity);
            break;
        case TileType::CLIMATE:
            _appendJsonEntry(body, first, tile.entity, "temperature", "{{ state_attr('", "','temperature') | float(0) }}");
            _appendJsonEntry(body, first, tile.entity, "current", "{{ state_attr('", "','current_temperature') | float(0) }}");
            _appendJsonEntry(body, first, tile.entity, "action", "{{ state_attr('", "','hvac_action') | tojson }}");
            _appendJsonEntry(body, first, tile.entity, "min_temp", "{{ state_attr('", "','min_temp') | float(0) }}");
            _appendJsonEntry(body, first, tile.entity, "max_temp", "{{ state_attr('", "','max_temp') | float(0) }}");
            _appendCapabilityEntries(body, first, tile.entity);
            break;
        default:
            break;
        }
    }
    // the attributes the panel of the open tile shows (a light, a dimmer or a climate)
    const auto detailTile = _detailTile;
    if (detailTile != kNoDetailTile && detailTile < _config->getTileCount()) {
        const auto &tile = _config->getTile(detailTile);
        if (tile.entity[0]) {
            _appendDetailEntries(body, first, tile.entity);
        }
    }
    body += '}';

    // the template is the value of a JSON member, its quotes and backslashes are escaped. The
    // connection wraps it into the `render_template` command
    _template.reserve(body.length() + 16);
    for (size_t i = 0; i < body.length(); i++) {
        const char chr = body.charAt(i);
        if (chr == '"' || chr == '\\') {
            _template += '\\';
        }
        _template += chr;
    }
}

void Client::_entityDomain(const char *entity, char *output, size_t outputSize)
{
    size_t index = 0;
    while (entity[index] && entity[index] != '.' && index + 1 < outputSize) {
        output[index] = entity[index];
        index++;
    }
    output[index] = 0;
}

// Domain, service and service data of an action. The connection sends them as one `call_service`
// command (the service data carries the entity id, which Home Assistant accepts like a target)
void Client::_buildAction(const Action &action, String &domain, String &service, String &data) const
{
    domain.clear();
    service.clear();
    data.clear();
    if (!_config || action.tile >= _config->getTileCount()) {
        return;
    }
    const auto &tile = _config->getTile(action.tile);
    if (!tile.entity[0]) {
        // spacer and area tiles have no entity
        return;
    }
    char domainName[16];
    _entityDomain(tile.entity, domainName, sizeof(domainName));
    domain = domainName;

    switch (action.type) {
    case Action::Type::TOGGLE:
        domain = F("homeassistant");
        service = F("toggle");
        data = "{\"entity_id\":\"";
        data += tile.entity;
        data += "\"}";
        break;
    case Action::Type::PRESS:
        service = F("press");
        data = "{\"entity_id\":\"";
        data += tile.entity;
        data += "\"}";
        break;
    case Action::Type::SET_LEVEL:
        domain = F("light");
        service = F("turn_on");
        StrWrapper(data).printf("{\"entity_id\":\"%s\",\"brightness_pct\":%u}", tile.entity, static_cast<unsigned>(lroundf(action.value)));
        break;
    case Action::Type::SET_TEMP:
        domain = F("climate");
        service = F("set_temperature");
        StrWrapper(data).printf("{\"entity_id\":\"%s\",\"temperature\":%.1f}", tile.entity, action.value);
        break;
    case Action::Type::SET_HVAC_MODE:
        domain = F("climate");
        service = F("set_hvac_mode");
        data = "{\"entity_id\":\"";
        data += tile.entity;
        data += "\",\"hvac_mode\":\"";
        data += action.text;
        data += "\"}";
        break;
    case Action::Type::SET_PRESET:
        domain = F("climate");
        service = F("set_preset_mode");
        data = "{\"entity_id\":\"";
        data += tile.entity;
        data += "\",\"preset_mode\":\"";
        data += action.text;
        data += "\"}";
        break;
    case Action::Type::SET_FAN_MODE:
        domain = F("climate");
        service = F("set_fan_mode");
        data = "{\"entity_id\":\"";
        data += tile.entity;
        data += "\",\"fan_mode\":\"";
        data += action.text;
        data += "\"}";
        break;
    case Action::Type::SET_EFFECT:
        domain = F("light");
        service = F("turn_on");
        data = "{\"entity_id\":\"";
        data += tile.entity;
        data += "\",\"effect\":\"";
        data += action.text;
        data += "\"}";
        break;
    case Action::Type::SET_COLOR:
        // hs_color instead of color_temp: it works for every color mode that has a hue and the
        // value stays in the level slider
        domain = F("light");
        service = F("turn_on");
        StrWrapper(data).printf("{\"entity_id\":\"%s\",\"hs_color\":[%.1f,%.1f]}", tile.entity, action.value, action.value2);
        break;
    case Action::Type::SET_COLOR_TEMP:
        // kelvin: mireds are the old unit, the current one is color_temp_kelvin
        domain = F("light");
        service = F("turn_on");
        StrWrapper(data).printf("{\"entity_id\":\"%s\",\"color_temp_kelvin\":%d}", tile.entity, static_cast<int>(lroundf(action.value)));
        break;
    default:
        domain.clear();
        service.clear();
        data.clear();
        break;
    }
}

} // namespace HomeAssistant
} // namespace WeatherStation2
