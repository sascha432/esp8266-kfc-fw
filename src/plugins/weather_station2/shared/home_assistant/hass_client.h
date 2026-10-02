/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// Client of the Home Assistant screen: one websocket connection that pushes the values of the tiles
// and sends the actions of the screen, both from a task of its own.
//
// The screen used to poll `POST /api/template` every `hass.poll` seconds. It now subscribes the
// same template with `render_template` (see Socket): Home Assistant re-renders it when one of the
// entities it reads changes and sends the result only when it differs, so a value that does not
// change costs nothing. The actions are sent over the same connection (`call_service`), the camera
// previews of a picture tile stay a GET of their own (a JPEG cannot be pushed) and the history
// graph of a sensor panel is read over the same connection as well.
//
// The task only stores the rendered template and the status (under _lock), the dashboard parses
// it in the main loop. The stored config is owned by the dashboard, Client::stop() has to be
// called before it is changed.
//

#include <Arduino_compat.h>
#include <Mutex.h>
#include <PrintString.h>

#include "hass_config.h"
#include "hass_socket.h"

class HTTPClient;

namespace WeatherStation2 {
namespace HomeAssistant {

class Client {
public:
    // one action of the screen, queued in the main loop and sent by the task
    struct Action {
        enum class Type : uint8_t {
            NONE,
            TOGGLE,        // switch/light on/off
            PRESS,         // button/input_button
            SET_LEVEL,     // dimmer, value = percent
            SET_TEMP,      // climate, value = degrees
            SET_HVAC_MODE, // climate, text = mode of the entity (off, heat, auto, ...)
            SET_PRESET,    // climate, text = preset_mode
            SET_FAN_MODE,  // climate, text = fan_mode
            SET_EFFECT,    // light, text = effect
            SET_COLOR,     // light, value = hue, value2 = saturation
            SET_COLOR_TEMP, // light, value = color temperature in kelvin
        };

        Type type{Type::NONE};
        uint8_t tile{0};
        float value{0};
        // second value (SET_COLOR) and the name of a mode/preset/fan/effect
        float value2{0};
        char text[24]{};
    };

    // stack of the request task (the TLS handshake runs on it)
    static constexpr uint32_t kTaskStack = 10240;
    // queued actions
    static constexpr size_t kQueueSize = 8;
    // time a queued action waits between two polls of the queue
    static constexpr uint32_t kIdleDelay = 100;
    // The task reads the events of the subscription for at most this long before it looks at the
    // rest of its work (the actions, the history graph and the camera previews). The values are
    // pushed by Home Assistant now and the events of a page arrive a few times per minute, so the
    // task is idle most of the time
    static constexpr uint32_t kPumpInterval = 50;
    // backoff of a connection that could not be opened (Home Assistant is down, the network is not
    // up yet, the token was rejected)
    static constexpr uint32_t kConnectRetryMin = 2000;
    static constexpr uint32_t kConnectRetryMax = 30000;
    // no panel is open
    static constexpr uint8_t kNoDetailTile = 0xff;
    // no page: the template is not built (or not subscribed) yet
    static constexpr uint8_t kNoPage = 0xff;

    // ------------------------------------------------------------------------------------
    // camera image of a picture tile
    // ------------------------------------------------------------------------------------
    // A picture tile is registered with its pixel box and the refresh interval of the
    // configuration. The request task fetches one image at a time (the JPEG of a camera proxy
    // is 40-80 KB, it would delay the pushed values and the action queue behind it) and stores the
    // decoded RGB565 frame for the LVGL task, which takes it over with takeImage().
    static constexpr uint8_t kMaxImageTiles = 4;
    // largest accepted JPEG. HA's camera proxy answers a 228x304 request with a 720x405 image
    // (about 40-80 KB); a camera that ignores the requested size can return the full sensor
    // image (a 1080p snapshot is a few hundred KB)
    static constexpr size_t kMaxImageSize = 393216;
    // A picture tile cannot be larger than the panel. The panel is 480x320 in landscape and
    // 320x480 in portrait (the dashboard can be rotated, see WeatherStation::HassRotation), so
    // both caps are the larger side of the two: the maps of the decoder and the image buffer of a
    // full screen image are sized for 480x480 px (460 KB of PSRAM)
    static constexpr uint16_t kMaxImageWidth = 480;
    static constexpr uint16_t kMaxImageHeight = 480;
    // A request that failed (camera off, no answer) is repeated after at least this long
    static constexpr uint32_t kMinImageRetry = 15000;

