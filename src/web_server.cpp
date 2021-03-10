/**
 * Author: sascha_lammers@gmx.de
 */

#if WEBSERVER_SUPPORT

#include <Arduino_compat.h>
#include <ArduinoOTA.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include <BufferStream.h>
#include <JsonConfigReader.h>
#include <ESPAsyncWebServer.h>
#include "build.h"
#include "web_server.h"
#include "rest_api.h"
#include "async_web_response.h"
#include "async_web_handler.h"
#include "kfc_fw_config.h"
#include "blink_led_timer.h"
#include "fs_mapping.h"
#include "session.h"
#include "../include/templates.h"
#include "web_socket.h"
#include "WebUISocket.h"
#include "WebUIAlerts.h"
#include "kfc_fw_config.h"
#include "failure_counter.h"
#if STK500V1
#include "../src/plugins/stk500v1/STK500v1Programmer.h"
#endif
#include "./plugins/mdns/mdns_sd.h"
#if IOT_REMOTE_CONTROL
#include "./plugins/remote/remote.h"
#endif
#if IOT_CLOCK
#include "./plugins/clock/clock.h"
#endif

#if DEBUG_WEB_SERVER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif
#include <debug_helper_disable_mem.h>

#define DEBUG_WEB_SERVER_SID                DEBUG_WEB_SERVER

#if DEBUG_WEB_SERVER_SID
#define __SID(...)                          __VA_ARGS__
#else
#define __SID(...)                          ;
#endif

using KFCConfigurationClasses::System;

using namespace WebServer;

static Plugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Plugin,
    "http",             // name
    "Web server",       // friendly name
    "",                 // web_templates
    "remote",           // config_forms
    "network",          // reconfigure_dependencies
    PluginComponent::PriorityType::HTTP,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    true,               // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

#define U_ATMEGA 254

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
#ifndef U_SPIFFS
#define U_SPIFFS U_FS
#endif
#endif


WebServerSetCPUSpeedHelper::WebServerSetCPUSpeedHelper() : SpeedBooster(
#if defined(ESP8266)
    System::Flags::getConfig().is_webserver_performance_mode_enabled
#endif
) {
}

HttpCookieHeader *createRemoveSessionIdCookie()
{
    return new HttpCookieHeader(FSPGM(SID, "SID"), String(), String('/'), HttpCookieHeader::COOKIE_EXPIRED);
}

const __FlashStringHelper *getContentType(const String &path)
{
    if (path.endsWith(FSPGM(_html)) || path.endsWith(F(".htm"))) {
        return FSPGM(mime_text_html, "text/html");
    }
    else if (path.endsWith(F(".css"))) {
        return FSPGM(mime_text_css, "text/css");
    }
    else if (path.endsWith(F(".json"))) {
        return FSPGM(mime_application_json, "application/json");
    }
    else if (path.endsWith(F(".js"))) {
        return FSPGM(mime_application_javascript, "application/javascript");
    }
    else if (path.endsWith(F(".png"))) {
        return FSPGM(mime_image_png, "image/png");
    }
    else if (path.endsWith(F(".gif"))) {
        return FSPGM(mime_image_gif, "image/gif");
    }
    else if (path.endsWith(F(".jpg"))) {
        return FSPGM(mime_image_jpeg, "image/jpeg");
    }
    else if (path.endsWith(F(".ico"))) {
        return FSPGM(mime_image_icon, "image/x-icon");
    }
    else if (path.endsWith(F(".svg"))) {
        return FSPGM(mime_image_svg_xml, "image/svg+xml");
    }
    else if (path.endsWith(F(".eot"))) {
        return FSPGM(mime_font_eot, "font/eot");
    }
    else if (path.endsWith(F(".woff"))) {
        return FSPGM(mime_font_woff, "font/woff");
    }
    else if (path.endsWith(F(".woff2"))) {
        return FSPGM(mime_font_woff2, "font/woff2");
    }
    else if (path.endsWith(F(".ttf"))) {
        return FSPGM(mime_font_ttf, "font/ttf");
    }
    else if (path.endsWith(FSPGM(_xml, ".xml"))) {
        return FSPGM(mime_text_xml, "text/xml");
    }
    else if (path.endsWith(F(".pdf"))) {
        return FSPGM(mime_application_pdf, "application/pdf");
    }
    else if (path.endsWith(F(".zip"))) {
        return FSPGM(mime_application_zip, "application/zip");
    }
    else if (path.endsWith(F(".gz"))) {
        return FSPGM(mime_application_x_gzip, "application/x-gzip");
    }
    else {
        return FSPGM(mime_text_plain, "text/plain");
    }
}

static void executeDelayed(AsyncWebServerRequest *request, std::function<void()> callback)
{
    request->onDisconnect([callback]() {
#if IOT_WEATHER_STATION
        __weatherStationAttachCanvas();
#endif
        _Scheduler.add(2000, false, [callback](Event::CallbackTimerPtr timer) {
            callback();
        });
    });
}

bool Plugin::_isPublic(const String &pathString) const
{
    auto path = pathString.c_str();
    if (strstr_P(path, PSTR(".."))) {
        return false;
    }
    else if (*path++ == '/') {
        return (
            !strcmp_P(path, SPGM(description_xml)) ||
            !strncmp_P(path, PSTR("css/"), 4) ||
            !strncmp_P(path, PSTR("js/"), 3) ||
            !strncmp_P(path, PSTR("images/"), 7) ||
            !strncmp_P(path, PSTR("fonts/"), 6)
        );
    }
    return false;
}

bool Plugin::_clientAcceptsGzip(AsyncWebServerRequest *request) const
{
    auto header = request->getHeader(FSPGM(Accept_Encoding, "Accept-Encoding"));
    if (!header) {
        return false;
    }
    auto acceptEncoding = header->value().c_str();
    return (strstr_P(acceptEncoding, SPGM(gzip, "gzip")) || strstr_P(acceptEncoding, SPGM(deflate, "deflate")));
}

