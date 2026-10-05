/**
 * Author: sascha_lammers@gmx.de
 */

#include "hass_dashboard.h"

#include <PrintHtmlEntities.h>
#include <StrView.h>
#include <utility>

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
// response helpers
//
// The template returns one flat key per value ("<entity>.state", "<entity>.value", ...), so the
// values are found with the same strstr/strtof technique the plugin uses for MQTT payloads.
// ------------------------------------------------------------------------------------------
namespace {

// notice of the status line while the configuration file is missing. The buffer of
// getScreenStatus() is sized from it, so the text exists once instead of two copies of it
constexpr const char kMissingFileStatus[] = "Home Assistant: /hass.yaml is required - upload the configuration "
                                            "file with the WebUI (File Manager, target /)";

// value of "<entity>.<key>", nullptr when the key is not part of the response
const char *_valueOf(const char *payload, const char *entity, const char *key)
{
    // The member ("<entity>.<key>":) is matched in place: neither the entity nor the key has a
    // length limit and no needle buffer is needed. `remaining` keeps every check inside the
    // payload, a member that does not fit into the rest of it cannot exist
    const auto entityLength = strlen(entity);
    const auto keyLength = strlen(key);
    const auto end = payload + strlen(payload);
    for (auto ptr = strchr(payload, '"'); ptr; ptr = strchr(ptr + 1, '"')) {
        const auto remaining = static_cast<size_t>(end - ptr);
        if (remaining < (entityLength + keyLength + 4)) {
            break;
        }
        if (strncmp(ptr + 1, entity, entityLength) || ptr[entityLength + 1] != '.') {
            continue;
        }
        const auto name = ptr + entityLength + 2;
        if (strncmp(name, key, keyLength) || name[keyLength] != '"' || name[keyLength + 1] != ':') {
            continue;
        }
        auto value = name + keyLength + 2;
        while (*value == ' ' || *value == ':') {
            value++;
        }
        return value;
    }
    return nullptr;
}

// four hex digits of a \uXXXX escape, 0 when they are not hex digits
uint32_t _hex4(const char *text)
{
    uint32_t value = 0;
    for (uint8_t i = 0; i < 4; i++) {
        const auto chr = text[i];
        uint32_t digit;
        if (chr >= '0' && chr <= '9') {
            digit = static_cast<uint32_t>(chr - '0');
        }
        else if (chr >= 'a' && chr <= 'f') {
            digit = static_cast<uint32_t>(chr - 'a' + 10);
        }
        else if (chr >= 'A' && chr <= 'F') {
            digit = static_cast<uint32_t>(chr - 'A' + 10);
        }
        else {
            return 0;
        }
        value = (value << 4) | digit;
    }
    return value;
}

// one character into the buffer of the parser, always keeps room for the terminator. `write` is the
// cursor of the buffer (`end` is its end) and is advanced past what was written. `cut` is set when
// the buffer is full: the caller decides whether a shortened value is acceptable
void _appendChar(char *&write, const char *end, char chr, bool &cut)
{
    if (write + 1 < end) {
        *write++ = chr;
    }
    else {
        cut = true;
    }
}

// code point of a \uXXXX escape as UTF-8 (the unit of a temperature sensor arrives as \u00B0C,
// see _decodeStringValue())
void _appendCodepoint(char *&write, const char *end, uint32_t codepoint, bool &cut)
{
    if (codepoint < 0x80) {
        _appendChar(write, end, static_cast<char>(codepoint), cut);
    }
    else if (codepoint < 0x800) {
        _appendChar(write, end, static_cast<char>(0xc0 | (codepoint >> 6)), cut);
        _appendChar(write, end, static_cast<char>(0x80 | (codepoint & 0x3f)), cut);
    }
    else if (codepoint < 0x10000) {
        _appendChar(write, end, static_cast<char>(0xe0 | (codepoint >> 12)), cut);
        _appendChar(write, end, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)), cut);
        _appendChar(write, end, static_cast<char>(0x80 | (codepoint & 0x3f)), cut);
    }
    else {
        _appendChar(write, end, static_cast<char>(0xf0 | (codepoint >> 18)), cut);
        _appendChar(write, end, static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)), cut);
        _appendChar(write, end, static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)), cut);
        _appendChar(write, end, static_cast<char>(0x80 | (codepoint & 0x3f)), cut);
    }
}

// The value of `key` is decoded into the buffer at `write`, NUL terminated, and `write` is advanced
// past it. The function allocates nothing and uses no stack buffer: the escapes are decoded straight
// into the buffer. Returns the start of the value, nullptr when the key is missing (or null) and for
// an empty literal. `truncated` (optional) reports that the buffer was too small for the whole value
const char *_decodeStringValue(const char *payload, const char *entity, const char *key, char *&write, const char *end, bool *truncated = nullptr)
{
    auto value = _valueOf(payload, entity, key);
    if (!value || !strncmp(value, "null", 4)) {
        return nullptr;
    }
    char *const start = write;
    bool cut = false;
    if (*value != '"') {
        // a number or a literal (true/false), copy it as it is
        while (*value && *value != ',' && *value != '}') {
            _appendChar(write, end, *value++, cut);
        }
        *write++ = 0;
        if (truncated) {
            *truncated = cut;
        }
        // an empty literal is no value (the key was directly followed by a comma or a brace)
        return (write > start + 1) ? start : nullptr;
    }
    // The escapes are decoded (`tojson` of Home Assistant writes every non ASCII character as
    // \uXXXX, a temperature unit arrives as "\u00B0C"): a string that is copied as it is shows the
    // escape instead of the character
    value++;
    while (*value && *value != '"') {
        if (*value != '\\' || !value[1]) {
            _appendChar(write, end, *value++, cut);
            continue;
        }
        const auto escaped = value[1];
        value += 2;
        switch (escaped) {
        case 'n':
            _appendChar(write, end, '\n', cut);
            break;
        case 'r':
            _appendChar(write, end, '\r', cut);
            break;
        case 't':
            _appendChar(write, end, '\t', cut);
            break;
        case 'b':
            _appendChar(write, end, '\b', cut);
            break;
        case 'f':
            _appendChar(write, end, '\f', cut);
            break;
        case 'u':
            {
                auto codepoint = _hex4(value);
                if (!codepoint) {
                    _appendChar(write, end, 'u', cut);
                    break;
                }
                value += 4;
                // a character above the BMP is written as a surrogate pair
                if (codepoint >= 0xd800 && codepoint <= 0xdbff && value[0] == '\\' && value[1] == 'u') {
                    const auto low = _hex4(value + 2);
                    if (low >= 0xdc00 && low <= 0xdfff) {
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                        value += 6;
                    }
                }
                _appendCodepoint(write, end, codepoint, cut);
            }
            break;
        default:
            // \" \\ \/ and an escape the firmware does not know: the character stands for itself
            _appendChar(write, end, static_cast<char>(escaped), cut);
            break;
        }
    }
    // the buffer always keeps room for the terminator (see _appendChar)
    *write++ = 0;
    if (truncated) {
        *truncated = cut;
    }
    return start;
}

