/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_debug.h"

#if IOT_LVGL_SUPPORT && DEBUG_LVGL_SCREENSHOT

#include <lvgl.h>
#include <esp32-hal-psram.h>
#include <async_web_response.h>
#include <HttpHeaders.h>
#include <web_server.h>
#include "lvgl_plugin.h"
#include "lvgl_screen.h"
#include "devices/wt32_sc01/wt32_sc01.h"

// Request handover: the HTTP task fills _request and sets _pending, the main loop performs the
// screenshot and sets _done. Only one request is in flight at a time (_busy), the HTTP task
// yields with delay() while it waits, so the main loop keeps running.
static char _request[192];
static volatile bool _pending = false;
static volatile bool _done = false;
static bool _busy = false;
static uint32_t _busySince = 0;
static constexpr uint32_t kBusyTimeout = 5000; // ms, safety net if a response is never finished
static constexpr uint32_t kCaptureTimeout = 3000; // ms, how long the HTTP task waits for the main loop

// The buffers are allocated once and reused - a 307 KB snapshot plus a 460 KB BMP per request
// would fragment PSRAM quickly while the page auto refreshes.
static uint8_t *_snapshot = nullptr;
static uint32_t _snapshotSize = 0;
static uint8_t *_bmp = nullptr;
static uint32_t _bmpSize = 0;
static size_t _bmpLength = 0;

static LVGLDebug::SetValueCallback _setValueCallback;
static LVGLDebug::GetValueHelpCallback _getHelpCallback;

static void _freeBuffers()
{
    if (_snapshot) {
        free(_snapshot);
        _snapshot = nullptr;
    }
    _snapshotSize = 0;
    if (_bmp) {
        free(_bmp);
        _bmp = nullptr;
    }
    _bmpSize = 0;
    _bmpLength = 0;
}

static void _write32(uint8_t *ptr, uint32_t value)
{
    ptr[0] = static_cast<uint8_t>(value);
    ptr[1] = static_cast<uint8_t>(value >> 8);
    ptr[2] = static_cast<uint8_t>(value >> 16);
    ptr[3] = static_cast<uint8_t>(value >> 24);
}

static void _write16(uint8_t *ptr, uint16_t value)
{
    ptr[0] = static_cast<uint8_t>(value);
    ptr[1] = static_cast<uint8_t>(value >> 8);
}

// renders the active screen into a PSRAM buffer and converts it to a 24 bit BMP
static bool _capture()
{
    auto disp = WT32_SC01::display();
    if (!disp) {
        return false;
    }
    auto screen = lv_disp_get_scr_act(disp);
    lv_refr_now(disp); // layout and draw everything, so the snapshot sees the final geometry

    auto snapshotSize = lv_snapshot_buf_size_needed(screen, LV_IMG_CF_TRUE_COLOR);
    if (!snapshotSize) {
        return false;
    }
    if (_snapshotSize < snapshotSize) {
        free(_snapshot);
        _snapshot = static_cast<uint8_t *>(ps_malloc(snapshotSize));
        _snapshotSize = _snapshot ? snapshotSize : 0;
    }
    if (!_snapshot) {
        __LDBG_printf("snapshot buffer allocation failed (%u bytes)", static_cast<unsigned>(snapshotSize));
        return false;
    }
    lv_img_dsc_t dsc;
    if (lv_snapshot_take_to_buf(screen, LV_IMG_CF_TRUE_COLOR, &dsc, _snapshot, snapshotSize) != LV_RES_OK) {
        return false;
    }

    const uint32_t width = dsc.header.w;
    const uint32_t height = dsc.header.h;
    const uint32_t rowSize = ((width * 3 + 3) / 4) * 4;
    const uint32_t imageSize = rowSize * height;
    const uint32_t fileSize = 54 + imageSize;
    if (_bmpSize < fileSize) {
        free(_bmp);
        _bmp = static_cast<uint8_t *>(ps_malloc(fileSize));
        _bmpSize = _bmp ? fileSize : 0;
    }
    if (!_bmp) {
        return false;
    }

    // BITMAPFILEHEADER + BITMAPINFOHEADER, 24 bit BGR, rows bottom up
    memset(_bmp, 0, 54);
    _bmp[0] = 'B';
    _bmp[1] = 'M';
    _write32(_bmp + 2, fileSize);
    _write32(_bmp + 10, 54);
    _write32(_bmp + 14, 40);
    _write32(_bmp + 18, width);
    _write32(_bmp + 22, height);
    _write16(_bmp + 26, 1);
    _write16(_bmp + 28, 24);
    _write32(_bmp + 34, imageSize);
    _write32(_bmp + 38, 2835); // 72 dpi
    _write32(_bmp + 42, 2835);

    auto pixels = reinterpret_cast<const lv_color_t *>(_snapshot);
    for (uint32_t y = 0; y < height; y++) {
        auto src = pixels + static_cast<size_t>(height - 1 - y) * width;
        auto dst = _bmp + 54 + static_cast<size_t>(y) * rowSize;
        for (uint32_t x = 0; x < width; x++) {
            // lv_color_to32() returns the 32 bit value of an lv_color32_t and is swap aware,
            // the channel accessors handle LV_COLOR_16_SWAP 1 (the panel needs byte swapped
            // pixels, reading .full of the 16 bit color would exchange red and blue)
            lv_color32_t color;
            color.full = lv_color_to32(src[x]);
            *dst++ = color.ch.blue;
            *dst++ = color.ch.green;
            *dst++ = color.ch.red;
        }
    }
    _bmpLength = fileSize;
    __LDBG_printf("screenshot %ux%u, %u bytes", static_cast<unsigned>(width), static_cast<unsigned>(height), static_cast<unsigned>(fileSize));
    return true;
}