// AsyncBaseResponse requires a different method to attach headers
// TODO implement setHttpHeaders to AsyncWebServerResponse
// response->setHttpHeaders(HttpHeadersVector &&);
static void _prepareAsyncBaseResponse(AsyncWebServerRequest *request, AsyncWebServerResponse *response, HttpHeaders &headers)
{
    headers.setAsyncBaseResponseHeaders(reinterpret_cast<AsyncBaseResponse *>(response));
}

static void _prepareAsyncWebserverResponse(AsyncWebServerRequest *request, AsyncWebServerResponse *response, HttpHeaders &headers)
{
    headers.setAsyncWebServerResponseHeaders(response);
}

// 404 handler
// to avoid having multiple handlers and save RAM, all custom handlers are executed here
void Plugin::handlerNotFound(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    HttpHeaders httpHeaders(false);
    AsyncWebServerResponse *response = nullptr;

    auto &url = request->url();
    if (url == F("/is-alive")) {
        response = request->beginResponse(200, FSPGM(mime_text_plain), String(request->arg(String('p')).toInt()));
        httpHeaders.addNoCache();
        _prepareAsyncWebserverResponse(request, response, httpHeaders);
    }
    else if (url == F("/webui-handler")) {
        plugin._handlerWebUI(request, httpHeaders);
        return;
    }
    else if (url == F("/alerts")) {
        plugin._handlerAlerts(request, httpHeaders);
        return;
    }
    else if (url == F("/sync-time")) {
        if (!plugin.isAuthenticated(request)) {
            request->send(403);
            return;
        }
        httpHeaders.addNoCache();
        PrintHtmlEntitiesString str;
        WebTemplate::printSystemTime(time(nullptr), str);
        response = new AsyncBasicResponse(200, FSPGM(mime_text_html), str);
        _prepareAsyncWebserverResponse(request, response, httpHeaders);
    }
    else if (url == F("/export-settings")) {
        plugin._handlerImportSettings(request, httpHeaders);
        return;
    }
    else if (url == F("/import-settings")) {
        plugin._handlerExportSettings(request, httpHeaders);
        return;
    }
    else if (url == F("/scan-wifi")) {
        if (!plugin.isAuthenticated(request)) {
            request->send(403);
            return;
        }
        response = new AsyncNetworkScanResponse(request->arg(FSPGM(hidden, "hidden")).toInt());
        _prepareAsyncBaseResponse(request, response, httpHeaders);
    }
    else if (url == F("/logout")) {
        __SID(__DBG_printf("sending remove SID cookie"));
        httpHeaders.addNoCache(true);
        httpHeaders.add(createRemoveSessionIdCookie());
        httpHeaders.add<HttpLocationHeader>(String('/'));
        response = request->beginResponse(302);
        _prepareAsyncWebserverResponse(request, response, httpHeaders);
    }
    else if (url == F("zeroconf")) {
        if (!plugin.isAuthenticated(request)) {
            request->send(403);
            return;
        }
        httpHeaders.addNoCache(true);
        response = new AsyncResolveZeroconfResponse(request->arg(FSPGM(value)));
        _prepareAsyncBaseResponse(request, response, httpHeaders);
    }
    else if (url.startsWith(F("/speedtest."))) { // handles speedtest.zip and speedtest.bmp
        plugin._handlerSpeedTest(request, !url.endsWith(F(".bmp")), httpHeaders);
        return;
    }

    // else section
    if (plugin._handleFileRead(url, plugin._clientAcceptsGzip(request), request, httpHeaders)) {
        return;
    }
    if (response) {
        request->send(response);
        return;
    }
    request->send(404);
}


void Plugin::_handlerWebUI(AsyncWebServerRequest *request, HttpHeaders &httpHeaders)
{
    if (!plugin.isAuthenticated(request)) {
        request->send(403);
        return;
    }
    // 503 if the webui is disabled
    if (!System::Flags::getConfig().is_webui_enabled) {
        request->send(503);
    }
    String str;
    {
        JsonUnnamedObject json;
        WebUISocket::createWebUIJSON(json);
        str = json.toString();
    }
    auto response = new AsyncBasicResponse(200, FSPGM(mime_application_json), str);
    httpHeaders.addNoCache();

    // PrintHtmlEntitiesString timeStr;
    // WebTemplate::printSystemTime(time(nullptr), timeStr);
    // httpHeaders.add<HttpSimpleHeader>('device-time', timeStr);

    httpHeaders.setAsyncWebServerResponseHeaders(response);
    //TODO fix
    // auto response = new AsyncJsonResponse();
    // WebUISocket::createWebUIJSON(response->getJsonObject());
    // HttpHeaders httpHeaders;
    // httpHeaders.addNoCache();
    // httpHeaders.setAsyncBaseResponseHeaders(response);
    request->send(response);
}

#if WEBUI_ALERTS_ENABLED

void Plugin::_handlerAlerts(AsyncWebServerRequest *request, HttpHeaders &headers)
{
    if (!isAuthenticated(request)) {
        request->send(403);
        return;
    }
    // 503 if disabled
    if (!WebAlerts::Alert::hasOption(WebAlerts::OptionsType::ENABLED)) {
        request->send(503);
    }
    headers.addNoCache(true);

    if (request->arg(F("clear")).toInt()) {
        WebAlerts::Alert::dismissAll();
        request->send(200);
        return;
    }

    auto dismiss = request->arg(F("dismiss_id"));
    if (dismiss.length() != 0) {
        WebAlerts::Alert::dismissAlerts(dismiss);
        request->send(200);
        return;
    }

    WebAlerts::IdType pollId = request->arg(F("poll_id")).toInt();
    if (
        (pollId > WebAlerts::FileStorage::getMaxAlertId()) ||
        !_sendFile(FSPGM(alerts_storage_filename), emptyString, headers, _clientAcceptsGzip(request), true, request, nullptr)
    ) {
        request->send(204);
    }

    // auto pollId = (uint32_t)request->arg(F("poll_id")).toInt();
    // if (pollId) {
    //     PrintHtmlEntitiesString str;
    //     WebAlerts::Alert::printAlertsAsJson(str, pollId, false);

    //     AsyncWebServerResponse *response = new AsyncBasicResponse(200, FSPGM(mime_application_json), str);
    //     HttpHeaders httpHeaders;
    //     httpHeaders.addNoCache();
    //     httpHeaders.setAsyncWebServerResponseHeaders(response);
    //     request->send(response);
    //     return;
    // }
    // else {
    //     auto alertId = (WebAlerts::IdType)request->arg(FSPGM(id)).toInt();
    //     if (alertId) {
    //         WebAlerts::Alert::dimiss(alertId);
    //         request->send(200, FSPGM(mime_text_plain), FSPGM(OK));
    //         return;
    //     }
    // }
    // request->send(400);
}

