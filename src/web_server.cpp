/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#if ENABLE_ARDUINO_OTA
#    include <ArduinoOTA.h>
#endif
#include "../include/templates.h"
#include "../src/plugins/plugins.h"
#include "JsonTools.h"
#include "WebUISocket.h"
#include "async_web_handler.h"
#include "async_web_response.h"
#include "blink_led_timer.h"
#include "build.h"
#include "failure_counter.h"
#include "fs_mapping.h"
#include "kfc_fw_config.h"
#include "rest_api.h"
#include "save_crash.h"
#include "session.h"
#include "templates.h"
#include "web_server.h"
#include "web_server_action.h"
#include "web_socket.h"
#include <BufferStream.h>
#include <ESPAsyncWebServer.h>
#include <EventScheduler.h>
#include <JsonConfigReader.h>
#include <PrintHtmlEntitiesString.h>
#include <PrintString.h>
#if IOT_BLINDS_CTRL && IOT_BLINDS_CTRL_SAVE_STATE
#    include "../src/plugins/blinds_ctrl/blinds_plugin.h"
#endif
#if IOT_CLOCK
#    include "../src/plugins/clock/clock.h"
#endif
#if IOT_WEATHER_STATION
#    include "../src/plugins/weather_station/WeatherStationBase.h"
#endif
#include "spgm_auto_def.h"

#if DEBUG_WEB_SERVER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#define DEBUG_WEB_SERVER_SID                DEBUG_WEB_SERVER

#if DEBUG_WEB_SERVER_SID
#define __SID(...)                          __VA_ARGS__
#else
#define __SID(...)                          ;
#endif

#if 0
#    include <debug_helper_disable_ptr_validation.h>
#endif

namespace WebServer {
    Plugin webServerPlugin;
}

using KFCConfigurationClasses::System;
using namespace WebServer;
using namespace KFCJson;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Plugin,
    "http",             // name
    "Web Server",       // friendly name
    "",                 // web_templates
    "remote",           // config_forms
    "network,mdns",     // reconfigure_dependencies
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

HttpCookieHeader *createRemoveSessionIdCookie()
{
    return new HttpCookieHeader(FSPGM(SID, "SID"), String(), String('/'), HttpCookieHeader::COOKIE_EXPIRED);
}

static inline const __FlashStringHelper *_getContentType(const String &path)
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

const __FlashStringHelper *getContentType(const String &path)
{
    __LDBG_printf("getContentType(%s)=%s", __S(path), __S(_getContentType(path)));
    return _getContentType(path);
}

void Plugin::executeDelayed(AsyncWebServerRequest *request, std::function<void()> callback)
{
    __LDBG_printf("executeDelayed url=%s callback=%p", request->url().c_str(), std::addressof(callback));
    request->onDisconnect([callback]() {
        _Scheduler.add(2000, false, [callback](Event::CallbackTimerPtr timer) {
            callback();
        });
    });
}