// resolves "screen" to an index, -1 if there is no such screen
static int8_t _resolveScreen(const String &name)
{
    if (!name.length()) {
        return -1;
    }
    if (isDigit(name.charAt(0))) {
        auto index = static_cast<int8_t>(name.toInt());
        return (index < static_cast<int8_t>(LVGLPlugin::screens().count())) ? index : -1;
    }
    auto &screens = LVGLPlugin::screens();
    for (uint8_t i = 0; i < screens.count(); i++) {
        auto screenName = screens.get(i)->getName();
        if (screenName && name.equalsIgnoreCase(screenName)) {
            return static_cast<int8_t>(i);
        }
    }
    return -1;
}

// applies the request, returns true if a screen has to be rebuilt
static bool _applyRequest()
{
    auto &screens = LVGLPlugin::screens();
    bool reload = false;
    char *ptr = _request;
    while (*ptr) {
        auto end = strchr(ptr, '&');
        if (end) {
            *end = 0;
        }
        auto value = strchr(ptr, '=');
        if (value) {
            *value++ = 0;
            if (strcmp(ptr, "screen") == 0) {
                auto index = _resolveScreen(String(value));
                if (index >= 0) {
                    __LDBG_printf("screenshot of screen #%u (%s)", static_cast<unsigned>(index), value);
                    screens.show(static_cast<uint8_t>(index));
                    reload = true; // show() does not rebuild if the screen is already active
                }
                else if (strcmp(value, "overview") == 0) {
                    __LDBG_printf("screenshot of the screen overview");
                    if (screens.showOverview()) {
                        reload = true;
                    }
                }
            }
            else if (strcmp(ptr, "backlight") == 0) {
                auto percent = constrain(atoi(value), 0, 100);
                LVGLPlugin::setBrightness(static_cast<uint8_t>(percent));
            }
            else if (strcmp(ptr, "set") == 0) {
                // <key>:<value>[;<key>:<value>...]
                char *pair = value;
                while (*pair) {
                    auto next = strchr(pair, ';');
                    if (next) {
                        *next++ = 0;
                    }
                    auto separator = strchr(pair, ':');
                    if (separator) {
                        *separator++ = 0;
                        if (_setValueCallback && _setValueCallback(String(pair), String(separator))) {
                            __LDBG_printf("debug value '%s' = '%s'", pair, separator);
                            reload = true;
                        }
                    }
                    if (!next) {
                        break; // the last pair has no ';' after it (pair = next would be a NULL deref)
                    }
                    pair = next;
                }
            }
        }
        ptr = end ? end + 1 : ptr + strlen(ptr);
    }
    return reload;
}

// ------------------------------------------------------------------------------------------
// HTTP
// ------------------------------------------------------------------------------------------