#else

void Plugin::_handlerAlerts(AsyncWebServerRequest *request, HttpHeaders &headers)
{
    // 503 response if disabled (no support compiled in)
    request->send(503);
}

#endif

void Plugin::_addRestHandler(RestHandler &&handler)
{
    if (_server) {
        // install rest handler
        __DBG_printf("installing REST handler url=%s", handler.getURL());
        if (_server->_restCallbacks.empty()) {
            // add AsyncRestWebHandler to web server
            // the object gets destroyed with the web server and requires to keep the list off callbacks separated
            auto restHandler = new AsyncRestWebHandler();
            __DBG_printf("handler=%p", restHandler);
            _server->addHandler(restHandler);
        }
        // store handler
        _server->_restCallbacks.emplace_back(std::move(handler));
    }
}

void Plugin::_handlerSpeedTest(AsyncWebServerRequest *request, bool zip, HttpHeaders &httpHeaders)
{
#if !defined(SPEED_TEST_NO_AUTH) || SPEED_TEST_NO_AUTH == 0
    if (!isAuthenticated(request)) {
        request->send(403);
        return;
    }
#endif
    httpHeaders.clear(2); // remove default headers and reserve 3 headers
    httpHeaders.addNoCache(); // adds 2 headers

    AsyncSpeedTestResponse *response;
    auto size = std::max(1024 * 64, (int)request->arg(FSPGM(size, "size")).toInt());
    if (zip) {
        response = new AsyncSpeedTestResponse(FSPGM(mime_application_zip), size);
        httpHeaders.add<HttpDispositionHeader>(F("speedtest.zip"));
    } else {
        response = new AsyncSpeedTestResponse(FSPGM(mime_image_bmp), size);
    }
    httpHeaders.setAsyncBaseResponseHeaders(response);

    request->send(response);
}

void Plugin::_handlerImportSettings(AsyncWebServerRequest *request, HttpHeaders &httpHeaders)
{
    if (!isAuthenticated(request)) {
        request->send(403);
        return;
    }

    httpHeaders.clear();
    httpHeaders.addNoCache();

    PGM_P message;
    int count = -1;

    auto configArg = FSPGM(config, "config");
    if (request->method() == HTTP_POST && request->hasArg(configArg)) {
        BufferStream data(request->arg(configArg));
        config.discard();

        JsonConfigReader reader(&data, config, nullptr);
        reader.initParser();
        bool result = reader.parseStream();
        if (result) {
            config.write();

            count = reader.getImportedHandles().size();
            message = SPGM(Success, "Success");
        }
        else {
            message = PSTR("Failed to parse JSON data");
        }
    }
    else {
        message = PSTR("No POST request");
    }
    request->send(200, FSPGM(mime_text_plain), PrintString(F("{\"count\":%d,\"message\":\"%s\"}"), count, message));
}

void Plugin::_handlerExportSettings(AsyncWebServerRequest *request, HttpHeaders &httpHeaders)
{
    if (!isAuthenticated(request)) {
        request->send(403);
        return;
    }
    httpHeaders.clear();
    httpHeaders.addNoCache();

    auto hostname = System::Device::getName();

    char timeStr[32];
    auto now = time(nullptr);
    struct tm *tm = localtime(&now);
    strftime_P(timeStr, sizeof(timeStr), PSTR("%Y%m%d_%H%M%S"), tm);
    PrintString filename(F("kfcfw_config_%s_b" __BUILD_NUMBER "_%s.json"), hostname, timeStr);
    httpHeaders.add<HttpDispositionHeader>(filename);

    PrintString content;
    config.exportAsJson(content, config.getFirmwareVersion());
    AsyncWebServerResponse *response = new AsyncBasicResponse(200, FSPGM(mime_application_json), content);
    httpHeaders.setAsyncWebServerResponseHeaders(response);

    request->send(response);
}

void Plugin::end()
{
    __LDBG_printf("server=%p", _server);
    _server.reset();
    _loginFailures.reset();
}

void Plugin::begin(bool restart)
{
    auto mode = System::WebServer::getMode();
    if (mode == System::WebServer::ModeType::DISABLED) {
#if MDNS_PLUGIN
        // call announce to remove the web server from MDNS
        // otherwise it shows until TTL is reached
        MDNSService::announce();
#endif
        return;
    }

    auto cfg = System::WebServer::getConfig();
    _server.reset(new AsyncWebServerEx(cfg.getPort()));
    if (!_server) { // out of memory?
        __DBG_printf("AsyncWebServerEx: failed to create object");
        return;
    }

#if MDNS_PLUGIN
    // add web server
    String service = FSPGM(http);
    if (cfg.is_https) {
        service += 's';
    }
    MDNSService::addService(service, FSPGM(tcp, "tcp"), cfg.getPort());
    MDNSService::announce();
#endif

    if (System::Flags::getConfig().is_log_login_failures_enabled) {
        _loginFailures.reset(new FailureCounterContainer());
        if (_loginFailures) {
            _loginFailures->readFromSPIFFS();
        }
    }
    else {
        _loginFailures.reset();
    }

    _server->onNotFound(handlerNotFound);

    // setup webui socket
    if (System::Flags::getConfig().is_webui_enabled) {
        __DBG_printf("WebUISocket::_server=%p", WebUISocket::getServerSocket());
        if (WebUISocket::getServerSocket() == nullptr) {
            WebUISocket::setup(_server.get());
        }
    }

    if (!restart) {
        _server->addHandler(new AsyncUpdateWebHandler());
    }

    _server->begin();
    __LDBG_printf("HTTP running on port %u", System::WebServer::getConfig().getPort());
}