// value of a panel attribute: one that does not fit into the buffer is dropped instead of being
// shortened - a shortened effect name would be sent to the entity and ignored
const char *_detailValue(const char *payload, const char *entity, const char *key, char *&write, const char *end)
{
    bool truncated = false;
    const auto value = _decodeStringValue(payload, entity, key, write, end, &truncated);
    return (value && !truncated) ? value : nullptr;
}

// value of an attribute the response did not carry (never nullptr, see Detail::kUnset)
const char *_orUnset(const char *value)
{
    return value ? value : Detail::kUnset;
}

// copies a value of the previous response into the buffer of the current one (the carry over of an
// attribute the entity stopped reporting). The source is the other slot and must not be pointed at:
// the next response reuses its buffer
const char *_copyValue(const char *value, char *&write, const char *end)
{
    if (!value || !*value) {
        return nullptr;
    }
    char *const start = write;
    bool cut = false;
    while (*value) {
        _appendChar(write, end, *value++, cut);
    }
    *write++ = 0;
    // a carried over value that does not fit is dropped as well
    return cut ? nullptr : start;
}

// value of `key` into a fixed size buffer (the tile model of a state response), false when the key
// is missing, the value is null or an empty literal
bool _parseStringValue(const char *payload, const char *entity, const char *key, char *output, size_t outputSize)
{
    if (outputSize == 0) {
        return false;
    }
    output[0] = 0;
    char *write = output;
    return _decodeStringValue(payload, entity, key, write, output + outputSize) != nullptr;
}

// number of a value, false when the key is missing, null or a string
bool _parseNumberValue(const char *payload, const char *entity, const char *key, float &number)
{
    const auto value = _valueOf(payload, entity, key);
    if (!value || *value == '"' || !strncmp(value, "null", 4)) {
        return false;
    }
    char *stop = nullptr;
    const auto parsed = strtof(value, &stop);
    if (!stop || stop == value) {
        return false;
    }
    number = parsed;
    return true;
}

// buffer of a detail slot. It is allocated in the PSRAM (the internal DRAM is the scarce resource of
// this board, see _ws2PsramObject()) and released with free(), which works for both heaps
char *_allocateDetailBuffer(size_t size)
{
#if ESP32
    auto ptr = static_cast<char *>(ps_malloc(size));
    if (ptr) {
        return ptr;
    }
#endif
    return static_cast<char *>(malloc(size));
}

} // namespace

// ------------------------------------------------------------------------------------------
// Detail
// ------------------------------------------------------------------------------------------
Detail::~Detail()
{
    reset();
}

void Detail::reset()
{
    if (buffer) {
        free(buffer);
        buffer = nullptr;
    }
    capacity = 0;
    clear();
}

void Detail::clear()
{
    valid = false;
    tile = kNoTile;
    generation = 0;
    mode = kUnset;
    preset = kUnset;
    fanMode = kUnset;
    effect = kUnset;
    colorMode = kUnset;
    colorModes = kUnset;
    modeList = kUnset;
    presetList = kUnset;
    fanModeList = kUnset;
    effectList = kUnset;
    hue = 0;
    saturation = 0;
    level = 0;
    colorTemp = 0;
    minColorTemp = 0;
    maxColorTemp = 0;
    used = 0;
}

void Detail::swap(Detail &other)
{
    std::swap(valid, other.valid);
    std::swap(tile, other.tile);
    std::swap(generation, other.generation);
    std::swap(mode, other.mode);
    std::swap(preset, other.preset);
    std::swap(fanMode, other.fanMode);
    std::swap(effect, other.effect);
    std::swap(colorMode, other.colorMode);
    std::swap(colorModes, other.colorModes);
    std::swap(modeList, other.modeList);
    std::swap(presetList, other.presetList);
    std::swap(fanModeList, other.fanModeList);
    std::swap(effectList, other.effectList);
    std::swap(hue, other.hue);
    std::swap(saturation, other.saturation);
    std::swap(level, other.level);
    std::swap(colorTemp, other.colorTemp);
    std::swap(minColorTemp, other.minColorTemp);
    std::swap(maxColorTemp, other.maxColorTemp);
    std::swap(buffer, other.buffer);
    std::swap(used, other.used);
    std::swap(capacity, other.capacity);
}

bool Detail::reserve(size_t length)
{
    // every value is written behind the previous one and NUL terminated, so the payload plus a
    // terminator per value is the upper bound
    const auto required = length + 32;
    if (required <= capacity) {
        return true;
    }
    const auto size = (required > kMaxBuffer) ? kMaxBuffer : required;
    const auto ptr = _allocateDetailBuffer(size);
    if (!ptr) {
        return false;
    }
    // the buffer is empty here (clear() ran before), nothing has to be copied
    if (buffer) {
        free(buffer);
    }
    buffer = ptr;
    capacity = size;
    return true;
}

void Detail::shrink()
{
    if (!buffer) {
        return;
    }
    if (!used) {
        // no value of the response was stored
        free(buffer);
        buffer = nullptr;
        capacity = 0;
        return;
    }
    // The tail is only given back when it is worth another allocation: the buffer is sized from the
    // payload of a whole page while the panel keeps a handful of values of its own entity
    if (used + 32 >= capacity) {
        return;
    }
    const auto ptr = _allocateDetailBuffer(used);
    if (!ptr) {
        return; // keeping the larger buffer is fine
    }
    memcpy(ptr, buffer, used);
    free(buffer);
    buffer = ptr;
    capacity = used;
}