class DebugBitmapResponse : public AsyncBaseResponse {
public:
    // A client that stops reading (it gave up, the tab went to the background, the connection was
    // dropped) never acknowledges the rest of the 460 KB. AsyncTCP reports data that was sent
    // without an acknowledgement after this many milliseconds (it has to be shorter than the 3 s RX
    // timeout of the server, which closes the connection when no packet arrives at all)
    static constexpr uint32_t kAckTimeout = 2500;

    DebugBitmapResponse(const uint8_t *data, size_t length) : AsyncBaseResponse(false), _data(data), _length(length), _position(0)
    {
        _code = 200;
        _contentLength = length;
        _sendContentLength = true;
        _chunked = false;
        _contentType = F("image/bmp");
        // browsers would show the last image again instead of a new capture
        _httpHeaders.replace<HttpCacheControlHeader>(HttpCacheControlHeader::CacheControlType::NONE, false, true, true);
    }
    virtual ~DebugBitmapResponse()
    {
        _busy = false; // the next request may capture again
    }

    virtual void _respond(AsyncWebServerRequest *request) override
    {
        auto *client = request->client();
        // The timeout callback of the server only closes the connection, which keeps the lwIP PCB
        // and every unacknowledged segment in the retransmit queue for about a minute - each of
        // them uses a slot of MEMP_NUM_TCP_PCB (16), so every new connection fails until the
        // retransmissions run out. This response replaces that callback for its own connection
        // (the response is sent with "Connection: close", so no other request can see it) and
        // aborts it instead: the PCB and the queued data are released at once and the capture is
        // available again immediately
        client->onTimeout([](void *arg, AsyncClient *client, uint32_t time) {
            (void)arg;
            (void)time;
            client->abort();
        }, nullptr);
        client->setAckTimeout(kAckTimeout);
        AsyncBaseResponse::_respond(request);
    }

    virtual bool _sourceValid() const override
    {
        return _length != 0;
    }

    virtual size_t _fillBuffer(uint8_t *data, size_t maxLen) override
    {
        auto length = std::min(maxLen, _length - _position);
        memcpy(data, _data + _position, length);
        _position += length;
        return length;
    }

private:
    const uint8_t *_data;
    size_t _length;
    size_t _position;
};

static void _handleScreenshot(AsyncWebServerRequest *request)
{
    if (!WebServer::Plugin::isAuthenticated(request)) {
        request->send(403);
        return;
    }
    if (_busy) {
        // the response that holds the lock was never finished (client aborted) - recover
        if (static_cast<int32_t>(millis() - _busySince) < static_cast<int32_t>(kBusyTimeout)) {
            request->send(503, F("text/plain"), F("a screenshot is already in progress"));
            return;
        }
        __LDBG_printf("screenshot lock timed out after %ums, recovering", static_cast<unsigned>(millis() - _busySince));
    }
    _busy = true;
    _busySince = millis();
    _bmpLength = 0;
    _request[0] = 0;

    auto addParameter = [](const String &name, const String &value) {
        if (!value.length()) {
            return;
        }
        auto used = strlen(_request);
        if (used + name.length() + value.length() + 2 >= sizeof(_request)) {
            return;
        }
        snprintf(_request + used, sizeof(_request) - used, "%s=%s&", name.c_str(), value.c_str());
    };
    addParameter(F("screen"), request->arg(F("screen")));
    addParameter(F("backlight"), request->arg(F("backlight")));
    addParameter(F("set"), request->arg(F("set")));

    _done = false;
    _pending = true;

    // the screenshot is taken by the main loop, this task yields while it waits
    auto started = millis();
    auto timeout = started + kCaptureTimeout;
    while (!_done && static_cast<int32_t>(millis() - timeout) < 0) {
        delay(1);
    }
    if (!_done || !_bmpLength) {
        _pending = false;
        _busy = false;
        __LDBG_printf("screenshot failed after %ums (done=%u length=%u)", static_cast<unsigned>(millis() - started), static_cast<unsigned>(_done), static_cast<unsigned>(_bmpLength));
        request->send(500, F("text/plain"), F("screenshot failed, see the device log"));
        return;
    }
    __LDBG_printf("screenshot served after %ums (%u bytes)", static_cast<unsigned>(millis() - started), static_cast<unsigned>(_bmpLength));
    request->send(new DebugBitmapResponse(_bmp, _bmpLength));
}