bool Plugin::_sendFile(const FileMapping &mapping, const String &formName, HttpHeaders &httpHeaders, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate)
{
    __LDBG_printf("mapping=%s exists=%u form=%s gz=%u auth=%u request=%p web_template=%p", mapping.getFilename(), mapping.exists(), formName.c_str(), client_accepts_gzip, isAuthenticated, request, webTemplate);

    if (!mapping.exists()) {
        return false;
    }

    auto &path = mapping.getFilenameString();
    bool isHtml = path.endsWith(FSPGM(_html));
    if (isAuthenticated && webTemplate == nullptr) {
        if (path.charAt(0) == '/' && formName.length()) {
            __LDBG_printf("template=%s", formName.c_str());
            auto plugin = PluginComponent::getTemplate(formName);
            if (plugin) {
                webTemplate = plugin->getWebTemplate(formName);
            }
            else if (nullptr != (plugin = PluginComponent::getForm(formName))) {
#if IOT_WEATHER_STATION
                __weatherStationDetachCanvas(true);
                request->onDisconnect(__weatherStationAttachCanvas); // unlock on disconnect
#endif
                __LDBG_printf("form=%s", formName.c_str());
                FormUI::Form::BaseForm *form = __LDBG_new(SettingsForm, nullptr);
                plugin->createConfigureForm(PluginComponent::FormCallbackType::CREATE_GET, formName, *form, request);
                webTemplate = __LDBG_new(ConfigTemplate, form);
            }
        }
    }
    if ((isHtml || path.endsWith(FSPGM(_xml))) && webTemplate == nullptr) {
        webTemplate = __LDBG_new(WebTemplate); // default for all .html files
    }

    __LDBG_printf("web_template=%p", webTemplate);
    AsyncBaseResponse *response;
    if (webTemplate != nullptr) {
        webTemplate->setSelfUri(request->url());
        webTemplate->setAuthenticated(isAuthenticated);
        // process with template
        response = __LDBG_new(AsyncTemplateResponse, FPSTR(getContentType(path)), mapping.open(FileOpenMode::read), webTemplate, [webTemplate](const String& name, DataProviderInterface &provider) {
            return TemplateDataProvider::callback(name, provider, *webTemplate);
        });
        httpHeaders.addNoCache();
    }
    else {
        httpHeaders.replace<HttpDateHeader>(FSPGM(Expires), 86400 * 30);
        httpHeaders.replace<HttpDateHeader>(FSPGM(Last_Modified), mapping.getModificationTime());
        if (_isPublic(path)) {
            httpHeaders.replace<HttpCacheControlHeader>(HttpCacheControlHeader::PUBLIC);
        }
        // regular file
#if 0
        response = new AsyncProgmemFileResponse(FPSTR(getContentType(path)), mapping.open(FileOpenMode::read));
#else
        auto response = new AsyncFileResponse(mapping.open(FileOpenMode::read), path, FPSTR(getContentType(path)));
        if (mapping.isGz()) {
            httpHeaders.add(FSPGM(Content_Encoding), FSPGM(gzip));
        }
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
        return true;
#endif
    }
    if (mapping.isGz()) {
        httpHeaders.add(FSPGM(Content_Encoding), FSPGM(gzip));
    }
    httpHeaders.setAsyncBaseResponseHeaders(response);
    request->send(response);

    return true;
}

void Plugin::setUpdateFirmwareCallback(UpdateFirmwareCallback callback)
{
#if 0
    if (_updateFirmwareCallback) { // create a list of callbacks that frees itself upon leaving the lamba
        auto prev_callback = std::move(_updateFirmwareCallback);
        _updateFirmwareCallback = [prev_callback, callback](size_t pos, size_t size) {
            prev_callback(pos, size);
            callback(pos, size);
        };
    }
    else { // single callback
        _updateFirmwareCallback = callback;
    }
#else
    _updateFirmwareCallback = callback;
#endif
}