// ------------------------------------------------------------------------------------------
// lifecycle
// ------------------------------------------------------------------------------------------
void Dashboard::begin()
{
    _reload();
}

void Dashboard::stop()
{
    _client.stop();
}

void Dashboard::reconfigure()
{
    _reload();
}

void Dashboard::setActive(bool active)
{
    if (_active == active) {
        return;
    }
    _active = active;
    _client.setActive(active);
    if (active) {
        // the screen was opened: subscribe the values of the visible page and check the file right
        // away
        _lastConfigCheck = 0;
        _client.requestRefresh();
    }
    else {
        // the panels are closed with the screen, and the picture tiles of the page that is left
        // are not fetched any more
        closeDetail();
        _client.clearImages();
    }
}

void Dashboard::setVisiblePage(PageIndex page)
{
    if (_visiblePage == page) {
        return;
    }
    __LDBG_printf("hass> page %u is visible", static_cast<unsigned>(page));
    _visiblePage = page;
    // The client only polls the tiles of the page that is visible. The values of the page that is
    // opened now are requested right away, the tiles of the page that is left keep the values
    // they have (a response of the page that is left is not applied any more, see
    // _applyResponse())
    _client.setVisiblePage(page);
    if (_active) {
        _client.requestRefresh();
    }
    // the requests of the page that is left are dropped. A frame that was decoded already stays
    // in the client until the screen collects it (and releases the buffer)
    _client.clearImages();
}

void Dashboard::setPictureBox(TileIndex index, uint16_t width, uint16_t height)
{
    if (index >= _pictureBox.size()) {
        return;
    }
    _pictureBox[index].width = width;
    _pictureBox[index].height = height;
}

bool Dashboard::takeImage(TileIndex &tile, uint16_t *&data, uint16_t &width, uint16_t &height, uint32_t &stamp)
{
    return _client.takeImage(tile, data, width, height, stamp);
}

// The camera image of a picture tile is a request of its own (a JPEG of 40-80 KB that has to be
// decoded), it is only fetched while the tile is visible. The client keeps the list, so a tile
// that is registered again (the dashboard calls this on every update) does not restart its
// refresh interval
void Dashboard::_updateImages()
{
    if (!_active || !_config.isLoaded()) {
        return;
    }
    TileIndex count = 0;
    for (TileIndex i = 0; i < _config.getTileCount(); i++) {
        const auto &tile = _config.getTile(i);
        const auto &box = _pictureBox[i];
        if (tile.page != _visiblePage || !box.width || !box.height) {
            continue;
        }
        _client.requestImage(i, box.width, box.height, tile.refresh);
        count++;
    }
    if (!count) {
        // the page has no picture tile (or the screen did not build them yet)
        _client.clearImages();
    }
}

void Dashboard::_reload()
{
    _client.stop();
    // the tile indices of the new configuration do not have to match the old ones
    _detailTile = kNoTile;
    _detail.reset();
    _detailBefore.reset();
    // the graph of the sensor panel belongs to a tile of the previous version
    _statsTile = kStatsNone;
    _statsHours = kStatsDefaultHours;
    _statsCount = 0;
    _statsStart = 0;
    _statsEnd = 0;
    _statsError = String();
    _statsPending = false;
    _configError = String();
    _requestError = String();
    _fileMissing = false;

    if (!_config.load()) {
        _fileMissing = _config.isFileMissing();
        _configError = _config.getError();
        return;
    }
    // a new version of the file, the screen has to rebuild its widget tree
    _configGeneration++;

    // the model starts empty, the first response fills it. The flags and the picture boxes are
    // sized to the tiles of the new version of the file
    const auto count = _config.getTileCount();
    _flags.assign(count, TileFlags());
    _pictureBox.assign(count, PictureBox());
    for (TileIndex i = 0; i < count; i++) {
        const auto &tile = _config.getTile(i);
        if (tile.unit[0]) {
            strncpy(_value(i).unit, tile.unit, sizeof(_value(i).unit) - 1);
        }
    }
    _response = String();
    _responseTime = 0;
    _statusCode = 0;
    // the request of the new client is built from the tiles of the page that is visible
    _client.setVisiblePage(_visiblePage);
    _client.begin(_config);
    _client.setActive(_active);
    if (_active) {
        _client.requestRefresh();
    }
}

void Dashboard::update()
{
    const auto now = millis();
    if (static_cast<uint32_t>(now - _lastUpdate) < kMinUpdateInterval) {
        return;
    }
    _lastUpdate = now;

    // values of the last request
    PageIndex page = Client::kNoPage;
    if (_client.takeResponse(_response, page)) {
        _applyResponse(_response.c_str(), page);
    }
    // attributes of the open panel
    if (_detailTile != kNoTile) {
        String detail;
        if (_client.takeDetailResponse(detail)) {
            _applyDetail(detail.c_str(), detail.length());
        }
    }
    _client.takeStatus(_statusCode, _requestError, _duration, _responseTime);
    _requestCount = _client.getRequestCount();

    // The history graph of the open sensor panel. The request is repeated while the panel is open:
    // the buckets of the recorder are 5 minute aggregates, so the minute keeps the window on the
    // clock and a new bucket shows up on screen within a minute of it being written
    if (_statsTile != kStatsNone) {
        if (static_cast<int32_t>(now - _statsNextFetch) >= 0) {
            _statsNextFetch = now + kStatsRefreshInterval;
            _statsPending = true;
            _client.requestStats(_statsTile, _statsHours);
        }
        // a new response replaces the buckets of the graph. One that belongs to a tile that is not
        // open any more (the panel was closed or another one was opened while the request was on
        // its way) is dropped
        uint32_t generation = _statsGeneration;
        TileIndex statsTile = Client::kNoStatsTile;
        uint8_t statsHours = 0;
        uint32_t statsStart = 0;
        uint32_t statsEnd = 0;
        uint16_t statsCount = 0;
        String statsError;
        if (_client.takeStats(generation, statsTile, statsHours, statsStart, statsEnd, _statsPoints, statsCount, statsError)) {
            _statsGeneration = generation;
            if (statsTile == _statsTile) {
                _statsHours = statsHours;
                _statsStart = statsStart;
                _statsEnd = statsEnd;
                _statsCount = statsCount;
                _statsError = statsError;
                _statsPending = false;
                __LDBG_printf("hass> %u statistics bucket(s) of tile %u applied%s%s", static_cast<unsigned>(statsCount),
                              static_cast<unsigned>(statsTile), statsError.length() ? ": " : "", statsError.c_str());
            }
        }
    }

    // An action that could not be sent (or that Home Assistant rejected) is given up: the tiles show
    // the state of the entity again (the screen had shown the result of the tap right away)
    TileIndex failedTile = 0;
    while (_client.takeActionFailure(failedTile)) {
        _revertPending(failedTile, "failed");
    }

    // the picture tiles of the visible page
    _updateImages();

    // A new version of the file (uploaded, or the first upload after the boot). Only the size is
    // compared: reading the file to find a change of its bytes costs as much as a state poll every
    // 3 seconds, so an upload has to change the size of the file to be noticed (see getFileInfo())
    if (static_cast<uint32_t>(now - _lastConfigCheck) >= kConfigCheckInterval) {
        _lastConfigCheck = now;
        uint32_t size = 0;
        getFileInfo(kConfigFile, size);
        if (size != _config.getFileSize()) {
            __LDBG_printf("%s changed (%u -> %u bytes), reloading", kConfigFile, static_cast<unsigned>(_config.getFileSize()),
                          static_cast<unsigned>(size));
            _reload();
        }
    }
}