bool Plugin::_isPublic(const String &pathString) const
{
    auto path = pathString.c_str();
    if (strstr_P(path, PSTR(".."))) { // deny any path traversing attempts
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

// 404 handler
// to avoid having multiple handlers and save RAM, all custom handlers are executed here
void Plugin::handlerNotFound(AsyncWebServerRequest *request)
{
    __LDBG_printf("not found handler=%s", request->url().c_str());
    HttpHeaders headers;
    AsyncWebServerResponse *response = nullptr;

    // StringVector list;
    // for(const auto header: request->getHeaders()) {
    //     list.emplace_back(std::move(header->toString()));
    // }
    // __LDBG_printf("headers for %s:\n%s", request->url().c_str(), implode('\n', list).c_str());

    auto &url = request->url();
    // --------------------------------------------------------------------
    #if ENABLE_ARDUINO_OTA
        if (url.endsWith(F("-arduino-ota"))) {
            for(;;) {
                auto statusStr = F("Enabled");
                if (url.startsWith(F("/start-"))) {
                    getInstance().ArduinoOTAbegin();
                }
                else if (url.startsWith(F("/stop-"))) {
                    statusStr = F("Diabled");
                    getInstance().ArduinoOTAend();
                }
                else {
                    break;
                }
                response = request->beginResponse(200, FSPGM(mime_text_plain), statusStr);
                headers.addNoCache(true);
                headers.setResponseHeaders(response);
                break;
            }
        }
        else
    #endif
    // --------------------------------------------------------------------
    if (url == F("/is-alive")) {
        response = request->beginResponse(200, FSPGM(mime_text_plain), std::move(String(request->arg(String('p')).toInt())));
        headers.addNoCache(true);
        headers.setResponseHeaders(response);
    }
    #if IOT_CLOCK && 0 //TODO implement WLED json API
        // --------------------------------------------------------------------
        else if (url == F("/json")) {
            auto json = ClockPlugin::getInstance().getWLEDJson();
            response = request->beginResponse(200, FSPGM(mime_application_json), json.c_str());
            headers.addNoCache(true);
            headers.setResponseHeaders(response);
        }
        // --------------------------------------------------------------------
        else if (url == F("/presets.json")) {
            response = request->beginResponse(200, FSPGM(mime_application_json), F("{}"));
            headers.addNoCache(true);
            headers.setResponseHeaders(response);
        }
    #endif
    // --------------------------------------------------------------------
    else if (url == F("/webui-handler")) {
        getInstance()._handlerWebUI(request, headers);
        return;
    }
    // --------------------------------------------------------------------
    else if (url == F("/alerts")) {
        getInstance()._handlerAlerts(request, headers);
        return;
    }
    // --------------------------------------------------------------------
    else if (url == F("/sync-time")) {
        if (!getInstance().isAuthenticated(request)) {
            auto response = request->beginResponse(403);
            _logRequest(request, response);
            request->send(response);
            return;
        }
        headers.addNoCache(true);
        PrintHtmlEntitiesString str;
        WebTemplate::printSystemTime(time(nullptr), str);
        response = new AsyncBasicResponse(200, FSPGM(mime_text_html), std::move(str));
        if (!response) {
            __LDBG_printf_E("memory allocation failed");
        }
        headers.setResponseHeaders(response);
    }
    // --------------------------------------------------------------------
    else if (url == F("/export-settings")) {
        getInstance()._handlerExportSettings(request, headers);
        return;
    }
    // --------------------------------------------------------------------
    else if (url == F("/import-settings")) {
        getInstance()._handlerImportSettings(request, headers);
        return;
    }
    // --------------------------------------------------------------------
    else if (url == F("/scan-wifi")) {
        if (!getInstance().isAuthenticated(request)) {
            auto response = request->beginResponse(403, FSPGM(mime_text_html));
            _logRequest(request, response);
            request->send(response);
            return;
        }
        headers.addNoCache();
        response = new AsyncNetworkScanResponse(request->arg(FSPGM(hidden, "hidden")).toInt());
        if (!response) {
            __LDBG_printf_E("memory allocation failed");
        }
        headers.setResponseHeaders(response);
    }
    // --------------------------------------------------------------------
    else if (url == F("/logout")) {
        __SID(__DBG_printf("sending remove SID cookie"));
        headers.addNoCache(true);
        headers.add(createRemoveSessionIdCookie());

        // headers.add<HttpLocationHeader>(String('/'));
        // response = request->beginResponse(302);
        // headers.setResponseHeaders(response);

        response = HttpLocationHeader::redir(request, String('/'), headers);
    }
    // --------------------------------------------------------------------
    else if (url == F("/mqtt-publish-ad.html")) {
        if (!getInstance().isAuthenticated(request)) {
            auto response = request->beginResponse(403, FSPGM(mime_text_html));
            _logRequest(request, response);
            request->send(response);
            return;
        }

        auto &session = Action::Handler::getInstance().initSession(request, F("/mqtt-publish-ad.html"), F("MQTT Auto Discovery"), Action::AuthType::AUTH);
        if (session.isNew()) {
            auto client = MQTT::Client::getClient();
            bool result = false;
            if (client) {
                auto sessionId = session.getId();
                result = client->publishAutoDiscovery(MQTT::RunFlags::FORCE, [sessionId](MQTT::StatusType status) {
                    const auto session = Action::Handler::getInstance().getSession(sessionId);
                    if (session) {
                        switch(status) {
                            case MQTT::StatusType::DEFERRED:
                                session->setStatus(F("Auto discovery deferred..."));
                                break;
                            case MQTT::StatusType::STARTED:
                                session->setStatus(F("Auto discovery running..." MESSAGE_TEMPLATE_AUTO_RELOAD(15)));
                                break;
                            case MQTT::StatusType::SUCCESS:
                                session->setStatus(F("Auto discovery successfully published..."), MessageType::SUCCESS);
                                break;
                            case MQTT::StatusType::FAILURE:
                                session->setStatus(F("Failed to publish auto discovery..."), MessageType::DANGER);
                                break;
                        }
                    }
                });
            }
            if (result) {
                session = Action::StateType::EXECUTING;
                session.setStatus(F("Starting auto discovery..." MESSAGE_TEMPLATE_AUTO_RELOAD(15)));
            }
            else {
                session = Action::StateType::FINISHED;
                session.setStatus(F("Failed to publish auto discovery..."), MessageType::DANGER);
            }
        }
        return;
    }
    // --------------------------------------------------------------------
    else if (url == F("/zeroconf")) {
        if (!getInstance().isAuthenticated(request)) {
            auto response = request->beginResponse(403);
            _logRequest(request, response);
            request->send(response);
            return;
        }
        headers.addNoCache(true);
        response = new AsyncResolveZeroconfResponse(request->arg(FSPGM(value)));
        if (!response) {
            __LDBG_printf_E("memory allocation failed");
        }
        headers.setResponseHeaders(response);
    }
    #if MDNS_PLUGIN
        else if (url == F("/mdns_discovery")) {
            MDNSPlugin::mdnsDiscoveryHandler(request);
            return;
        }
    #endif
    // --------------------------------------------------------------------
    #if ESP8266
        else if (url == F("/savecrash.json")) {
            if (!getInstance().isAuthenticated(request)) {
                auto response = request->beginResponse(403);
                _logRequest(request, response);
                request->send(response);
                return;
            }
            headers.addNoCache(true);
            response = SaveCrash::webHandler::json(request, headers);
        }
    #endif
    #if IOT_SENSOR_HAVE_AMBIENT_LIGHT_SENSOR
        // --------------------------------------------------------------------
        else if (url == F("/ambient_light_sensor")) {
            if (!getInstance().isAuthenticated(request)) {
                auto response = request->beginResponse(403);
                _logRequest(request, response);
                request->send(response);
                return;
            }
            int id = request->arg(F("id")).toInt();
            Sensor_AmbientLight *lightSensor = nullptr;
            for(const auto &sensor: SensorPlugin::getSensors()) {
                auto tmpSensor = reinterpret_cast<Sensor_AmbientLight *>(sensor);
                if (sensor->getType() == SensorPlugin::SensorType::AMBIENT_LIGHT && tmpSensor->getId() == id && tmpSensor->enabled()) {
                    lightSensor = reinterpret_cast<Sensor_AmbientLight *>(sensor);
                    break;
                }
            }
            if (!lightSensor || lightSensor->getValue() == -1) {
                auto response = request->beginResponse(503);
                _logRequest(request, response);
                request->send(response);
                return;
            }
            headers.addNoCache(true);
            response = request->beginResponse(200, FSPGM(mime_text_plain), std::move(String(lightSensor->getValue())));
            headers.setResponseHeaders(response);
        }
    #endif
    // --------------------------------------------------------------------
    #if WEBSERVER_SPEED_TEST
        else if (url.startsWith(F("/speedtest."))) { // handles speedtest.zip and speedtest.bmp
            getInstance()._handlerSpeedTest(request, !url.endsWith(F(".bmp")), headers);
            return;
        }
    #endif

    // handle response
    if (response) {
        _logRequest(request, response);
        request->send(response);
        return;
    }

    // else section
    if (getInstance()._handleFileRead(url, getInstance()._clientAcceptsGzip(request), request, headers)) {
        return;
    }
    send(404, request);
}

bool Plugin::isHtmlContentType(AsyncWebServerRequest *request, HttpHeaders *headers)
{
    __LDBG_printf("isHtmlContentType(%s)", __S(request->url()));
    if (headers) {
        // check if the output is html
        auto header = headers->find(F("Content-Type"));
        if (header) {
            if (header->getValue().indexOf(F("text/html")) == -1) {
                __LDBG_printf("content-type %s", header->getValue().c_str());
                return false;
            }
        }
    }
    // ignore these user agents
    auto &userAgent = request->header(F("User-Agent"));
    if (userAgent.startsWith(F("KFCFW OTA"))) {
        __LDBG_printf("user-agent %s", userAgent.c_str());
        return false;
    }
    auto &url = request->url();
    if (!url.endsWith(F(".html")) && !url.endsWith(F(".htm"))) {
        auto dot = url.indexOf('.', url.lastIndexOf('/') + 1);
        if (dot != -1) {
            // filename has an extension
            __LDBG_printf("url %s", url.c_str());
            return false;
        }
    }
    auto &requestedWith = request->header(F("X-Requested-With")) ;
    if (requestedWith.equalsIgnoreCase(F("XMLHttpRequest"))) {
        // ajax request
        __LDBG_printf("ajax request %s", url.c_str());
        return false;
    }
    auto &accept = request->header(F("Accept"));
    if (accept.length() == 0) {
        __LDBG_printf("accept header missing");
        return false;
    }
    if (accept.indexOf(F("text/html")) == -1) {
        // most clients accept html even for pictures, but not in this case
        __LDBG_printf("accept %s", accept.c_str());
        return false;
    }
    __LDBG_printf("return true");
    return true;
}

bool Plugin::sendFileResponse(uint16_t code, const String &path, AsyncWebServerRequest *request, HttpHeaders &headers, WebTemplate *webTemplate)
{
    auto mapping = FileMapping(path);
    if (mapping.exists()) {
        if (!webTemplate) {
            webTemplate = new WebTemplate(isAuthenticated(request) ? WebTemplate::AuthType::AUTH : WebTemplate::AuthType::NO_AUTH);
            if (!webTemplate) {
                __LDBG_printf_E("memory allocation failed");
            }
        }
        if (!webTemplate->isAuthenticationSet()) {
            __LDBG_printf("not authenticated %s", path.c_str());
        }
        AsyncWebServerResponse *response = new AsyncTemplateResponse(FSPGM(mime_text_html), mapping.open(FileOpenMode::read), webTemplate, [webTemplate](const String& name, DataProviderInterface &provider) {
            return TemplateDataProvider::callback(name, provider, *webTemplate);
        });
        if (!response) {
            __LDBG_printf("memory allocation failed");
            response = request->beginResponse(503);
        }
        if (code) {
            response->setCode(code);
        }
        headers.setResponseHeaders(response);
        _logRequest(request, response);
        request->send(response);
        return true;
    }
    if (webTemplate) {
        delete webTemplate;
    }
    return false;
}

void Plugin::message(AsyncWebServerRequest *request, MessageType type, const String &message, const String &title, HttpHeaders &headers)
{
    auto webTemplate = new MessageTemplate(message, title, isAuthenticated(request));
    if (!webTemplate) {
        __LDBG_printf("memory allocation failed");
    }
    switch(type) {
        case MessageType::DANGER:
            webTemplate->setTitleClass(F("text-white bg-danger"));
            break;
        case MessageType::WARNING:
            webTemplate->setTitleClass(F("text-dark bg-warning"));
            break;
        case MessageType::INFO:
            webTemplate->setTitleClass(F("text-white bg-info"));
            break;
        case MessageType::SUCCESS:
            webTemplate->setTitleClass(F("text-white bg-success"));
            break;
        default:
        case MessageType::PRIMARY:
            webTemplate->setTitleClass(F("text-white bg-primary"));
            break;
    }
    if (sendFileResponse(200, F("/.message.html"), request, headers, webTemplate)) {
        return;
    }
    auto response = request->beginResponse(200, FSPGM(mime_text_plain), title + '\n' + message + F("\n\n(File system read error occurred during request)"));
    _logRequest(request, response);
    request->send(response);
}

void Plugin::send(uint16_t httpCode, AsyncWebServerRequest *request, const String &message)
{
    HttpHeaders headers;
    headers.addNoCache(true);

    __LDBG_printf("send httpcode=%u request_uri=%s message=%s content_type_html=%u", httpCode, request->url().c_str(), message.c_str(), isHtmlContentType(request));
    if (isHtmlContentType(request)) {
        NotFoundTemplate *webTemplate;
        if (message.length() == 0) {
            webTemplate = new NotFoundTemplate(httpCode, PrintString(F("URL: <i>%s</i> - <strong>%s</strong>"), request->url().c_str(), AsyncWebServerResponse::responseCodeToString(httpCode)));
        }
        else {
            webTemplate = new NotFoundTemplate(httpCode, message);
        }
        if (!webTemplate) {
            __LDBG_printf("memory allocation failed");
        }
        webTemplate->setAuthenticated(isAuthenticated(request));
        if (sendFileResponse(httpCode, F("/.message.html"), request, headers, webTemplate)) {
            return;
        }
    }

    auto response = request->beginResponse(httpCode);
    headers.setResponseHeaders(response);
    _logRequest(request, response);
    request->send(response);
}

static String _getUrlAndPostDataFromRequest(AsyncWebServerRequest *request)
{
    auto url = PrintString(urlEncode(request->url(), F("\r\n\" ")));
    if (request->args()) {
        for(size_t i = 0; i < request->args(); i++) {
            auto param = request->getParam(i);
            url.printf_P(PSTR("%c%s=%s"), (i == 0 ? '?' : '&'), param->name().c_str(), urlEncode(param->value(), F("\r\n\" ?=")).c_str());
        }
    }
    return url;
}

#if WEBSERVER_LOG_SERIAL

    void Plugin::_logRequest(AsyncWebServerRequest *request, AsyncWebServerResponse *response)
    {
        __LDBG_printf("_logRequest(%s)", __S(request->url()));
        PrintString log;
        IPAddress(request->client()->getRemoteAddress()).printTo(log);
        log.strftime_P(PSTR(" - - [%m/%d/%Y %H:%M:%S] \""), time(nullptr));
        log.print(request->methodToString());
        log.print(' ');
        log.print(_getUrlAndPostDataFromRequest(request));
        if (response) {
            auto contentLength = response->getContentLength();
            auto contentLengthStr = contentLength ? String(contentLength) : String('-');
            log.printf_P(PSTR(" HTTP/1.%u\" %d %s \"%s\""), request->version(), response->getCode(), contentLengthStr.c_str(), response->getContentType().c_str());
        }
        else {
            log.printf_P(PSTR("\" failed"));
        }
        Serial.println(log);
    }

#endif

void Plugin::_handlerWebUI(AsyncWebServerRequest *request, HttpHeaders &headers)
{
    if (!getInstance().isAuthenticated(request)) {
        send(403, request);
        return;
    }
    // 503 if the webui is disabled
    if (!System::Flags::getConfig().is_webui_enabled) {
        send(503, request);
        return;
    }
    auto json = WebUISocket::createWebUIJSON();
    String &jsonStr = json.toString();
    // __LDBG_printf("_handlerWebUI %u", jsonStr.length());
    auto response = new AsyncBasicResponse(200, FSPGM(mime_application_json), std::move(jsonStr)); //TODO use stream or fillbuffer
    if (!response) {
        __LDBG_printf("memory allocation failed");
    }
    headers.addNoCache();
    headers.setResponseHeaders(response);
    _logRequest(request, response);
    request->send(response);
}

void Plugin::_handlerAlerts(AsyncWebServerRequest *request, HttpHeaders &headers)
{
    // 503 response if disabled (no support compiled in)
    auto response = request->beginResponse(503);
    _logRequest(request, response);
    request->send(response);
}

void Plugin::_addRestHandler(RestHandler &&handler)
{
    if (_server) {
        // install rest handler
        __LDBG_printf("installing REST handler url=%s", handler.getURL());
        if (_server->_restCallbacks.empty()) {
            // add AsyncRestWebHandler to web server
            // the object gets destroyed with the web server and requires to keep the list off callbacks separated
            auto restHandler = new AsyncRestWebHandler();
            if (!restHandler) {
                __LDBG_printf("memory allocation failed");
            }
            __LDBG_printf("handler=%p", restHandler);
            _server->addHandler(restHandler);
        }
        // store handler
        _server->_restCallbacks.emplace_back(std::move(handler));
    }
}

#if WEBSERVER_SPEED_TEST

    void Plugin::_handlerSpeedTest(AsyncWebServerRequest *request, bool zip, HttpHeaders &headers)
    {
        #if !defined(SPEED_TEST_NO_AUTH) || SPEED_TEST_NO_AUTH == 0
            if (!isAuthenticated(request)) {
                send(403, request);
                return;
            }
        #endif
        headers.clear(2); // remove default headers and reserve 3 headers
        headers.addNoCache(); // adds 2 headers

        AsyncSpeedTestResponse *response;
        auto size = std::max(1024 * 64, (int)request->arg(FSPGM(size, "size")).toInt());
        if (zip) {
            response = new AsyncSpeedTestResponse(FSPGM(mime_application_zip), size);
            if (!response) {
                __LDBG_printf("memory allocation failed");
            }
            headers.add<HttpDispositionHeader>(F("speedtest.zip"));
        } else {
            response = new AsyncSpeedTestResponse(FSPGM(mime_image_bmp), size);
            if (!response) {
                __LDBG_printf("memory allocation failed");
            }
        }
        _logRequest(request, response);
        request->send(response);
    }

#endif

void Plugin::_handlerImportSettings(AsyncWebServerRequest *request, HttpHeaders &headers)
{
    if (!isAuthenticated(request)) {
        send(403, request);
        return;
    }

    headers.clear();
    headers.addNoCache();

    int count = -1;
    uint16_t status;
    const __FlashStringHelper *message;

    auto configArg = FSPGM(config, "config");
    if ((request->method() & HTTP_POST) && request->hasArg(configArg)) {
        BufferStream data(request->arg(configArg));
        config.discard();

        JsonConfigReader reader(&data, config, nullptr);
        reader.initParser();
        bool result = reader.parseStream();
        if (result) {
            config.write();

            count = reader.getImportedHandles().size();
            message = FSPGM(Success, "Success");
            status = 200;
        }
        else {
            message = F("Failed to parse JSON data");
            status = 400;
        }
    }
    else {
        message = AsyncWebServerResponse::responseCodeToString(405);
        status = 405;
    }
    send(request, request->beginResponse(200, FSPGM(mime_text_plain), PrintString(F("{\"status\":%u,\"count\":%d,\"message\":\"%s\"}"), status, count, message)));
}

void Plugin::_handlerExportSettings(AsyncWebServerRequest *request, HttpHeaders &headers)
{
    if (!isAuthenticated(request)) {
        send(403, request);
        return;
    }
    headers.clear();
    headers.addNoCache();

    auto hostname = System::Device::getName();

    PrintString filename(F("kfcfw_config_%s_b" __BUILD_NUMBER "_"), hostname);
    filename.strftime_P(PSTR("%Y%m%d_%H%M%S.json"), time(nullptr));
    headers.add<HttpDispositionHeader>(filename);

    PrintString content;
    config.exportAsJson(content, config.getFirmwareVersion());
    auto response = request->beginResponse(200, FSPGM(mime_application_json), content);
    headers.setResponseHeaders(response);
    send(request, response);
}

#if ENABLE_ARDUINO_OTA

    inline static void ArduinoOTALoop()
    {
        ArduinoOTA.handle();
    }

#endif

void Plugin::end()
{
    #if ENABLE_ARDUINO_OTA
        LoopFunctions::remove(ArduinoOTALoop);
    #endif
    #if MDNS_PLUGIN
        MDNSService::removeService(FSPGM(http, "http"), FSPGM(tcp));
        MDNSService::removeService(FSPGM(https, "https"), FSPGM(tcp));
    #endif
    __LDBG_printf("server=%p", _server.get());
    _server.reset();
    #if SECURITY_LOGIN_ATTEMPTS
        _loginFailures.reset();
    #endif
}

#if ENABLE_ARDUINO_OTA

    const __FlashStringHelper *Plugin::ArduinoOTAErrorStr(ota_error_t err)
    {
        if (err == static_cast<ota_error_t>(ArduinoOTAInfo::kNoError)) {
            return F("NONE");
        }
        switch (err) {
        case OTA_AUTH_ERROR:
            return F("AUTH ERROR");
        case OTA_BEGIN_ERROR:
            return F("BEGIN ERROR");
        case OTA_CONNECT_ERROR:
            return F("CONNECT ERROR");
        case OTA_RECEIVE_ERROR:
            return F("RECEIVE ERROR");
        case OTA_END_ERROR:
            return F("END ERROR");
        default:
            break;
        }
        return F("UNKNOWN");
    }

    void Plugin::ArduinoOTAbegin()
    {
        if (_AOTAInfo._runnning) {
            return;
        }
        ArduinoOTA.setHostname(System::Device::getName());
        ArduinoOTA.setPassword(System::Device::getPassword());
        // ArduinoOTA.setPort(8266); // default
        ArduinoOTA.onStart([this]() {
            Logger_notice(F("Firmware upload started"));
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
            __LDBG_printf("ArduinoOTA start");
            _AOTAInfo.start();
            #if IOT_WEATHER_STATION
            #endif
        });
        ArduinoOTA.onEnd([this]() {
            __LDBG_printf("ArduinoOTA end");
            if (_AOTAInfo) {
                _AOTAInfo.stop();
                Logger_security(F("Firmware upgrade successful, rebooting device"));
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                _Scheduler.add(2000, false, [](Event::CallbackTimerPtr timer) {
                    config.restartDevice();
                });
            }
            else {
                Logger_error(F("Firmware upgrade failed: %s"), ArduinoOTAErrorStr(_AOTAInfo._error));
            }
        });
        ArduinoOTA.onProgress([this](int progress, int size) {
            __LDBG_printf("ArduinoOTA progress %d / %d", progress, size);
            if (_AOTAInfo) {
                _AOTAInfo.update(progress, size);
                if (_updateFirmwareCallback) {
                    _updateFirmwareCallback(progress, size);
                }
            }
        });
        ArduinoOTA.onError([this](ota_error_t err) {
            if (_AOTAInfo) {
                _AOTAInfo.stop(err);
                if (!_AOTAInfo) {
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
                    Logger_error(F("Firmware upgrade failed: %s"), ArduinoOTAErrorStr(_AOTAInfo._error));
                }
            }
        });
        ArduinoOTA.setRebootOnSuccess(false);
        #if ESP32
            ArduinoOTA.begin();
            ArduinoOTA.setMdnsEnabled(System::Flags::getConfig().is_mdns_enabled);
        #else
            ArduinoOTA.begin(System::Flags::getConfig().is_mdns_enabled);
        #endif
        LOOP_FUNCTION_ADD(ArduinoOTALoop);
        _AOTAInfo._runnning = true;

        __LDBG_printf("Arduino OTA running on %s", ArduinoOTA.getHostname().c_str());
    }

    void Plugin::ArduinoOTAend()
    {
        if (!_AOTAInfo._runnning) {
            return;
        }
        __LDBG_printf("Arduino OTA stopped");
        LoopFunctions::remove(ArduinoOTALoop);
        _AOTAInfo = ArduinoOTAInfo();
        // release memory
        ArduinoOTA.~ArduinoOTAClass();
        ArduinoOTA = ArduinoOTAClass();
    }

    void Plugin::ArduinoOTADumpInfo(Print &output)
    {
        output.printf_P(PSTR("running=%u in_progress=%u reboot_pending=%u progress=%u/%u error=%d (%s)\n"),
            _AOTAInfo._runnning, _AOTAInfo._inProgress, _AOTAInfo._rebootPending,
            _AOTAInfo._progress, _AOTAInfo._size,
            _AOTAInfo._error, ArduinoOTAErrorStr(_AOTAInfo._error)
            // ArduinoOTA._state
        );
    }

#endif

#if MDNS_PLUGIN

    void Plugin::_addMDNS()
    {
        MDNSService::removeService(FSPGM(http, "http"), FSPGM(tcp));
        MDNSService::removeService(FSPGM(https, "https"), FSPGM(tcp));
        auto cfg = System::WebServer::getConfig();
        String service = FSPGM(http);
        if (cfg.is_https) {
            service += 's';
        }
        MDNSService::addService(service, FSPGM(tcp, "tcp"), cfg.getPort());
        #if ENABLE_ARDUINO_OTA
            MDNSService::enableArduino();
        #endif

        MDNSService::announce();
    }

#endif

void Plugin::begin(bool restart)
{
    __LDBG_printf("restart=%u", restart);
    auto mode = System::WebServer::getMode();
    if (mode == System::WebServer::ModeType::DISABLED) {
        #if MDNS_PLUGIN
            // call announce to remove the web server from MDNS
            // otherwise it shows until TTL is reached
            MDNSService::announce();
        #endif
        return;
    }

    __DBG_assert_printf(_server.get() == nullptr, "SERVER ALREADY RUNNING PTR=%p", _server.get());

    auto cfg = System::WebServer::getConfig();
    _server.reset(new AsyncWebServerEx(cfg.getPort()));
    if (!_server) { // out of memory?
        __LDBG_printf("memory allocation failed");
        return;
    }

    #if MDNS_PLUGIN2
        _addMDNS();
    #endif

    #if SECURITY_LOGIN_ATTEMPTS
        if (System::Flags::getConfig().is_log_login_failures_enabled) {
            _loginFailures.reset(new FailureCounterContainer());
            if (!_loginFailures) {
                __LDBG_printf("memory allocation failed");
            }
            if (_loginFailures) {
                _loginFailures->readFromFS();
            }
        }
        else {
            _loginFailures.reset();
        }
    #endif

    // setup webui socket
    if (System::Flags::getConfig().is_webui_enabled) {
        __LDBG_printf("WebUISocket::_server=%p", WebUISocket::getServerSocket());
        if (WebUISocket::getServerSocket() == nullptr) {
            WebUISocket::setup(_server.get());
        }
    }

    #if WEBSERVER_KFC_OTA
        if (!restart) {
            addHandler(new AsyncUpdateWebHandler(), AsyncUpdateWebHandler::getURI());
        }
    #endif

    _server->onNotFound(handlerNotFound);

    _server->begin();
    __LDBG_printf("HTTP running on port %u", cfg.getPort());
}

AsyncWebServerResponse *Plugin::_beginFileResponse(const FileMapping &mapping, const String &formName, HttpHeaders &headers, bool client_accepts_gzip, bool isAuthenticated, AsyncWebServerRequest *request, WebTemplate *webTemplate)
{
    __LDBG_printf("mapping=%s exists=%u form=%s gz=%u auth=%u request=%p web_template=%p", mapping.getFilename(), mapping.exists(), formName.c_str(), client_accepts_gzip, isAuthenticated, request, webTemplate);

    if (!mapping.exists()) {
        return nullptr;
    }

    auto &path = mapping.getFilenameString();
    if (path.startsWith('.') || path.indexOf(F("/.")) != -1) {
        return nullptr; // hidden file
    }

    bool isHtml = path.endsWith(FSPGM(_html));
    if (isAuthenticated && webTemplate == nullptr) {
        if (path.charAt(0) == '/' && formName.length()) {
            auto plugin = PluginComponent::getTemplate(formName);
            if (plugin) {
                webTemplate = plugin->getWebTemplate(formName);
            }
            else if (nullptr != (plugin = PluginComponent::getForm(formName))) {
                #if IOT_WEATHER_STATION
                    // free memory for the form
                    WeatherStationBase::disableDisplay();
                    Plugin::executeDelayed(request, []() {
                        config.release();
                        WeatherStationBase::enableDisplay();
                    });
                #endif
                FormUI::Form::BaseForm *form = new SettingsForm(nullptr);
                plugin->createConfigureForm(PluginComponent::FormCallbackType::CREATE_GET, formName, *form, request);
                webTemplate = new ConfigTemplate(form, isAuthenticated);
            }
            // __LDBG_printf("form=%s plugin=%08x webTemplate=%08x", formName.c_str(), (uint32_t)plugin, (uint32_t)webTemplate);
        }
    }
    if ((isHtml || path.endsWith(FSPGM(_xml))) && webTemplate == nullptr) {
        webTemplate = new WebTemplate(); // default for all .html files
    }

    __LDBG_printf("web_template=%p", webTemplate);
    AsyncBaseResponse *response;
    if (webTemplate != nullptr) {
        webTemplate->setSelfUri(request->url());
        webTemplate->setAuthenticated(isAuthenticated);
        // process with template
        response = new AsyncTemplateResponse(FPSTR(getContentType(path)), mapping.open(FileOpenMode::read), webTemplate, [webTemplate](const String& name, DataProviderInterface &provider) {
            return TemplateDataProvider::callback(name, provider, *webTemplate);
        });
        headers.addNoCache();
    }
    else {
        headers.replace<HttpDateHeader>(FSPGM(Expires), 86400 * 30);
        headers.replace<HttpDateHeader>(FSPGM(Last_Modified), mapping.getModificationTime());
        if (_isPublic(path)) {
            headers.replace<HttpCacheControlHeader>(HttpCacheControlHeader::CacheControlType::PUBLIC);
        }
        // regular file
#if 0
        response = new AsyncProgmemFileResponse(FPSTR(getContentType(path)), mapping.open(FileOpenMode::read));
#else
        auto response = new AsyncFileResponse(mapping.open(FileOpenMode::read), path, FPSTR(getContentType(path)));
        if (mapping.isGz()) {
            headers.add(FSPGM(Content_Encoding), FSPGM(gzip));
        }
        headers.setResponseHeaders(response);
        return response;
#endif
    }
    if (mapping.isGz()) {
        headers.add(FSPGM(Content_Encoding), FSPGM(gzip));
    }
    headers.setResponseHeaders(response);
    return response;
}

void Plugin::setUpdateFirmwareCallback(UpdateFirmwareCallback callback)
{
#if 0
    if (_updateFirmwareCallback) { // create a list of callbacks that frees itself upon leaving the lambda
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

bool Plugin::_handleFileRead(String path, bool client_accepts_gzip, AsyncWebServerRequest *request, HttpHeaders &headers)
{
    __LDBG_printf("path=%s gz=%u request=%p", path.c_str(), client_accepts_gzip, request);

    if (path.endsWith('/')) {
        path += FSPGM(index_html);
    }

    String formName;
    auto mapping = FileMapping(path);
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
        auto response = request->beginResponse_P(503, FSPGM(mime_text_plain), PSTR("503: Client does not support gzip Content Encoding"));
        _logRequest(request, response);
        request->send(response);
        return true;
    }

    auto authType = getAuthenticated(request);
    auto isAuthenticated = (authType > AuthType::NONE);
    WebTemplate *webTemplate = nullptr;

    if (!_isPublic(path) && !isAuthenticated) {
        auto loginError = FSPGM(Your_session_has_expired, "Your session has expired");

        auto &sid = request->arg(FSPGM(SID));
        if (sid.length()) { // just report failures if the cookie is invalid
            __SID(__LDBG_printf("invalid SID=%s", sid.c_str()));
            Logger_security(F("Authentication failed for %s"), IPAddress(request->client()->getRemoteAddress()).toString().c_str());
        }

        headers.addNoCache(true);

        const String *pUsername;
        const String *pPassword;

        // no_cache_headers();
        if (request->method() == HTTP_POST && ((pUsername = &request->arg(FSPGM(username)))->length() != 0) && ((pPassword = &request->arg(FSPGM(password)))->length() != 0)) {

            IPAddress remote_addr = request->client()->remoteIP();
            auto username = System::Device::getUsername();
            auto password = System::Device::getPassword();

            #if SECURITY_LOGIN_ATTEMPTS

                __SID(debug_printf_P(PSTR("blocked=%u username=%s match:user=%u pass=%u\n"),
                    _loginFailures && _loginFailures->isAddressBlocked(remote_addr),
                    pUsername->c_str(), (*pUsername == username), (*pPassword == password))
                );

                if (
                    ((!_loginFailures) || (_loginFailures && _loginFailures->isAddressBlocked(remote_addr) == false)) &&
                    (*pUsername == username && *pPassword == password)
                ) {

            #else

                if (
                    (*pUsername == username && *pPassword == password)
                ) {

            #endif

                auto &cookie = headers.add<HttpCookieHeader>(FSPGM(SID), generate_session_id(username, password, nullptr), String('/'));
                authType = AuthType::PASSWORD;
                time_t keepTime = request->arg(FSPGM(keep, "keep")).toInt();
                if (keepTime) {
                    auto now = time(nullptr);
                    keepTime = (keepTime == 1 && isTimeValid(now)) ? now : keepTime; // check if the time was provied, otherwise use system time
                    if (isTimeValid(keepTime)) {
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
                #if SECURITY_LOGIN_ATTEMPTS
                    if (_loginFailures) {
                        const FailureCounter &failure = _loginFailures->addFailure(remote_addr);
                        Logger_security(F("Login from %s failed %d times since %s (%s)"), remote_addr.toString().c_str(), failure.getCounter(), failure.getFirstFailure().c_str(), getAuthTypeStr(authType));
                    }
                    else
                #endif
                {
                    Logger_security(F("Login from %s failed (%s)"), remote_addr.toString().c_str(), getAuthTypeStr(authType));
                }
                return _sendFile(FSPGM(_login_html, "/login.html"), String(), headers, client_accepts_gzip, isAuthenticated, request, new LoginTemplate(loginError));
            }
        }
        else {
            if (path.endsWith(FSPGM(_html))) {
                headers.add(createRemoveSessionIdCookie());
                return _sendFile(FSPGM(_login_html), String(), headers, client_accepts_gzip, isAuthenticated, request, new LoginTemplate(loginError));
            }
            else {
                send(403, request);
                return true;
            }
        }
    }

    if (isAuthenticated && request->method() == HTTP_POST) {  // http POST processing

        __LDBG_printf("HTTP post %s", path.c_str());
        headers.addNoCache(true);

        if (path.startsWith('/') && path.endsWith(FSPGM(_html))) {
            // auto name = path.substring(1, path.length() - 5);

            __LDBG_printf("get_form=%s", formName.c_str());
            auto plugin = PluginComponent::getForm(formName);
            if (plugin) {
                __LDBG_printf("found=%p", plugin);
                #if IOT_WEATHER_STATION
                    // release display memory while create the forms
                    WeatherStationBase::disableDisplay();
                #endif
                FormUI::Form::BaseForm *form = new SettingsForm(request);
                plugin->createConfigureForm(PluginComponent::FormCallbackType::CREATE_POST, formName, *form, request);
                webTemplate = new ConfigTemplate(form, isAuthenticated);
                if (form->validate()) {
                    plugin->createConfigureForm(PluginComponent::FormCallbackType::SAVE, formName, *form, request);
                    System::Flags::getWriteableConfig().is_factory_settings = false;
                    config.write();
                    Plugin::executeDelayed(request, [plugin, formName]() {
                        plugin->invokeReconfigure(formName);
                    });
                    WebTemplate::_aliveRedirection = path.substring(1);
                    mapping = FileMapping(FSPGM(applying_html), true);
                }
                else {
                    plugin->createConfigureForm(PluginComponent::FormCallbackType::DISCARD, formName, *form, request);
                    config.discard();
                }
                #if IOT_WEATHER_STATION
                    Plugin::executeDelayed(request, []() {
                        config.release();
                        WeatherStationBase::enableDisplay();
                    });
                #endif
            }
            else { // no plugin found
                bool isRebootHtml = path.endsWith(FSPGM(reboot_html, "reboot.html"));
                bool isFactoryHtml = !isRebootHtml && path.endsWith(FSPGM(factory_html, "factory.html"));
                if (isRebootHtml || isFactoryHtml) {
                    if (request->hasArg(FSPGM(yes))) {
                        #if defined(HAVE_NVS_FLASH)
                            auto formatNVS = request->arg(F("format_nvs")).toInt();
                        #endif
                        bool safeMode = false;
                        if (isRebootHtml) {
                            safeMode = request->arg(FSPGM(safe_mode)).toInt();
                        }
                        else if (isFactoryHtml) {
                            #if defined(HAVE_NVS_FLASH)
                                if (formatNVS) {
                                    Logger_security(F("Formatting NVS partition"));
                                    config.formatNVS();
                                }
                            #endif
                            config.restoreFactorySettings();
                            config.write();
                            #if IOT_BLINDS_CTRL && IOT_BLINDS_CTRL_SAVE_STATE
                                BlindsControl::StateType states[2] = {
                                    static_cast<BlindsControl::StateType>(request->arg(F("blindsctrl_channel0")).toInt()),
                                    static_cast<BlindsControl::StateType>(request->arg(F("blindsctrl_channel1")).toInt())
                                };
                                __LDBG_printf("blinds factory channel states: 0=%s 1=%s", BlindsControl::ChannelState::__getFPStr(states[0]), BlindsControl::ChannelState::__getFPStr(states[1]));
                                BlindsControlPlugin::_saveState(states, 2);
                            #endif
                            RTCMemoryManager::clear();
                        }
                        Plugin::executeDelayed(request, [safeMode]() {
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

    return _sendFile(mapping, formName, headers, client_accepts_gzip, isAuthenticated, request, webTemplate);
}

void Plugin::handleFormData(const String &formName, AsyncWebServerRequest *request, PluginComponent &plugin)
{
    uint32_t code = 200;
    auto action = WebSocketAction::NONE;
    auto actionStr = request->arg(F("__websocket_action"));
    if (actionStr == F("discard")) {
        action = WebSocketAction::DISCARD;
    }
    else if (actionStr == F("save")) {
        action = WebSocketAction::SAVE;
    }
    else if (actionStr == F("apply")) {
        action = WebSocketAction::APPLY;
    }
    __LDBG_printf("actionStr=%s action=%u", __S(actionStr), action);

    if (action == WebSocketAction::NONE) {
        __LDBG_printf("plugin=%s form=%s invalid action=%s", plugin.getName_P(), formName.c_str(), actionStr.c_str());
        code = 400;
    }
    else if (!plugin.canHandleForm(formName)) {
        __LDBG_printf("plugin=%s cannot handle form=%s", plugin.getName_P(), formName.c_str());
        code = 404;
    }
    else {
        FormUI::Form::BaseForm *form = new SettingsForm(request);
        plugin.createConfigureForm(PluginComponent::FormCallbackType::CREATE_POST, formName, *form, request);
        bool modified = config.isDirty();
        if (action == WebSocketAction::DISCARD || !form->validate()) {
            modified = modified || form->hasChanged();
            // form->dump(DEBUG_OUTPUT, emptyString);
            __LDBG_printf("plugin=%s discard config changed=%u dirty=%u modified=%u form=%s action=%s", plugin.getName_P(), form->hasChanged(), config.isDirty(), modified, formName.c_str(), actionStr.c_str());
            if (modified) {
                __LDBG_printf("discarding modified data");
                plugin.createConfigureForm(PluginComponent::FormCallbackType::DISCARD, formName, *form, request);
                config.discard();
                plugin.reconfigure(formName);
            }
        }
        else {
            // form->dump(DEBUG_OUTPUT, emptyString);
            modified = modified || form->hasChanged();
            __LDBG_printf("plugin=%s validated changed=%u dirty=%u modified=%u form=%s action=%s", plugin.getName_P(), form->hasChanged(), config.isDirty(), modified, formName.c_str(), actionStr.c_str());
            plugin.createConfigureForm(PluginComponent::FormCallbackType::SAVE, formName, *form, request);
            if (action == WebSocketAction::SAVE && modified) {
                __LDBG_printf("saving modified data");
                // save current and previous changes from apply
                config.write();
            }
            // only reconfigure if the form has changed
            // if config is dirty, those changes have been applied already
            if (form->hasChanged()) {
                __LDBG_printf("reconfigure plugin=%s", plugin.getName_P());
                plugin.reconfigure(formName);
            }
        }
        delete form;
    }
    #if WEBSERVER_LOG_SERIAL
        {
            PrintString log;
            log.print(reinterpret_cast<const char *>(request->_tempObject));
            log.strftime_P(PSTR(" - - [%m/%d/%Y %H:%M:%S] \""), time(nullptr));
            log.print(request->methodToString());
            log.print(' ');
            log.print(_getUrlAndPostDataFromRequest(request));
            log.printf_P(PSTR(" HTTP/1.%u\" %d %u \"%s\""), request->version(), code, request->contentLength(), PSTR("application/x-www-form-urlencoded"));
            Serial.println(log);
        }
    #endif
}

Plugin::Plugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(Plugin))
{
    REGISTER_PLUGIN(this, "WebServerPlugin");
}

void Plugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    #if ENABLE_ARDUINO_OTA_AUTOSTART
        if (mode == SetupModeType::DEFAULT || mode == SetupModeType::DELAYED_AUTO_WAKE_UP) {
            ArduinoOTAbegin();
        }
    #endif
    if (mode == SetupModeType::DELAYED_AUTO_WAKE_UP) {
        invokeReconfigureNow(getName());
    }
    else {
        __LDBG_printf_N("starting web server");
        begin(false);
    }
}

void Plugin::reconfigure(const String &source)
{
    if (source == F("mdns")) {
        #if MDNS_PLUGIN
            _addMDNS();
        #endif
        return;
    }

    HandlerStoragePtr storage;
    bool running = !!_server;

    __LDBG_printf("server=%p source=%s", _server.get(), source.c_str());
    if (_server) {

        // store handlers and web sockets during reconfigure
        storage.reset(new HandlerStorage());
        storage->_restCallbacks = std::move(_server->_restCallbacks);
        storage->moveHandlers(_server->_handlers);

        // end web server
        end();

    }

    // initialize web server
    begin(running);

    // if the server is disabled, the data of storage is released
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
        #if WEBSERVER_KFC_OTA
            auto kfcOta = PSTR("Enabled");
        #else
            auto kfcOta = PSTR("Disabled");
        #endif
        #if ENABLE_ARDUINO_OTA
            auto ArduinoOta = PSTR("Enabled");
        #else
            auto ArduinoOta = PSTR("Disabled");
        #endif
        #if ENABLE_ARDUINO_OTA_AUTOSTART
            auto autoStart = PSTR(" (Auto)");
        #else
            auto autoStart = PSTR(" (Manual)");
        #endif
        output.printf_P(PSTR(HTML_S(br) "KFC OTA: %s, ArduinoOTA: %s%s"), kfcOta, ArduinoOta, autoStart);
    }
    else {
        output.print(FSPGM(disabled));
    }
}

bool Plugin::addHandler(AsyncWebHandler *handler, const __FlashStringHelper *uri)
{
    __DBG_validatePointerCheck(handler, VP_HSU);
    __LDBG_assert_printf(!!getInstance()._server, "_server is nullptr");
    if (!getInstance()._server) {
        return false;
    }
    getInstance()._server->addHandler(handler);
    __LDBG_printf("handler=%p uri=%s", handler, uri);
    return true;
}

AsyncCallbackWebHandler *Plugin::addHandler(const String &uri, ArRequestHandlerFunction onRequest)
{
    __DBG_validatePointerCheck(uri, VP_HS);
    __LDBG_assert_printf(!!getInstance()._server, "_server is nullptr");
    if (!getInstance()._server) {
        return nullptr;
    }
    auto handler = new AsyncCallbackWebHandler();
    if (!handler) {
        __LDBG_printf("memory allocation failed");
    }
    __LDBG_printf("handler=%p uri=%s", handler, uri.c_str());
    handler->setUri(uri);
    handler->onRequest(onRequest);
    getInstance()._server->addHandler(handler);
    return handler;
}

AuthType Plugin::getAuthenticated(AsyncWebServerRequest *request)
{
    const String *pSID;
    String cSID;
    if (((pSID = &request->arg(FSPGM(SID)))->length() != 0) || HttpCookieHeader::parseCookie(request, FSPGM(SID), cSID)) {
        bool isCookie = (cSID.length() != 0);
        if (pSID->length() != 0) { // parameter overrides cookie
            cSID = *pSID;
            isCookie = false;
        }
        __SID(__DBG_printf("SID=%s remote=%s type=%s", cSID.c_str(), request->client()->remoteIP().toString().c_str(), request->methodToString()));
        if (cSID.length() == 0) {
            return AuthType::NONE;
        }
        if (verify_session_id(cSID.c_str(), System::Device::getUsername(), System::Device::getPassword())) {
            // __SID(__DBG_printf("valid SID=%s type=%s", cSID.c_str(), isCookie ? FSPGM(cookie, "cookie") : request->methodToString()));
            return isCookie ? AuthType::SID_COOKIE : AuthType::SID;
        }
        __SID(__DBG_printf("invalid SID=%s", cSID.c_str()));
    }
    else {
        __SID(__DBG_print("no SID"));
    }
    return AuthType::NONE;
}

// ------------------------------------------------------------------------
// Reset Detector and SaveCrash
// ------------------------------------------------------------------------

namespace SaveCrash {

    AsyncWebServerResponse *webHandler::json(AsyncWebServerRequest *request, HttpHeaders &headers)
    {
        using namespace MQTT::Json;

        PrintString jsonStr;
        AsyncWebServerResponse *response;
        auto fs = SaveCrash::createFlashStorage();

        auto &cmd = request->arg(F("cmd"));
        if (cmd == F("clear")) {
            fs.clear(SPIFlash::ClearStorageType::ERASE);
            response = request->beginResponse_P(200, FSPGM(mime_application_json), PSTR("{\"result\":\"OK\"}"));
        }
        else {
            uint32_t id = strtoul(request->arg(F("id")).c_str(), nullptr, 16);
            if (id) {
                __LDBG_printf("crashlog %u", id);
                JsonPrintStringWrapper wrapper(jsonStr);
                jsonStr.print(F("{\"trace\":\""));
                fs.getCrashLog([&](const SaveCrash::CrashLogEntry &item) {
                    if (item.getId() == id) {
                        fs.printCrashLog(wrapper, item);
                        id = ~0U;
                        return false;
                    }
                    return true;
                });
                if (id != ~0U) {
                    return request->beginResponse(410);
                }
                jsonStr.print(F("\"}"));
            }
            else {
                __LDBG_printf("crash log index");
                NamedArray items(F("items"));
                String timeStr;
                bool outOfMemory = false;
                auto info = fs.getInfo([&](const SaveCrash::CrashLogEntry &item) {
                    __LDBG_printf("id=0x%08x reason=%s", item.getId(), item._header.getReason());
                    if (ESP.getFreeHeap() < 4096) {
                        outOfMemory = true;
                        __LDBG_printf("crashlog index: out of heap");
                        return false;
                    }
                    auto stack = item._header.getStack();
                    timeStr = item._header.getTimeStr();
                    PrintString reason;
                    item._header.printReason(reason);
                    items.append(UnnamedObject(
                        NamedFormattedInteger(F("id"), item.getId(), F("\"%08x\"")),
                        NamedString(F("ts"), timeStr),
                        NamedUint32(F("t"), item._header._time),
                        NamedString(F("r"), reason),
                        NamedString(F("st"), stack)
                    ));
                    return true;
                });
                __LDBG_printf("info available=%u capacity=%u entries=%u largest-block=%u size=%u", info.available(), info.capacity(), info.numTraces(), info.getLargestBlock(), info.size());

                auto infoStr = PrintString(F("%u%% free %s/%s %s"), info.available() * 100 / info.capacity(), formatBytes(info.available()).c_str(), formatBytes(info.capacity()).c_str(), SPGM(UTF8_rocket));
                if (outOfMemory) {
                    infoStr += F(" (Insufficient memory to show all)");
                }
                UnnamedObjectWriter(jsonStr, items, NamedString(F("info"), reinterpret_cast<FStr>(infoStr.c_str())));
            }
            response = request->beginResponse(200, FSPGM(mime_application_json), std::move(jsonStr));
        }

        headers.setResponseHeaders(response);
        return response;
    }
}

void ResetDetectorPlugin::createMenu()
{
    #if ESP8266
        bootstrapMenu.addMenuItem(F("SaveCrash Log"), F("savecrash.html"), navMenu.util);
    #endif
}

#include "web_server_action.h"