bool Plugin::_handleFileRead(String path, bool client_accepts_gzip, AsyncWebServerRequest *request, HttpHeaders &httpHeaders)
{
    __LDBG_printf("path=%s gz=%u request=%p", path.c_str(), client_accepts_gzip, request);

    if (String_endsWith(path, '/')) {
        path += FSPGM(index_html);
    }

    String formName;
    auto mapping = FileMapping(path.c_str());
    if (path.charAt(0) == '/') {
        int pos;
        if (!mapping.exists() && (pos = path.indexOf('/', 3)) != -1 && path.endsWith(FSPGM(_html))) {
            formName = path.substring(pos + 1, path.length() - 5);
            String path2 = path.substring(0, pos) + FSPGM(_html);
            mapping = FileMapping(path2.c_str());
            __LDBG_printf("path=%s path2=%s form=%s mapping=%u name=%s", path.c_str(), path2.c_str(), formName.c_str(), mapping.exists(), mapping.getFilenameString().c_str());
        }
        else {
            formName = path.substring(1, path.length() - 5);
        }
    }

    if (!mapping.exists()) {
        __LDBG_printf("not found=%s", path.c_str());
        return false;
    }
    if (mapping.isGz() && !client_accepts_gzip) {
        __LDBG_printf("gzip not supported=%s", path.c_str());
        request->send_P(503, FSPGM(mime_text_plain), PSTR("503: Client does not support gzip Content Encoding"));
        return true;
    }

    auto authType = getAuthenticated(request);
    auto isAuthenticated = this->isAuthenticated(request);
    WebTemplate *webTemplate = nullptr;

    if (!_isPublic(path) && !isAuthenticated) {
        auto loginError = FSPGM(Your_session_has_expired, "Your session has expired");

        if (request->hasArg(FSPGM(SID))) { // just report failures if the cookie is invalid
            __SID(debug_printf_P(PSTR("invalid SID=%s\n"), request->arg(FSPGM(SID)).c_str()));
            Logger_security(F("Authentication failed for %s"), IPAddress(request->client()->getRemoteAddress()).toString().c_str());
        }

        httpHeaders.addNoCache(true);

        // no_cache_headers();
        if (request->method() == HTTP_POST && request->hasArg(FSPGM(username)) && request->hasArg(FSPGM(password))) {
            IPAddress remote_addr = request->client()->remoteIP();
            auto username = System::Device::getUsername();
            auto password = System::Device::getPassword();

            __SID(debug_printf_P(PSTR("blocked=%u username=%s match:user=%u pass=%u\n"), _loginFailures && _loginFailures->isAddressBlocked(remote_addr), request->arg(FSPGM(username)).c_str(), (request->arg(FSPGM(username)) == username), (request->arg(FSPGM(password)) == password)));

            if (
                ((!_loginFailures) || (_loginFailures && _loginFailures->isAddressBlocked(remote_addr) == false)) &&
                (request->arg(FSPGM(username)) == username && request->arg(FSPGM(password)) == password)
            ) {

                auto &cookie = httpHeaders.add<HttpCookieHeader>(FSPGM(SID), generate_session_id(username, password, nullptr), String('/'));
                authType = AuthType::PASSWORD;
                time_t keepTime = request->arg(FSPGM(keep, "keep")).toInt();
                if (keepTime) {
                    auto now = time(nullptr);
                    keepTime = (keepTime == 1 && IS_TIME_VALID(now)) ? now : keepTime; // check if the time was provied, otherwise use system time
                    if (IS_TIME_VALID(keepTime)) {
                        cookie.setExpires(keepTime + System::Device::getConfig().getWebUICookieLifetimeInSeconds());
                    }
                }
                __SID(debug_printf_P(PSTR("new SID cookie=%s\n"), cookie.getValue().c_str()));

                __LDBG_printf("Login successful: type=%u cookie=%s", getAuthTypeStr(authType), cookie.getValue().c_str());
                isAuthenticated = true;
                Logger_security(F("Login successful from %s (%s)"), remote_addr.toString().c_str(), getAuthTypeStr(authType));
            }
            else {
                loginError = FSPGM(Invalid_username_or_password, "Invalid username or password");
                if (_loginFailures) {
                    const FailureCounter &failure = _loginFailures->addFailure(remote_addr);
                    Logger_security(F("Login from %s failed %d times since %s (%s)"), remote_addr.toString().c_str(), failure.getCounter(), failure.getFirstFailure().c_str(), getAuthTypeStr(authType));
                }
                else {
                    Logger_security(F("Login from %s failed (%s)"), remote_addr.toString().c_str(), getAuthTypeStr(authType));
                }
                return _sendFile(FSPGM(_login_html, "/login.html"), String(), httpHeaders, client_accepts_gzip, isAuthenticated, request, __LDBG_new(LoginTemplate, loginError));
            }
        }
        else {
            if (path.endsWith(FSPGM(_html))) {
                httpHeaders.add(createRemoveSessionIdCookie());
                return _sendFile(FSPGM(_login_html), String(), httpHeaders, client_accepts_gzip, isAuthenticated, request, __LDBG_new(LoginTemplate, loginError));
            }
            else {
                request->send(403);
                return true;
            }
        }
    }

    if (isAuthenticated && request->method() == HTTP_POST) {  // http POST processing

        __LDBG_printf("HTTP post %s", path.c_str());

        httpHeaders.addNoCache(true);

        if (path.charAt(0) == '/' && path.endsWith(FSPGM(_html))) {
            // auto name = path.substring(1, path.length() - 5);

            __LDBG_printf("get_form=%s", formName.c_str());
            auto plugin = PluginComponent::getForm(formName);
            if (plugin) {
                __LDBG_printf("found=%p", plugin);
#if IOT_WEATHER_STATION
                __weatherStationDetachCanvas(true);
#endif
                FormUI::Form::BaseForm *form = __LDBG_new(SettingsForm, request);
                plugin->createConfigureForm(PluginComponent::FormCallbackType::CREATE_POST, formName, *form, request);
                webTemplate = __LDBG_new(ConfigTemplate, form);
                if (form->validate()) {
                    plugin->createConfigureForm(PluginComponent::FormCallbackType::SAVE, formName, *form, request);
                    System::Flags::getWriteableConfig().is_factory_settings = false;
                    config.write();
                    executeDelayed(request, [plugin, formName]() {
                        plugin->invokeReconfigure(formName);
                    });
                    WebTemplate::_aliveRedirection = path.substring(1);
                    mapping = FileMapping(FSPGM(applying_html), true);
                }
                else {
                    plugin->createConfigureForm(PluginComponent::FormCallbackType::DISCARD, formName, *form, request);
                    config.discard();
#if IOT_WEATHER_STATION
                    request->onDisconnect(__weatherStationAttachCanvas); // unlock on disconnect
#endif
                }
            }
            else { // no plugin found
                auto cPath = path.c_str() + 1;
                bool isRebootHtml = !strcmp_P(cPath, SPGM(reboot_html, "reboot.html"));
                bool isFactoryHtml = !isRebootHtml && !strcmp_P(cPath, SPGM(factory_html, "factory.html"));
                if (isRebootHtml || isFactoryHtml) {
                    if (request->hasArg(FSPGM(yes))) {
                        bool safeMode = false;
                        if (isRebootHtml) {
                            safeMode = request->arg(FSPGM(safe_mode)).toInt();
                        }
                        else if (isFactoryHtml) {
                            config.restoreFactorySettings();
                            config.write();
                            RTCMemoryManager::clear();
                        }
                        executeDelayed(request, [safeMode]() {
                            config.restartDevice(safeMode);
                        });
                        WebTemplate::_aliveRedirection = FSPGM(status_html);
                        mapping = FileMapping(FSPGM(rebooting_html), true);
                    }
                    else {
                        request->redirect(path);
                    }
                }
            }
        }
    }

    return _sendFile(mapping, formName, httpHeaders, client_accepts_gzip, isAuthenticated, request, webTemplate);
}

Plugin::Plugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(Plugin))
{
    REGISTER_PLUGIN(this, "WebServerPlugin");
}

void Plugin::setup(SetupModeType mode)
{
    begin();
    return;

    //TODO
    // crashes

    if (mode == SetupModeType::DELAYED_AUTO_WAKE_UP) {
        invokeReconfigureNow(getName());
    }
    else {
        begin();
    }
}