// ------------------------------------------------------------------------------------------
// values
// ------------------------------------------------------------------------------------------
// Applies the rendered template of one subscription. `page` is the page the response was built
// from (the tiles of the page that is visible, see Client::setVisiblePage()).
//
// A tile is updated when its entity is part of the response. An entity that several tiles use is
// applied to all of them, also to the tiles of a page that is not shown right now: the page is up
// to date the moment it is opened again (it is subscribed then, which confirms the values, but the
// tiles would show the state from before the change until that answer arrives). A tile whose entity
// is not part of the response keeps what it has - the entity of a tile of another page is not
// unavailable, it is simply not part of this subscription. Only a tile of the page the response was
// built from is reported as unavailable when Home Assistant left its entity out (the entity was
// removed, or the answer is empty)
void Dashboard::_applyResponse(const char *payload, PageIndex page)
{
    if (!_config.isLoaded() || !payload || !*payload) {
        return;
    }
    const auto started = millis();
    const auto count = _config.getTileCount();
    TileIndex covered = 0;
    TileIndex updated = 0;

    for (TileIndex i = 0; i < count; i++) {
        const auto &tile = _config.getTile(i);
        auto &value = _value(i);
        auto &flags = _flags[i];

        if (!tile.entity[0]) {
            // a spacer or an area tile has no entity, there is nothing to read
            continue;
        }
        const auto ownPage = (tile.page == page);
        if (ownPage) {
            covered++;
        }
        char state[sizeof(value.text)];
        const auto hasState = _parseStringValue(payload, tile.entity, "state", state, sizeof(state));
        // the state of a button is the time of its last press, "unknown" is a button that was never
        // pressed - it is idle, not unavailable
        const auto unknown = hasState && !strcasecmp(state, "unknown") && tile.type != TileType::BUTTON;
        if (!hasState || !state[0] || unknown || !strcasecmp(state, "unavailable")) {
            // The entity is in the response (or it should be, this tile belongs to the page the
            // response was built from) but it does not report a state: the tile is unavailable.
            // A tile of another page is left alone: the response simply does not carry its entity
            if (ownPage || hasState) {
                value.state = TileState::UNAVAILABLE;
            }
            // an entity that stopped reporting cannot confirm an action either, the timeout of
            // _markPending() ends the wait
            if (flags.pending && static_cast<uint32_t>(millis() - flags.pendingSince) >= kPendingTimeout) {
                flags.pending = false;
            }
            continue;
        }
        updated++;
        // the raw state, the screen shows it for entities that do not report a number
        strncpy(value.text, state, sizeof(value.text) - 1);
        value.text[sizeof(value.text) - 1] = 0;

        switch (tile.type) {
        case TileType::SENSOR:
            {
                float number = 0;
                if (_parseNumberValue(payload, tile.entity, "value", number)) {
                    value.value = number;
                }
                if (!tile.unit[0]) {
                    // the unit of the entity, the escapes of the JSON are decoded (a temperature
                    // arrives as "\u00B0C"). The trace prints the text and its UTF-8 bytes, so a
                    // unit that does not render can be told apart from one that is not decoded
                    char previous[sizeof(value.unit)];
                    strncpy(previous, value.unit, sizeof(previous) - 1);
                    previous[sizeof(previous) - 1] = 0;
                    if (_parseStringValue(payload, tile.entity, "unit", value.unit, sizeof(value.unit)) && strcmp(previous, value.unit)) {
                        __LDBG_printf("hass> tile %u '%s': unit '%s' (%u bytes)", static_cast<unsigned>(i), tile.name, value.unit,
                                      static_cast<unsigned>(strlen(value.unit)));
                    }
                }
                value.deviceClass[0] = 0;
                _parseStringValue(payload, tile.entity, "class", value.deviceClass, sizeof(value.deviceClass));
                // a binary sensor (motion, door, ...) reports on/off, the tile follows the state.
                // A number is a plain reading: the entity has no on/off state, so the tile is not
                // drawn as an active one
                if (!strcasecmp(state, "on")) {
                    value.state = TileState::ON;
                }
                else {
                    value.state = TileState::OFF;
                }
            }
            break;

        case TileType::DIMMER:
            {
                value.state = (!strcasecmp(state, "on")) ? TileState::ON : TileState::OFF;
                float brightness = 0;
                if (_parseNumberValue(payload, tile.entity, "brightness", brightness)) {
                    auto percent = (brightness * 100.0f) / 255.0f;
                    if (percent < 0) {
                        percent = 0;
                    }
                    else if (percent > 100) {
                        percent = 100;
                    }
                    value.value = percent;
                }
                _readCapabilities(payload, tile, i);
            }
            break;

        case TileType::CLIMATE:
            {
                if (!strcasecmp(state, "off")) {
                    value.mode = 0;
                }
                else if (!strcasecmp(state, "heat")) {
                    value.mode = 1;
                }
                else if (!strcasecmp(state, "cool")) {
                    value.mode = 2;
                }
                else {
                    value.mode = 0xff;
                }
                value.state = (value.mode == 0) ? TileState::OFF : TileState::ON;
                // the state is the mode of the entity, the panel shows it as it is ("auto", ...)
                strncpy(value.modeName, state, sizeof(value.modeName) - 1);
                // the action is optional, entities that do not have it show the mode instead
                value.action[0] = 0;
                _parseStringValue(payload, tile.entity, "action", value.action, sizeof(value.action));
                float number = 0;
                if (_parseNumberValue(payload, tile.entity, "temperature", number)) {
                    value.value = number;
                }
                if (_parseNumberValue(payload, tile.entity, "current", number)) {
                    value.current = number;
                }
                if (_parseNumberValue(payload, tile.entity, "min_temp", number)) {
                    value.minTemp = number;
                }
                if (_parseNumberValue(payload, tile.entity, "max_temp", number)) {
                    value.maxTemp = number;
                }
            }
            break;

        default:
            value.state = (!strcasecmp(state, "on")) ? TileState::ON : TileState::OFF;
            // a switch or a light: log the transitions, not every response (the poll repeats the
            // same state and a trace per tile and poll buries everything else)
            {
                if (flags.lastState != static_cast<uint8_t>(value.state)) {
                    flags.lastState = static_cast<uint8_t>(value.state);
                    __LDBG_printf("hass> tile %u '%s' %s (%ums after the last action)", static_cast<unsigned>(i), tile.name, state,
                                  static_cast<unsigned>(millis() - _lastAction));
                }
            }
            break;
        }
        // what the entity reports decides whether an action that is on its way is confirmed (the
        // state of a switch, a light, a dimmer or the mode of a climate)
        _reconcilePending(i, value, state);
    }
    // Every action that was queued has to confirm itself: the pushed template arrives when the
    // entity reports the new state (the value the entity had when the action was sent does not
    // confirm it), the timeout ends the wait at the latest
    bool pending = _client.hasPendingAction();
    for (TileIndex i = 0; i < count; i++) {
        auto &value = _value(i);
        auto &flags = _flags[i];
        if (!flags.pending) {
            continue;
        }
        // The optimistic state of a toggle is our own value: only the entity reporting it (or the
        // timeout) ends the wait, see _reconcilePending(). A value the entity reported (a level, a
        // setpoint, a color) confirms an action as well
        const auto stateChanged = (flags.expectedState == TileState::UNKNOWN) && (static_cast<uint8_t>(value.state) != flags.pendingState);
        const auto valueChanged = fabsf(value.value - flags.pendingValue) > 0.05f;
        if (stateChanged || valueChanged) {
            flags.pending = false;
        }
        else if (static_cast<uint32_t>(millis() - flags.pendingSince) >= kPendingTimeout) {
            // the entity never reported the change (or the command was ignored): the action is given
            // up and the tile shows the state of the entity again
            _revertPending(i, "was not confirmed by the entity");
        }
        pending = pending || flags.pending;
    }
    __LDBG_printf("hass> response of page %u: %u tile(s) updated, %u of its %u tile(s) covered, pending=%u, queued=%u (%ums)",
                  static_cast<unsigned>(page), static_cast<unsigned>(updated), static_cast<unsigned>(covered),
                  static_cast<unsigned>(count), static_cast<unsigned>(pending), static_cast<unsigned>(_client.hasPendingAction()),
                  static_cast<unsigned>(millis() - started));
}