    // ------------------------------------------------------------------------------------
    // history graph of a sensor panel
    // ------------------------------------------------------------------------------------
    // The graph is drawn from the 5 minute aggregated long term statistics of Home Assistant,
    // which are only available over its websocket API (see Socket). The request task fetches them
    // like a camera image: one at a time, after the pushed values and the actions.
    static constexpr uint8_t kNoStatsTile = 0xff;
    // range of the graph in hours, the buttons of the sensor panel
    static constexpr uint8_t kMinStatsHours = 12;
    static constexpr uint8_t kMaxStatsHours = 48;
    static constexpr uint8_t kDefaultStatsHours = 24;

    // Requests the statistics of the entity of a tile for the last `hours` hours. The request is
    // performed by the task, takeStats() hands the answer over to the main loop
    void requestStats(uint8_t tile, uint8_t hours);
    // Copies a new statistics response into the buffers of the caller: false while no response
    // arrived since `generation`. `points` needs room for Socket::kMaxPoints buckets. The error is
    // empty when the window has no statistics at all (the entity has none)
    bool takeStats(uint32_t &generation, uint8_t &tile, uint8_t &hours, uint32_t &start, uint32_t &end, Socket::Point *points,
                   uint16_t &count, String &error);

    // Registers a picture tile of the visible page (idempotent, the dashboard calls it on
    // every update). `width`/`height` are the pixel box of the tile on the panel and
    // `interval` the number of seconds between two images
    void requestImage(uint8_t tile, uint16_t width, uint16_t height, uint16_t interval);
    // No picture tile is visible: the pending requests are dropped. A frame that waits for the
    // LVGL task is kept (the screen collects it and releases the buffer)
    void clearImages();
    // hands the last decoded frame over to the caller, which owns the PSRAM buffer and has to
    // release it with free(). false while no frame is waiting
    bool takeImage(uint8_t &tile, uint16_t *&data, uint16_t &width, uint16_t &height, uint32_t &stamp);

    Client();
    ~Client();

    // builds the template and starts the task. The config has to outlive the client
    bool begin(const Config &config);
    // stops the task and waits for it, call it before the config is changed
    void stop();

    // The connection is only subscribed while the screen is visible: the values of the page that is
    // shown are pushed by Home Assistant and nothing is requested while the screen is off
    void setActive(bool active);
    // Page the screen shows (0 = the main page). The subscription is built from the tiles of that
    // page only: the template grows with the number of tiles and a page that is not shown does not
    // have to be up to date. The request task rebuilds and subscribes the template - the main loop
    // must not touch it while the task sends it
    void setVisiblePage(uint8_t page);
    // One tile whose `call_service` failed (the request could not be sent or Home Assistant
    // answered with an error). The dashboard reads them and reverts the tile to the state the
    // entity reports (the screen showed the result of the tap right away, see
    // Dashboard::toggle()): takeActionFailure() hands one tile index out
    bool takeActionFailure(uint8_t &tile);
    // subscribes the template of the visible page again (one render on the Home Assistant side,
    // the answer is pushed): the safety net of the subscription and what the screen does when it
    // is opened
    void requestRefresh();
    // queues an action, false when the queue is full
    bool queueAction(const Action &action);
    // true while an action is waiting or in flight
    bool hasPendingAction();

    // response of the last pushed template (main loop). `page` is the page the response was built
    // from: the tiles of the other pages are only touched by it where they use one of its entities
    bool takeResponse(String &response, uint8_t &page);
    // opens the panel of a tile: the attributes of its entity (the mode/preset/fan/effect lists,
    // the color and the level) are part of the subscribed template until closeDetail() is called
    void requestDetail(uint8_t tile);
    void closeDetail();
    // response of the last pushed template, while a panel is open (main loop)
    bool takeDetailResponse(String &response);
    // state of the connection (main loop)
    void takeStatus(int16_t &statusCode, String &error, uint32_t &duration, uint32_t &responseTime);

    uint32_t getRequestCount() const {
        return _requestCount;
    }
    uint32_t getActionCount() const {
        return _actionCount;
    }
    bool isRunning() const {
        return _task != nullptr;
    }

private:
    // one registered picture tile of the visible page
    struct ImageRequest {
        uint16_t width{0};
        uint16_t height{0};
        uint16_t interval{kDefaultRefresh};
        // millis() the next image is due
        uint32_t next{0};
        bool used{false};
        uint8_t tile{0};
    };