static void _handlePage(AsyncWebServerRequest *request)
{
    if (!WebServer::Plugin::isAuthenticated(request)) {
        request->send(403);
        return;
    }
    auto &screens = LVGLPlugin::screens();
    auto active = screens.getActiveIndex();

    String html = F(
        "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>LVGL screen debug</title>"
        "<style>body{background:#111;color:#ddd;font-family:monospace;margin:12px}"
        "img{background:#000;border:1px solid #444;max-width:96vw;display:block;margin-top:10px}"
        ".bar{margin:6px 0}input,button,select{font-family:monospace;background:#222;color:#ddd;"
        "border:1px solid #555;padding:3px}.hint{color:#888;font-size:12px;margin-top:6px}</style></head><body>"
        "<div class=\"bar\">screen <select id=\"s\">"
    );
    for (uint8_t i = 0; i < screens.count(); i++) {
        html += F("<option value=\"");
        html += String(i);
        html += F("\"");
        if (active == static_cast<int8_t>(i)) {
            html += F(" selected");
        }
        html += F(">");
        html += screens.get(i)->getName();
        html += F("</option>");
    }
    if (screens.getOverview()) {
        // the overview is not a registered screen, it is selected by name
        html += F("<option value=\"overview\"");
        if (screens.isOverviewActive()) {
            html += F(" selected");
        }
        html += F(">OVERVIEW</option>");
    }
    html += F(
        "</select> <button onclick=\"shot()\">reload</button>"
        " <label><input type=\"checkbox\" id=\"auto\" onchange=\"autoRefresh()\"> auto 2s</label>"
        " backlight <input id=\"bl\" size=\"3\" placeholder=\"%\"> <span id=\"info\"></span></div>"
        "<div class=\"bar\">set <input id=\"kv\" size=\"60\" placeholder=\"key:value;key:value\">"
        " <button onclick=\"apply()\">apply</button> <button onclick=\"clr()\">clear</button></div>"
        "<div class=\"hint\" id=\"hint\">"
    );
    if (_getHelpCallback) {
        if (auto help = _getHelpCallback()) {
            html += help;
        }
    }
    html += F(
        "</div><img id=\"img\" src=\"\"><script>"
        "var extra='',timer=null;"
        "function base(){var v=document.getElementById('s').value,b=document.getElementById('bl').value;"
        "return '/lvgl-screen.bmp?screen='+v+'&t='+Date.now()+(b?'&backlight='+b:'')+extra;}"
        "function shot(){document.getElementById('img').src=base();"
        "document.getElementById('info').textContent=new Date().toLocaleTimeString();}"
        "function apply(){var v=document.getElementById('kv').value.trim();"
        "if(v){extra+='&set='+encodeURIComponent(v);}shot();}"
        "function clr(){extra='';document.getElementById('kv').value='';shot();}"
        "function autoRefresh(){if(timer){clearInterval(timer);timer=null;}"
        "if(document.getElementById('auto').checked){timer=setInterval(shot,2000);}}"
        "shot();"
        "</script></body></html>"
    );
    request->send(200, F("text/html"), html);
}

namespace LVGLDebug {

void setValueCallback(SetValueCallback callback)
{
    _setValueCallback = callback;
}

void setHelpCallback(GetValueHelpCallback callback)
{
    _getHelpCallback = callback;
}

void setup()
{
    WebServer::Plugin::addHandler(F("/lvgl-screen.bmp"), _handleScreenshot);
    WebServer::Plugin::addHandler(F("/lvgl-screen"), _handlePage);
    __LDBG_printf("screenshot handlers registered (/lvgl-screen, /lvgl-screen.bmp)");
}

void loop()
{
    if (!_pending) {
        return;
    }
    _pending = false;
    _bmpLength = 0;
    if (_applyRequest()) {
        // A pushed value or a new screen needs the widget tree to be rebuilt: values that only
        // create() applies (the forecast days for example) would not change otherwise, and
        // LVGLScreenManager::show() does not rebuild a screen that is already active
        LVGLPlugin::screens().reload();
    }
    if (!_capture()) {
        __LDBG_printf("screenshot failed");
    }
    _done = true;
}

void release()
{
    _freeBuffers();
}

} // namespace LVGLDebug

#endif