// ------------------------------------------------------------------------------------------
// actions
// ------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------
// capabilities of the panel of a tile
// ------------------------------------------------------------------------------------------
// true when the entries of a comma separated JSON string value contain `name`. The caller passes the
// first character behind the opening quote. The list is scanned in place, it is never copied: the
// effect list of a light does not fit into a fixed buffer (169 characters on the LED matrix, 295 on
// a Hue light) and only the presence of a name matters here
static bool _hasListEntry(const char *value, const char *name)
{
    const auto length = strlen(name);
    while (*value && *value != '"') {
        if (!strncasecmp(value, name, length) && (value[length] == ',' || value[length] == '"')) {
            return true;
        }
        // the next entry of the list
        while (*value && *value != ',' && *value != '"') {
            value++;
        }
        if (*value == ',') {
            value++;
        }
    }
    return false;
}

void Dashboard::_readCapabilities(const char *payload, const Tile &tile, TileIndex index)
{
    // every light can be switched on and off, the other controls depend on the entity
    uint8_t caps = kCapPower;
    const auto colorModes = _valueOf(payload, tile.entity, "color_modes");
    if (colorModes && *colorModes == '"') {
        if (_hasListEntry(colorModes + 1, "hs") || _hasListEntry(colorModes + 1, "rgb") ||
            _hasListEntry(colorModes + 1, "rgbw") || _hasListEntry(colorModes + 1, "rgbww") ||
            _hasListEntry(colorModes + 1, "xy")) {
            caps |= kCapColor;
        }
        if (_hasListEntry(colorModes + 1, "color_temp")) {
            caps |= kCapColorTemp;
        }
        // The level is a capability of the entity, not of its state: a light that is switched off
        // reports no brightness attribute (`has_level` = 0) and the brightness button of the panel
        // disappeared while the entity was off. `brightness` and `white` are dimmable and every
        // color mode carries a level as well
        if (_hasListEntry(colorModes + 1, "brightness") || _hasListEntry(colorModes + 1, "white") || (caps & (kCapColor | kCapColorTemp))) {
            caps |= kCapLevel;
        }
    }
    // an entity with a non empty effect list has the effects button (the list itself is stored by
    // _applyDetail(), it does not fit into a fixed buffer)
    const auto effectList = _valueOf(payload, tile.entity, "effect_list");
    if (effectList && *effectList == '"' && effectList[1] && effectList[1] != '"') {
        caps |= kCapEffects;
    }
    float number = 0;
    if (_parseNumberValue(payload, tile.entity, "color_temp", number) && number > 0) {
        caps |= kCapColorTemp;
        caps |= kCapLevel;
    }
    if (_parseNumberValue(payload, tile.entity, "has_level", number) && number > 0) {
        caps |= kCapLevel;
    }
    _flags[index].capabilities = caps;
}