    static void _taskEntry(void *arg);
    void _loop();
    // hands the pushed template over to the main loop
    void _takePushedTemplate();
    // sends one action over the connection
    void _sendAction(const Action &action);
    // one camera image: request, decode and store the frame for the LVGL task
    bool _fetchImage(const ImageRequest &request);
    // statistics of the entity of a tile, stores the buckets for the LVGL task
    void _fetchStats(uint8_t tile, uint8_t hours);
    // decodes a JPEG into the RGB565 buffer of the tile. `pixels` is in PSRAM and owned by the
    // caller, `info` carries the sizes and times of the trace
    bool _decodeImage(const uint8_t *data, size_t length, uint16_t width, uint16_t height, uint16_t *&pixels, PrintString &info);
    // one GET without a body, the response is stored in a PSRAM buffer (up to kMaxImageSize)
    bool _get(const String &url, uint8_t *&data, size_t &length, int16_t &statusCode);
    // headers, GET and the body of a binary request that was begun
    bool _getBody(HTTPClient &http, uint8_t *&data, size_t &length, int16_t &statusCode, uint32_t timeout);
    // when the next image of a tile is due after a failure
    static uint32_t _imageRetryDelay(uint16_t interval);
    // true when the URL uses https
    bool _isHttps() const;
    // copies the domain of an entity id ("button" of "button.restart")
    static void _entityDomain(const char *entity, char *output, size_t outputSize);
    // builds the subscribed template from the tiles of the visible page and the open panel
    void _buildTemplate();
    // domain, service and service data of an action
    void _buildAction(const Action &action, PrintString &domain, PrintString &service, PrintString &data) const;
    // takes the next action of the queue
    bool _popAction(Action &action);
    // sets the error of the last request
    void _setError(const String &error);

private:
    const Config *_config{nullptr};
    String _template;
    // the page _template was built from (kNoPage while no template is built)
    uint8_t _templatePage{kNoPage};
    // written by the request task, read by the main loop, both under _lock
    String _response;
    // the page the response in _response was built from
    uint8_t _responsePage{kNoPage};
    String _error;
    // The dashboard publishes which tile is open (kNoDetailTile = no panel is open), the task adds
    // the attributes of its entity to the subscribed template
    String _detailResponse;
    volatile bool _detailValid{false};
    volatile uint8_t _detailTile{kNoDetailTile};
    mutable SemaphoreMutex _lock;
    volatile bool _responseValid{false};
    volatile int16_t _statusCode{0};
    volatile uint32_t _duration{0};
    volatile uint32_t _responseTime{0};
    volatile uint32_t _requestCount{0};
    volatile uint32_t _actionCount{0};
    // set by the main loop, read by the task
    volatile bool _active{false};
    volatile bool _refreshRequested{false};
    volatile bool _stop{false};
    // page the subscribed template is built from and the flag that it has to be built (and
    // subscribed) again (the task owns _template, the main loop only publishes the page)
    volatile uint8_t _visiblePage{0};
    volatile bool _templateDirty{false};
    // request task
    void *_task{nullptr};
    // the task subscribes the template again (a page change, an open panel, a resync or a
    // connection that was opened again)
    volatile bool _resubscribe{false};
    // the connection to the websocket API of Home Assistant
    Socket _socket;
    // queued actions, guarded by _lock
    Action _queue[kQueueSize];
    uint8_t _queueHead{0};
    uint8_t _queueTail{0};
    // tiles whose action failed, one bit per tile index (kMaxTiles / 32 words), guarded by _lock
    uint32_t _actionFailed[(kMaxTiles + 31) / 32]{};
    // picture tiles of the visible page, guarded by _lock
    ImageRequest _images[kMaxImageTiles];
    // frame waiting for the LVGL task and its owner, guarded by _lock
    uint16_t *_image{nullptr};
    uint16_t _imageWidth{0};
    uint16_t _imageHeight{0};
    uint8_t _imageTile{0};
    uint32_t _imageStamp{0};
    bool _imageValid{false};
    // statistics of the trace
    volatile uint32_t _imageCount{0};
    volatile uint32_t _imageFailureCount{0};
    // Statistics request of the sensor panel. The main loop publishes the tile and the range of
    // the window and the task performs the request (one at a time, like a camera image). The
    // buckets are written into _statsPoints by the task and the main loop copies them out under
    // _lock
    volatile uint8_t _statsTile{kNoStatsTile};
    volatile uint8_t _statsHours{kDefaultStatsHours};
    volatile bool _statsRequest{false};
    Socket::Point _statsPoints[Socket::kMaxPoints];
    uint16_t _statsCount{0};
    // response of the last request, guarded by _lock
    uint8_t _statsResultTile{kNoStatsTile};
    uint8_t _statsResultHours{0};
    uint32_t _statsStart{0};
    uint32_t _statsEnd{0};
    String _statsError;
    volatile uint32_t _statsGeneration{0};
    // a result was stored and not copied out by the main loop yet: the next request waits for it,
    // otherwise the buckets of a window that is still shown would be overwritten
    volatile bool _statsUntaken{false};
};

} // namespace HomeAssistant
} // namespace WeatherStation2