void Plugin::reconfigure(const String &source)
{
    HandlerStoragePtr storage;

    __LDBG_printf("server=%p source=%s", _server, source.c_str());
    if (_server) {

        // store handlers and web sockets during reconfigure
        storage.reset(new HandlerStorage());
        storage->_restCallbacks = std::move(_server->_restCallbacks);
        storage->moveHandlers(_server->_handlers);

        // end web server
        end();
#if MDNS_PLUGIN
        MDNSService::removeService(FSPGM(http, "http"), FSPGM(tcp));
        MDNSService::removeService(FSPGM(https, "https"), FSPGM(tcp));
#endif

    }

    // initialize web server
    begin(true);

    // if the server was disabled, the data of storage is released
    if (storage && _server) {
        // re-attach handlers from storage
        if (!storage->_restCallbacks.empty()) {
            for(auto &&restCallback: storage->_restCallbacks) {
                addRestHandler(std::move(restCallback));
            }
        }
        for(auto &handler: storage->_handlers) {
            _server->addHandler(handler.release());
        }
    }
}

void Plugin::shutdown()
{
    // auto mdns = PluginComponent::getPlugin<MDNSPlugin>(F("mdns"));
    // if (mdns) {
    //     mdns->shutdown();
    // }
    end();
}

void Plugin::getStatus(Print &output)
{
    output.print(F("Web server "));
    if (_server) {
        auto cfg = System::WebServer::getConfig();
        output.printf_P(PSTR("running on port %u"), cfg.getPort());
#if WEBSERVER_TLS_SUPPORT
        output.print(F(", TLS "));
        if (cfg.is_https) {
            output.print(FSPGM(enabled));
        } else {
            output.print(FSPGM(disabled));
        }
#endif
        size_t sockets = WsClient::_webSockets.size();
        size_t clients = 0;
        for(auto socket: WsClient::_webSockets) {
            clients += socket->count();
        }
        output.printf_P(PSTR(HTML_S(br) "%u WebSocket(s), %u client(s) connected"), sockets, clients);
        int count = _server->_restCallbacks.size();
        if (count) {
            output.printf_P(PSTR(HTML_S(br) "%d Rest API endpoints"), count);
        }
    }
    else {
        output.print(FSPGM(disabled));
    }
}

Plugin &Plugin::getInstance()
{
    return plugin;
}

AsyncWebServerEx *Plugin::getWebServerObject()
{
    return plugin._server.get();
}

bool Plugin::addHandler(AsyncWebHandler *handler)
{
    if (!plugin._server) {
        return false;
    }
    plugin._server->addHandler(handler);
    __LDBG_printf("handler=%p", handler);
    return true;
}

AsyncCallbackWebHandler *Plugin::addHandler(const String &uri, ArRequestHandlerFunction onRequest)
{
    if (!plugin._server) {
        return nullptr;
    }
    auto handler = new AsyncCallbackWebHandler();
    __LDBG_printf("handler=%p uri=%s", handler, uri.c_str());
    handler->setUri(uri);
    handler->onRequest(onRequest);
    plugin._server->addHandler(handler);
    return handler;
}

AuthType Plugin::getAuthenticated(AsyncWebServerRequest *request) const
{
    String SID;
    auto auth = request->getHeader(FSPGM(Authorization));
    if (auth) {
        auto &value = auth->value();
        __SID(__DBG_printf("auth SID=%s remote=%s", value.c_str(), request->client()->remoteIP().toString().c_str()));
        if (String_startsWith(value, FSPGM(Bearer_))) {
            auto token = value.c_str() + 7;
            const auto len = value.length() - 7;
            __SID(__DBG_printf("token=%s device_token=%s len=%u", token, System::Device::getToken(), len));
            if (len >= System::Device::kTokenMinSize && !strcmp(token, System::Device::getToken())) {
                __SID(__DBG_print("valid BEARER token"));
                return AuthType::BEARER;
            }
        }
        __SID(__DBG_print("Authorization header failed"));
    }
    else {
        auto isRequestSID = request->hasArg(FSPGM(SID));
        if ((isRequestSID && (SID = request->arg(FSPGM(SID)))) || HttpCookieHeader::parseCookie(request, FSPGM(SID), SID)) {
            __SID(__DBG_printf("SID=%s remote=%s type=%s", SID.c_str(), request->client()->remoteIP().toString().c_str(), request->methodToString()));
            if (SID.length() == 0) {
                return AuthType::NONE;
            }
            if (verify_session_id(SID.c_str(), System::Device::getUsername(), System::Device::getPassword())) {
                __SID(__DBG_printf("valid SID=%s type=%s", SID.c_str(), isRequestSID ? request->methodToString() : FSPGM(cookie, "cookie")));
                return isRequestSID ? AuthType::SID : AuthType::SID_COOKIE;
            }
            __SID(__DBG_printf("invalid SID=%s", SID.c_str()));
        }
        else {
            __SID(__DBG_print("no SID"));
        }
    }
    return AuthType::NONE;
}


// ------------------------------------------------------------------------
// RestRequest
// ------------------------------------------------------------------------

RestHandler::RestHandler(const __FlashStringHelper *url, Callback callback) : _url(url), _callback(callback)
{

}

const __FlashStringHelper *RestHandler::getURL() const
{
    return _url;
}

AsyncWebServerResponse *RestHandler::invokeCallback(AsyncWebServerRequest *request, RestRequest &rest)
{
    return _callback(request, rest);
}

// ------------------------------------------------------------------------
// RestRequest
// ------------------------------------------------------------------------

RestRequest::RestRequest(AsyncWebServerRequest *request, const RestHandler &handler, AuthType auth) :
    _request(request),
    _handler(handler),
    _auth(auth),
    _stream(),
    _reader(_stream),
    _readerError(false)
{
    _reader.initParser();
}

AuthType RestRequest::getAuth() const
{
    return _auth;
}