bool Dashboard::_queue(Client::Action::Type type, TileIndex index, float value, const char *text, float value2)
{
    if (!_config.isLoaded() || index >= _config.getTileCount() || !_client.isRunning()) {
        return false;
    }
    Client::Action action;
    action.type = type;
    action.tile = index;
    action.value = value;
    action.value2 = value2;
    if (text) {
        strncpy(action.text, text, sizeof(action.text) - 1);
    }
    if (!_client.queueAction(action)) {
        __LDBG_printf("action queue is full");
        return false;
    }
    // The action is on its way: the value the entity reports confirms it (the template is pushed
    // when it changes, see _applyResponse)
    _lastAction = millis();
    return true;
}

// An action that was queued for a tile: the values the entity had when it was sent are stored, so a
// response can tell whether the entity reported the change (see _applyResponse). The screen shows
// the mark until then.
void Dashboard::_markPending(TileIndex index)
{
    auto &value = _value(index);
    auto &flags = _flags[index];
    flags.pending = true;
    flags.pendingState = static_cast<uint8_t>(value.state);
    flags.pendingValue = value.value;
    flags.pendingSince = millis();
}

// The action of a tile was applied to the tile right away (optimistic, see toggle()): the state the
// entity is expected to report is in expectedState. A response that reports it confirms the action,
// a response that reports the state the entity had when the action was sent does not - the request
// was in flight while the action was applied and the tapped state has to stay on screen. Any other
// state wins over the optimistic one (the entity is the only source of the state).
void Dashboard::_reconcilePending(TileIndex index, TileValue &value, const char *state)
{
    auto &flags = _flags[index];
    if (flags.expectedState == TileState::UNKNOWN) {
        return;
    }
    // "off" is the only state that is not active: a switch/light reports "on"/"off", a climate its
    // mode ("heat", "cool", "auto", ...)
    const auto reported = (!strcasecmp(state, "off")) ? TileState::OFF : TileState::ON;
    if (reported == flags.expectedState) {
        flags.expectedState = TileState::UNKNOWN;
        flags.pending = false;
        __LDBG_printf("hass> tile %u '%s' confirmed the action (state %u)", static_cast<unsigned>(index), _config.getTile(index).name,
                      static_cast<unsigned>(reported));
        return;
    }
    if (static_cast<uint8_t>(reported) == flags.pendingState) {
        // the request was in flight while the action was sent: the tile keeps what the user tapped
        value.state = flags.expectedState;
        return;
    }
    __LDBG_printf("hass> tile %u '%s' reports state %u instead of the tapped %u", static_cast<unsigned>(index),
                  _config.getTile(index).name, static_cast<unsigned>(reported), static_cast<unsigned>(flags.expectedState));
    flags.expectedState = TileState::UNKNOWN;
}

// An action is given up (it failed or the entity never confirmed it): the tile goes back to the
// state it had when the action was sent, the entity is the only source of the state again
void Dashboard::_revertPending(TileIndex index, const char *reason)
{
    auto &value = _value(index);
    auto &flags = _flags[index];
    if (!flags.pending) {
        return;
    }
    const auto optimistic = (flags.expectedState != TileState::UNKNOWN);
    __LDBG_printf("hass> action of tile %u '%s' %s: showing the state of the entity (%u)%s", static_cast<unsigned>(index),
                  _config.getTile(index).name, reason, static_cast<unsigned>(flags.pendingState),
                  optimistic ? ", the tapped state is reverted" : "");
    flags.pending = false;
    if (optimistic) {
        value.state = static_cast<TileState>(flags.pendingState);
        flags.expectedState = TileState::UNKNOWN;
    }
}

void Dashboard::toggle(TileIndex index)
{
    if (index >= _config.getTileCount()) {
        return;
    }
    const auto &tile = _config.getTile(index);
    __LDBG_printf("hass> toggle tile %u '%s' (%s), reported state %u, pending %u", static_cast<unsigned>(index), tile.name, tile.entity,
                  static_cast<unsigned>(_value(index).state), static_cast<unsigned>(_flags[index].pending));
    const auto type = (tile.type == TileType::BUTTON) ? Client::Action::Type::PRESS : Client::Action::Type::TOGGLE;
    auto &value = _value(index);
    auto &flags = _flags[index];
    // The state of the entity before the action is stored first (the revert target), then the tile
    // shows the result of the tap right away - waiting for the response of the service call feels
    // laggy. It is confirmed when the entity reports that state (the fast polls of the client run
    // until then) and reverted when the action fails or the entity never reports it (see
    // _reconcilePending()/_revertPending()).
    const auto tapped = (type == Client::Action::Type::TOGGLE) ? ((value.state == TileState::ON) ? TileState::OFF : TileState::ON)
                                                               : TileState::UNKNOWN;
    if (!_queue(type, index, 0)) {
        __LDBG_printf("hass> toggle tile %u dropped", static_cast<unsigned>(index));
        return;
    }
    _markPending(index);
    if (tapped != TileState::UNKNOWN) {
        flags.expectedState = tapped;
        value.state = tapped;
        __LDBG_printf("hass> tile %u '%s' shows the tapped state %u until the entity reports it", static_cast<unsigned>(index),
                      tile.name, static_cast<unsigned>(tapped));
    }
}

void Dashboard::setLevel(TileIndex index, uint8_t percent)
{
    if (index >= _config.getTileCount() || _config.getTile(index).type != TileType::DIMMER) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_LEVEL, index, percent)) {
        return;
    }
    // Only the mark, never the value: the screen holds the level the finger set (its own expected
    // value) while the entity has not reported it. Writing it here as well confirmed that hold with
    // our own value, so the next state response - the one from the request that was in flight and
    // still carries the old level - took the tile over and the fill jumped to the old level and back
    _markPending(index);
}

void Dashboard::setTemperature(TileIndex index, float temperature)
{
    if (index >= _config.getTileCount() || _config.getTile(index).type != TileType::CLIMATE) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_TEMP, index, temperature)) {
        return;
    }
    // same as setLevel(): the setpoint the user stepped is held by the screen, the response is the
    // only writer of the value
    _markPending(index);
}

void Dashboard::setMode(TileIndex index, const char *mode)
{
    if (index >= _config.getTileCount() || _config.getTile(index).type != TileType::CLIMATE || !mode || !*mode) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_HVAC_MODE, index, 0, mode)) {
        return;
    }
    _markPending(index);
    // `off` switches the entity off, every other mode turns it on: the tile shows that right away
    // (the mode the entity took is confirmed by the response, see _reconcilePending())
    auto &value = _value(index);
    auto &flags = _flags[index];
    flags.expectedState = strcasecmp(mode, "off") ? TileState::ON : TileState::OFF;
    value.state = flags.expectedState;
}

void Dashboard::setPreset(TileIndex index, const char *preset)
{
    if (index >= _config.getTileCount() || _config.getTile(index).type != TileType::CLIMATE || !preset || !*preset) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_PRESET, index, 0, preset)) {
        return;
    }
    _markPending(index);
}

void Dashboard::setFanMode(TileIndex index, const char *fanMode)
{
    if (index >= _config.getTileCount() || _config.getTile(index).type != TileType::CLIMATE || !fanMode || !*fanMode) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_FAN_MODE, index, 0, fanMode)) {
        return;
    }
    _markPending(index);
}

void Dashboard::setEffect(TileIndex index, const char *effect)
{
    if (index >= _config.getTileCount() || !effect) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_EFFECT, index, 0, effect)) {
        return;
    }
    _markPending(index);
}

void Dashboard::setColor(TileIndex index, float hue, float saturation)
{
    if (index >= _config.getTileCount() || _config.getTile(index).type != TileType::DIMMER) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_COLOR, index, hue, nullptr, saturation)) {
        return;
    }
    _markPending(index);
}

void Dashboard::setColorTemp(TileIndex index, float kelvin)
{
    if (index >= _config.getTileCount() || _config.getTile(index).type != TileType::DIMMER || kelvin <= 0) {
        return;
    }
    if (!_queue(Client::Action::Type::SET_COLOR_TEMP, index, kelvin)) {
        return;
    }
    _markPending(index);
}

// ------------------------------------------------------------------------------------------
// detail attributes of an open panel
// ------------------------------------------------------------------------------------------
void Dashboard::requestDetail(TileIndex index)
{
    if (index >= _config.getTileCount()) {
        return;
    }
    if (_detailTile == index) {
        return;
    }
    // keep the generation so that the screen knows when the new values arrived
    const auto generation = _detail.generation;
    _detail.reset();
    _detail.generation = generation;
    _detail.tile = index;
    _detailTile = index;
    _client.requestDetail(index);
}

void Dashboard::closeDetail()
{
    if (_detailTile == kNoTile) {
        return;
    }
    _detailTile = kNoTile;
    _client.closeDetail();
    const auto generation = _detail.generation;
    _detail.reset();
    _detail.generation = generation;
}

// ------------------------------------------------------------------------------------------
// history graph of the sensor panel
// ------------------------------------------------------------------------------------------
void Dashboard::requestStats(TileIndex index, uint8_t hours)
{
    if (!_config.isLoaded() || index >= _config.getTileCount()) {
        return;
    }
    if (hours < Client::kMinStatsHours || hours > Client::kMaxStatsHours) {
        hours = kStatsDefaultHours;
    }
    if (_statsTile != index || _statsHours != hours) {
        // the range or the entity changed: the buckets of the previous window are not what the
        // graph shows any more
        _statsCount = 0;
        _statsStart = 0;
        _statsEnd = 0;
        _statsError = String();
    }
    _statsTile = index;
    _statsHours = hours;
    _statsPending = true;
    // the request is on its way, the next refresh follows one interval later
    _statsNextFetch = millis() + kStatsRefreshInterval;
    _client.requestStats(index, hours);
}

void Dashboard::closeStats()
{
    if (_statsTile == kStatsNone) {
        return;
    }
    __LDBG_printf("hass> statistics of tile %u closed", static_cast<unsigned>(_statsTile));
    _statsTile = kStatsNone;
    _statsHours = kStatsDefaultHours;
    _statsCount = 0;
    _statsStart = 0;
    _statsEnd = 0;
    _statsError = String();
    _statsPending = false;
}