AsyncWebServerResponse *RestRequest::createResponse(AsyncWebServerRequest *request)
{
    AsyncWebServerResponse *response = nullptr;
    // respond with a json message and status 200
    if (!(request->method() & WebRequestMethod::HTTP_POST)) {
        response = request->beginResponse(200, FSPGM(mime_application_json), PrintString(F("{\"status\":%u,\"message\":\"%s\"}"), 405, AsyncWebServerResponse::responseCodeToString(405)));
    }
    else if (_auth == false) {
        response = request->beginResponse(200, FSPGM(mime_application_json), PrintString(F("{\"status\":%u,\"message\":\"%s\"}"), 401, AsyncWebServerResponse::responseCodeToString(401)));
    }
    else if (_readerError) {
        response = request->beginResponse(200, FSPGM(mime_application_json), PrintString(F("{\"status\":%u,\"message\":\"%s\"}"), 400, AsyncWebServerResponse::responseCodeToString(400)));
    }
    else {
        return getHandler().invokeCallback(request, *this);
    }
    // add no cache and no store to any 200 response
    HttpHeaders headers;
    headers.addNoCache(true);
    headers.setAsyncWebServerResponseHeaders(response);
    return response;
}

RestHandler &RestRequest::getHandler()
{
    return _handler;
}

JsonMapReader &RestRequest::getJsonReader()
{
    return _reader;
}

void RestRequest::writeBody(uint8_t *data, size_t len)
{
    if (!_readerError) {
        _stream.setData(data, len);
        if (!_reader.parseStream()) {
            __LDBG_printf("json parser: %s", _reader.getLastErrorMessage().c_str());
            _readerError = true;
        }
    }
}

bool RestRequest::isUriMatch(const __FlashStringHelper *uri) const
{
    if (!uri) {
        return false;
    }
    String baseUrl = _handler.getURL();
    append_slash(baseUrl);
    baseUrl += uri;

    return _request->url().equals(baseUrl);
}


// ------------------------------------------------------------------------
// AsyncRestWebHandler
// ------------------------------------------------------------------------

bool AsyncRestWebHandler::canHandle(AsyncWebServerRequest *request)
{
    __LDBG_printf("url=%s auth=%d", request->url().c_str(), Plugin::getInstance().isAuthenticated(request));

    auto &url = request->url();
    for(const auto &handler: plugin._server->_restCallbacks) {
        if (url == FPSTR(handler.getURL())) {
            request->addInterestingHeader(FSPGM(Authorization));

            // emulate AsyncWebServerRequest dtor using onDisconnect callback
            request->_tempObject = new RestRequest(request, handler, Plugin::getInstance().getAuthenticated(request));
            request->onDisconnect([this, request]() {
                if (request->_tempObject) {
                    delete reinterpret_cast<RestRequest *>(request->_tempObject);
                    request->_tempObject = nullptr;
                }
            });
            return true;
        }
    }
    return false;
}

void AsyncRestWebHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    __LDBG_printf("url=%s data='%*.*s' idx=%u len=%u total=%u temp=%p", request->url().c_str(), len, len, data, index, len, total, request->_tempObject);
    if (request->_tempObject) {
        auto &rest = *reinterpret_cast<RestRequest *>(request->_tempObject);
        // wondering why enum class == true compiles? -> global custom operator
        if (rest.getAuth() == true) {
            rest.writeBody(data, len);
        }
    }
    else {
        request->send(500);
    }
}

void AsyncRestWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    __LDBG_printf("url=%s json temp=%p is_post_metod=%u", request->url().c_str(), request->_tempObject, (request->method() & WebRequestMethod::HTTP_POST));
    if (request->_tempObject) {
        auto &rest = *reinterpret_cast<RestRequest *>(request->_tempObject);
#if DEBUG_WEB_SERVER
        rest.getJsonReader().dump(DEBUG_OUTPUT);
#endif
        request->send(rest.createResponse(request));
    }
    else {
        request->send(500);
    }
}

// ------------------------------------------------------------------------
// AsyncUpdateWebHandler
// ------------------------------------------------------------------------

bool AsyncUpdateWebHandler::canHandle(AsyncWebServerRequest *request)
{
    if (!(request->method() & WebRequestMethod::HTTP_POST)) {
        return false;
    }
    if (request->url() != F("/upload")) {
        return false;
    }
    // emulate AsyncWebServerRequest dtor using onDisconnect callback
    request->_tempObject = new UploadStatus();
    request->onDisconnect([this, request]() {
        if (request->_tempObject) {
            delete reinterpret_cast<UploadStatus *>(request->_tempObject);
            request->_tempObject = nullptr;
        }
    });
    return true;
}

void AsyncUpdateWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    if (!Plugin::getInstance().isAuthenticated(request)) {
        return request->send(403);
    }

    auto status = reinterpret_cast<UploadStatus *>(request->_tempObject);
    if (!status) {
        return request->send(500);
    }

    PrintString errorStr;
    AsyncWebServerResponse *response = nullptr;
#if STK500V1
    if (status->command == U_ATMEGA) {
        if (!status->tempFile || !status->tempFile.fullName()) {
            errorStr = F("Failed to read temporary file");
            goto errorResponse;
        }
        // get filename and close the file
        String filename = status->tempFile.fullName();
        status->tempFile.close();

        // check if singleton exists
        if (stk500v1) {
            Logger::error(F("ATmega firmware upgrade already running"));
            request->send_P(200, FSPGM(mime_text_plain), PSTR("Upgrade already running"));
        }
        else {
            Logger_security(F("Starting ATmega firmware upgrade..."));

            stk500v1 = __LDBG_new(STK500v1Programmer, Serial);
            stk500v1->setSignature_P(PSTR("\x1e\x95\x0f"));
            stk500v1->setFile(filename);
            stk500v1->setLogging(STK500v1Programmer::LOG_FILE);

            // give it 3.5 seconds to upload the file (a few KB max)
            _Scheduler.add(3500, false, [filename](Event::CallbackTimerPtr timer) {
                if (stk500v1) {
                    // start update
                    stk500v1->begin([]() {
                        __LDBG_free(stk500v1);
                        stk500v1 = nullptr;
                        // remove temporary file
                        KFCFS::unlink(filename);
                    });
                }
                else {
                    // write something to the logfile
                    Logger::error(F("Cannot start ATmega firmware upgrade"));
                    // remove temporary file
                    KFCFS::unlink(filename);
                }
            });

            response = request->beginResponse(302);
            HttpHeaders httpHeaders(false);
            httpHeaders.add<HttpLocationHeader>(String('/') + FSPGM(serial_console_html));
            httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::CLOSE);
            httpHeaders.setAsyncWebServerResponseHeaders(response);
            request->send(response);
        }
    }
    else