void Dashboard::_applyDetail(const char *payload, size_t length)
{
    if (_detailTile == kNoTile || _detailTile >= _config.getTileCount()) {
        return;
    }
    const auto &tile = _config.getTile(_detailTile);
    const auto generation = _detail.generation + 1;
    // A switched off light reports neither the effects nor the color, some integrations drop the
    // whole attribute. The last known values are carried over from the previous response then, so
    // the panel keeps its effects and the color of the entity instead of falling back to "no
    // effects, white". The two slots only exchange the pointers and the buffer: nothing is
    // allocated or copied to keep the previous response
    _detailBefore.swap(_detail);
    const bool carryOver = (_detailBefore.tile == _detailTile);
    _detail.clear();
    _detail.tile = _detailTile;
    _detail.generation = generation;

    // One buffer for every value of the response, sized from the payload and capped by
    // Detail::kMaxBuffer - it is given back to what the values really need when the parse is done
    if (!_detail.reserve(length)) {
        __DBG_printf_E("hass> cannot allocate %u bytes for the panel of tile %u", static_cast<unsigned>(length), static_cast<unsigned>(_detailTile));
        return;
    }
    char *write = _detail.buffer;
    const char *const end = _detail.buffer + _detail.capacity;

    // the values are decoded straight into the buffer, the parser neither allocates nor uses a
    // stack buffer - it gets the write cursor of the buffer
    _detail.mode = _orUnset(_detailValue(payload, tile.entity, "state", write, end));
    _detail.preset = _orUnset(_detailValue(payload, tile.entity, "preset_mode", write, end));
    _detail.fanMode = _orUnset(_detailValue(payload, tile.entity, "fan_mode", write, end));
    _detail.effect = _orUnset(_detailValue(payload, tile.entity, "effect", write, end));
    _detail.colorMode = _orUnset(_detailValue(payload, tile.entity, "color_mode", write, end));
    _detail.colorModes = _orUnset(_detailValue(payload, tile.entity, "color_modes", write, end));
    _detail.modeList = _orUnset(_detailValue(payload, tile.entity, "hvac_modes", write, end));
    _detail.presetList = _orUnset(_detailValue(payload, tile.entity, "preset_modes", write, end));
    _detail.fanModeList = _orUnset(_detailValue(payload, tile.entity, "fan_modes", write, end));
    _detail.effectList = _orUnset(_detailValue(payload, tile.entity, "effect_list", write, end));
    float number = 0;
    if (_parseNumberValue(payload, tile.entity, "brightness", number)) {
        auto percent = (number * 100.0f) / 255.0f;
        _detail.level = static_cast<uint8_t>((percent < 0) ? 0 : ((percent > 100) ? 100 : percent));
    }
    // the two numbers of "30.0,100.0" are parsed in place, they do not need the buffer
    const auto hsColor = _valueOf(payload, tile.entity, "hs_color");
    if (hsColor && *hsColor == '"') {
        char *stop = nullptr;
        _detail.hue = strtof(hsColor + 1, &stop);
        if (stop && *stop == ',') {
            _detail.saturation = strtof(stop + 1, nullptr);
        }
    }
    if (_parseNumberValue(payload, tile.entity, "color_temp", number)) {
        _detail.colorTemp = number;
    }
    if (_parseNumberValue(payload, tile.entity, "min_color_temp", number)) {
        _detail.minColorTemp = number;
    }
    if (_parseNumberValue(payload, tile.entity, "max_color_temp", number)) {
        _detail.maxColorTemp = number;
    }
    // carry the values of the last response over when the entity stopped reporting them (it is off)
    if (carryOver) {
        if (!_detail.effect[0]) {
            _detail.effect = _orUnset(_copyValue(_detailBefore.effect, write, end));
        }
        if (!_detail.colorModes[0]) {
            _detail.colorModes = _orUnset(_copyValue(_detailBefore.colorModes, write, end));
        }
        if (!_detail.effectList[0]) {
            _detail.effectList = _orUnset(_copyValue(_detailBefore.effectList, write, end));
        }
        if (_detail.colorTemp <= 0) {
            _detail.colorTemp = _detailBefore.colorTemp;
        }
        if (_detail.minColorTemp <= 0) {
            _detail.minColorTemp = _detailBefore.minColorTemp;
            _detail.maxColorTemp = _detailBefore.maxColorTemp;
        }
        if (_detail.hue <= 0 && _detail.saturation <= 0) {
            _detail.hue = _detailBefore.hue;
            _detail.saturation = _detailBefore.saturation;
        }
    }
    // the buffer is given back to what the values of this response really need
    _detail.used = static_cast<size_t>(write - _detail.buffer);
    _detail.shrink();
    _detail.valid = true;
    __LDBG_printf("hass> panel of tile %u: %u bytes payload, %u bytes stored", static_cast<unsigned>(_detailTile),
                  static_cast<unsigned>(length), static_cast<unsigned>(_detail.used));
}

// ------------------------------------------------------------------------------------------
// status
// ------------------------------------------------------------------------------------------
const String &Dashboard::getScreenStatus() const
{
    // The status is read by the screen on every tick (up to twice, 5 Hz). It is composed into a
    // stack buffer and only copied into the member when it changed: a String built per call is one
    // heap allocation per tick. The buffer is as large as the longest message (the missing file
    // notice), a longer error of the configuration file is truncated like the display would clip it
    char buffer[sizeof(kMissingFileStatus)];
    if (!_config.isLoaded()) {
        if (_fileMissing) {
            memcpy(buffer, kMissingFileStatus, sizeof(kMissingFileStatus));
        }
        else {
            snprintf(buffer, sizeof(buffer), "Home Assistant: %s", _configError.c_str());
        }
    }
    else if (_requestError.length()) {
        // the error of the last request is a member of its own
        return _requestError;
    }
    else if (!_responseTime) {
        snprintf(buffer, sizeof(buffer), "waiting for %s", _config.getUrl());
    }
    else {
        buffer[0] = 0;
    }
    if (!_screenStatus.length() && !buffer[0]) {
        return _screenStatus;
    }
    if (!_screenStatus.length() || strcmp(_screenStatus.c_str(), buffer)) {
        _screenStatus = buffer;
    }
    return _screenStatus;
}

void Dashboard::getStatus(Print &output) const
{
    if (!_config.isLoaded()) {
        output.printf_P(PSTR("Home Assistant: %s%s"), _fileMissing ? "/hass.yaml is missing" : _configError.c_str(), HTML_S(br));
        return;
    }
    output.printf_P(PSTR("Home Assistant: %s, %u tile(s), resync %us, %u update(s), %s%s"),
        _config.getUrl(), static_cast<unsigned>(_config.getTileCount()), static_cast<unsigned>(_config.getPollInterval()),
        static_cast<unsigned>(_requestCount), getScreenStatus().c_str(), HTML_S(br));

    for (TileIndex i = 0; i < _config.getTileCount(); i++) {
        const auto &tile = _config.getTile(i);
        const auto &value = tile.value;
        if (tile.type == TileType::SPACER || tile.type == TileType::AREA) {
            // there is no entity behind these tiles, only the page of an area matters
            String info;
            if (tile.type == TileType::AREA) {
                StrWrapper(info).printf("page %u", static_cast<unsigned>(tile.areaPage));
            }
            output.printf_P(PSTR("  %s (%s) %s%s" HTML_S(br)), (tile.type == TileType::AREA) ? tile.name : "-",
                getTileTypeName(tile.type), info.c_str(), "");
            continue;
        }
        const char *state = "unknown";
        switch (value.state) {
        case TileState::ON:
            state = "on";
            break;
        case TileState::OFF:
            state = "off";
            break;
        case TileState::UNAVAILABLE:
            state = "unavailable";
            break;
        default:
            break;
        }
        output.printf_P(PSTR("  %s (%s) %s%.1f%s%s" HTML_S(br)), tile.entity, getTileTypeName(tile.type), state,
            static_cast<double>(value.value), tile.unit[0] ? tile.unit : value.unit, _flags[i].pending ? " (pending)" : "");
    }
}

} // namespace HomeAssistant
} // namespace WeatherStation2