#endif
    if (Update.hasError()) {
        // TODO check if we need to restart the file system
        // KFCFS.begin();

        Update.printError(errorStr);
#if STK500V1
errorResponse: ;
#endif
        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
        Logger_error(F("Firmware upgrade failed: %s"), errorStr.c_str());

        String message = F("<h2>Firmware update failed with an error:<br></h2><h3>");
        message += errorStr;
        message += F("</h3>");

        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        if (!plugin._sendFile(String('/') + FSPGM(update_fw_html), String(), httpHeaders, plugin._clientAcceptsGzip(request), true, request, __LDBG_new(UpgradeTemplate, message))) {
            // fallback if the file system cannot be read anymore
            message += F("<br><a href=\"/\">Home</a>");
            request->send(200, FSPGM(mime_text_html), message);
        }
    }
    else {
        Logger_security(F("Firmware upgrade successful"));

#if IOT_CLOCK
        // turn dispaly off since the update will take a few seconds and freeze the clock
        ClockPlugin::getInstance()._setBrightness(0);
#endif

        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SLOW);
        Logger_notice(F("Rebooting after upgrade"));

        String location;
        switch(status->command) {
            case U_FLASH: {
#if DEBUG
                auto hash = request->arg(F("elf_hash"));
                __LDBG_printf("hash %u %s", hash.length(), hash.c_str());
                if (hash.length() == System::Firmware::getElfHashHexSize()) {
                    System::Firmware::setElfHashHex(hash.c_str());
                    config.write();
                }
#endif
                WebTemplate::_aliveRedirection = String(FSPGM(update_fw_html, "update-fw.html")) + F("#u_flash");
                location += '/';
                location += FSPGM(rebooting_html);
            } break;
            case U_SPIFFS:
                WebTemplate::_aliveRedirection = String(FSPGM(update_fw_html)) + F("#u_spiffs");
                location += '/';
                location += FSPGM(rebooting_html);
                break;
        }

        // executeDelayed sets a new ondisconnect callback
        // delete upload object before
        if (status) {
            delete status;
            request->_tempObject = nullptr;
            status = nullptr;
        }

        // execute restart with a delay to finish the current request
        executeDelayed(request, []() {
            config.restartDevice();
        });

        response = request->beginResponse(302);
        HttpHeaders httpHeaders(false);
        httpHeaders.add<HttpLocationHeader>(location);
        httpHeaders.replace<HttpConnectionHeader>(HttpConnectionHeader::CLOSE);
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
}

void AsyncUpdateWebHandler::handleUpload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!Plugin::getInstance().isAuthenticated(request)) {
        return request->send(403);
    }
    if (!request->_tempObject) {
        return request->send(500);
    }

    UploadStatus *status = reinterpret_cast<UploadStatus *>(request->_tempObject);
    if (index == 0) {
        Logger_notice(F("Firmware upload started"));
#if IOT_REMOTE_CONTROL
        RemoteControlPlugin::prepareForUpdate();
#endif
    }

    if (plugin._updateFirmwareCallback) {
        plugin._updateFirmwareCallback(index + len, request->contentLength());
    }

    if (status && !status->error) {
        PrintString out;
        if (!index) {
            BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);

            size_t size;
            uint8_t command;
            uint8_t imageType = 0;

            if (request->arg(FSPGM(image_type)) == F("u_flash")) { // firmware selected
                imageType = 0;
            }
            else if (request->arg(FSPGM(image_type)) == F("u_spiffs")) { // spiffs selected
                imageType = 1;
            }
#if STK500V1
            else if (request->arg(FSPGM(image_type)) == F("u_atmega")) { // atmega selected
                imageType = 3;
            }
            else if (filename.indexOf(F(".hex")) != -1) { // auto select
                imageType = 3;
            }
#endif
            else if (filename.indexOf(F("spiffs")) != -1) { // auto select
                imageType = 2;
            }

#if STK500V1
            if (imageType == 3) {
                status->command = U_ATMEGA;
                status->tempFile = KFCFS.open(FSPGM(stk500v1_tmp_file), fs::FileOpenMode::write);
                __LDBG_printf("ATmega fw temp file %u, filename %s", (bool)status->tempFile, String(FSPGM(stk500v1_tmp_file)).c_str());
            }
            else
#endif
            {
                if (imageType) {
#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
                    size = ((size_t) &_FS_end - (size_t) &_FS_start);
#else
                    size = 1048576;
#endif
                    command = U_SPIFFS;
                } else {
                    size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                    command = U_FLASH;
                }
                status->command = command;
                debug_printf_P(PSTR("Update Start: %s, image type %d, size %d, command %d\n"), filename.c_str(), imageType, (int)size, command);

#if defined(ESP8266)
                Update.runAsync(true);
#endif

                if (!Update.begin(size, command)) {
                    status->error = true;
                }
            }
        }
#if STK500V1
        if (status->command == U_ATMEGA) {
            if (!status->tempFile) {
                status->error = true;
            }
            else if (status->tempFile.write(data, len) != len) {
                status->error = true;
            }
#if DEBUG_WEB_SERVER
            if (final) {
                if (status->tempFile) {
                    __DBG_printf("upload success: %uB", status->tempFile.size());
                else {
                    __DBG_printf("upload error: tempFile = false");
                }
            }
#endif
        }
        else
#endif
        {
            if (!status->error && !Update.hasError()) {
                if (Update.write(data, len) != len) {
                    status->error = true;
                }
            }

            __DBG_printf("is_finished=%u is_running=%u error=%u progress=%u remaining=%u size=%u", Update.isFinished(), Update.isRunning(), Update.getError(), Update.progress(), Update.remaining(), Update.size());

            if (final) {
                if (Update.end(true)) {
                    __LDBG_printf("update success: %uB", index + len);
                } else {
                    status->error = true;
                }
            }
            if (status->error) {
#if DEBUG
                // print error to debug output
                Update.printError(DEBUG_OUTPUT);
#endif
            }
        }
    }
}


#endif
