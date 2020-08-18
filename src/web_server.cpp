/**
 * Author: sascha_lammers@gmx.de
 */

#if WEBSERVER_SUPPORT

#include <Arduino_compat.h>
#include <ArduinoOTA.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include <StreamString.h>
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
#if STK500V1
#include "../src/plugins/stk500v1/STK500v1Programmer.h"
#endif
#include "./plugins/mdns/mdns_sd.h"
#if IOT_REMOTE_CONTROL
#include "./plugins/remote/remote.h"
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

static WebServerPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    WebServerPlugin,
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

#if SECURITY_LOGIN_ATTEMPTS
FailureCounterContainer &loginFailures = plugin._loginFailures;
#endif


#define U_ATMEGA 254

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
#ifndef U_SPIFFS
#define U_SPIFFS U_FS
#endif
#endif

struct UploadStatus_t {
    AsyncWebServerResponse *response;
    bool error;
    uint8_t command;
    size_t size;
};

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
    if (String_endsWith(path, SPGM(_html)) || String_endsWith(path, PSTR(".htm"))) {
        return FSPGM(mime_text_html, "text/html");
    }
    else if (String_endsWith(path, PSTR(".css"))) {
        return FSPGM(mime_text_css, "text/css");
    }
    else if (String_endsWith(path, PSTR(".json"))) {
        return FSPGM(mime_application_json, "application/json");
    }
    else if (String_endsWith(path, PSTR(".js"))) {
        return FSPGM(mime_application_javascript, "application/javascript");
    }
    else if (String_endsWith(path, PSTR(".png"))) {
        return FSPGM(mime_image_png, "image/png");
    }
    else if (String_endsWith(path, PSTR(".gif"))) {
        return FSPGM(mime_image_gif, "image/gif");
    }
    else if (String_endsWith(path, PSTR(".jpg"))) {
        return FSPGM(mime_image_jpeg, "image/jpeg");
    }
    else if (String_endsWith(path, PSTR(".ico"))) {
        return FSPGM(mime_image_icon, "image/x-icon");
    }
    else if (String_endsWith(path, PSTR(".svg"))) {
        return FSPGM(mime_image_svg_xml, "image/svg+xml");
    }
    else if (String_endsWith(path, PSTR(".eot"))) {
        return FSPGM(mime_font_eot, "font/eot");
    }
    else if (String_endsWith(path, PSTR(".woff"))) {
        return FSPGM(mime_font_woff, "font/woff");
    }
    else if (String_endsWith(path, PSTR(".woff2"))) {
        return FSPGM(mime_font_woff2, "font/woff2");
    }
    else if (String_endsWith(path, PSTR(".ttf"))) {
        return FSPGM(mime_font_ttf, "font/ttf");
    }
    else if (String_endsWith(path, SPGM(_xml, ".xml"))) {
        return FSPGM(mime_text_xml, "text/xml");
    }
    else if (String_endsWith(path, PSTR(".pdf"))) {
        return FSPGM(mime_application_pdf, "application/pdf");
    }
    else if (String_endsWith(path, PSTR(".zip"))) {
        return FSPGM(mime_application_zip, "application/zip");
    }
    else if (String_endsWith(path, PSTR(".gz"))) {
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
        Scheduler.addTimer(2000, false, [callback](EventScheduler::TimerPtr timer) {
            callback();
        });
    });
}

bool WebServerPlugin::_isPublic(const String &pathString) const
{
    auto path = pathString.c_str();
    if (strstr_P(path, PSTR(".."))) {
        return false;
    } else if (*path++ == '/') {
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

bool WebServerPlugin::_clientAcceptsGzip(AsyncWebServerRequest *request) const
{
    auto header = request->getHeader(FSPGM(Accept_Encoding, "Accept-Encoding"));
    if (!header) {
        return false;
    }
    auto acceptEncoding = header->value().c_str();
    return (strstr_P(acceptEncoding, SPGM(gzip, "gzip")) || strstr_P(acceptEncoding, SPGM(deflate, "deflate")));
}

void WebServerPlugin::handlerNotFound(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (!plugin._handleFileRead(request->url(), plugin._clientAcceptsGzip(request), request)) {
        request->send(404);
    }
}

void WebServerPlugin::handlerScanWiFi(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (plugin.isAuthenticated(request) == true) {
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        auto response = new AsyncNetworkScanResponse(request->arg(FSPGM(hidden, "hidden")).toInt());
        httpHeaders.setAsyncBaseResponseHeaders(response);
        request->send(response);
    } else {
        request->send(403);
    }
}

void WebServerPlugin::handlerZeroconf(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (plugin.isAuthenticated(request) == true) {
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        auto response = new AsyncResolveZeroconfResponse(request->arg(FSPGM(value)));
        httpHeaders.setAsyncBaseResponseHeaders(response);
        request->send(response);
    } else {
        request->send(403);
    }
}

void WebServerPlugin::handlerLogout(AsyncWebServerRequest *request)
 {
    __SID(debug_println(F("sending remove SID cookie")));
    auto response = request->beginResponse(302);
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache(true);
    httpHeaders.add(createRemoveSessionIdCookie());
    httpHeaders.add(new HttpLocationHeader(String('/')));
    httpHeaders.setAsyncWebServerResponseHeaders(response);
    request->send(response);
}

void WebServerPlugin::handlerAlive(AsyncWebServerRequest *request)
{
    auto response = request->beginResponse(200, FSPGM(mime_text_plain), String(request->arg(String('p')).toInt()));
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache();
    httpHeaders.setAsyncWebServerResponseHeaders(response);
    request->send(response);
}

void WebServerPlugin::handlerSyncTime(AsyncWebServerRequest *request)
{
    if (plugin.isAuthenticated(request) == true) {
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        PrintHtmlEntitiesString str;
        WebTemplate::printSystemTime(time(nullptr), str);
        auto response = new AsyncBasicResponse(200, FSPGM(mime_text_html), str);
        httpHeaders.setAsyncWebServerResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

void WebServerPlugin::handlerWebUI(AsyncWebServerRequest *request)
{
    if (plugin.isAuthenticated(request) == true) {
        WebServerSetCPUSpeedHelper setCPUSpeed;
        auto response = new AsyncJsonResponse();
        WsWebUISocket::createWebUIJSON(response->getJsonObject());
        response->updateLength();
        HttpHeaders httpHeaders;
        httpHeaders.addNoCache();
        httpHeaders.setAsyncBaseResponseHeaders(response);
        request->send(response);
    }
    else {
        request->send(403);
    }
}

#if WEBUI_ALERTS_ENABLED

void WebServerPlugin::handlerAlerts(AsyncWebServerRequest *request)
{
    if (plugin.isAuthenticated(request) == true) {
        WebServerSetCPUSpeedHelper setCPUSpeed;
        auto pollId = (uint32_t)request->arg(F("poll_id")).toInt();
        if (pollId) {
            PrintHtmlEntitiesString str = String('[');
            config.printAlertsAsJson(str, false, pollId);
            str += ']';
            AsyncWebServerResponse *response = new AsyncBasicResponse(200, FSPGM(mime_application_json), str);
            HttpHeaders httpHeaders;
            httpHeaders.addNoCache();
            httpHeaders.setAsyncWebServerResponseHeaders(response);
            request->send(response);
            return;
        }
        else {
            auto alertId = (uint32_t)request->arg(FSPGM(id)).toInt();
            if (alertId) {
                WebUIAlerts_remove(alertId);
                request->send(200, FSPGM(mime_text_plain), FSPGM(OK));
                return;
            }
        }
        request->send(400);
    }
    else {
        request->send(403);
    }
}

#endif

void WebServerPlugin::handlerSpeedTest(AsyncWebServerRequest *request, bool zip)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
#if !defined(SPEED_TEST_NO_AUTH) || SPEED_TEST_NO_AUTH == 0
    if (plugin.isAuthenticated(request)) {
#endif
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();

        AsyncSpeedTestResponse *response;
        auto size = std::max(1024 * 64, (int)request->arg(FSPGM(size, "size")).toInt());
        if (zip) {
            response = new AsyncSpeedTestResponse(FSPGM(mime_application_zip), size);
            httpHeaders.add(new HttpDispositionHeader(F("speedtest.zip")));
        } else {
            response = new AsyncSpeedTestResponse(FSPGM(mime_image_bmp), size);
        }
        httpHeaders.setAsyncBaseResponseHeaders(response);

        request->send(response);
#if !defined(SPEED_TEST_NO_AUTH) || SPEED_TEST_NO_AUTH == 0
    } else {
        request->send(403);
    }
#endif
}


void WebServerPlugin::handlerSpeedTestZip(AsyncWebServerRequest *request)
{
    handlerSpeedTest(request, true);
}

void WebServerPlugin::handlerSpeedTestImage(AsyncWebServerRequest *request)
{
    handlerSpeedTest(request, false);
}

void WebServerPlugin::handlerImportSettings(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (plugin.isAuthenticated(request) == true) {
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();

        PGM_P message;
        int count = -1;

        auto configArg = FSPGM(config, "config");
        if (request->method() == HTTP_POST && request->hasArg(configArg)) {
            StreamString data;
            data.print(request->arg(configArg));
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
    else {
        request->send(403);
    }
}

void WebServerPlugin::handlerExportSettings(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (plugin.isAuthenticated(request) == true) {

        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();

        auto hostname = System::Device::getName();

        char timeStr[32];
        auto now = time(nullptr);
        struct tm *tm = localtime(&now);
        strftime_P(timeStr, sizeof(timeStr), PSTR("%Y%m%d_%H%M%S"), tm);
        PrintString filename(F("kfcfw_config_%s_b" __BUILD_NUMBER "_%s.json"), hostname, timeStr);
        httpHeaders.add(new HttpDispositionHeader(filename));

        PrintString content;
        config.exportAsJson(content, config.getFirmwareVersion());
        AsyncWebServerResponse *response = new AsyncBasicResponse(200, FSPGM(mime_application_json), content);
        httpHeaders.setAsyncWebServerResponseHeaders(response);

        request->send(response);
    }
    else {
        request->send(403);
    }
}

void WebServerPlugin::handlerUpdate(AsyncWebServerRequest *request)
{
    if (request->_tempObject) {
        UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
        AsyncWebServerResponse *response = nullptr;
#if STK500V1
        if (status->command == U_ATMEGA) {
            if (stk500v1) {
                request->send_P(200, FSPGM(mime_text_plain), PSTR("Upgrade already running"));
            }
            else {
                Logger_security(F("Starting ATmega firmware upgrade..."));

                stk500v1 = new STK500v1Programmer(Serial);
                stk500v1->setSignature_P(PSTR("\x1e\x95\x0f"));
                stk500v1->setFile(FSPGM(stk500v1_tmp_file));
                stk500v1->setLogging(STK500v1Programmer::LOG_FILE);

                Scheduler.addTimer(3500, false, [](EventScheduler::TimerPtr timer) {
                    stk500v1->begin([]() {
                        delete stk500v1;
                        stk500v1 = nullptr;
                    });
                });

                response = request->beginResponse(302);
                HttpHeaders httpHeaders(false);
                httpHeaders.add(new HttpLocationHeader(FSPGM(_serial_console_html, "/serial_console.html")));
                httpHeaders.replace(new HttpConnectionHeader(HttpConnectionHeader::CLOSE));
                httpHeaders.setAsyncWebServerResponseHeaders(response);
                request->send(response);
            }
        } else
#endif
        if (!Update.hasError()) {
            Logger_security(F("Firmware upgrade successful"));

            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SLOW);
            Logger_notice(F("Rebooting after upgrade"));
            executeDelayed(request, []() {
                config.restartDevice();
            });

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
                    WebTemplate::_aliveRedirection = String(FSPGM(update_fw_html, "update_fw.html")) + F("#u_flash");
                    location += '/';
                    location += FSPGM(rebooting_html);
                } break;
                case U_SPIFFS:
                    WebTemplate::_aliveRedirection = String(FSPGM(update_fw_html)) + F("#u_spiffs");
                    location += '/';
                    location += FSPGM(rebooting_html);
                    break;
            }
            response = request->beginResponse(302);
            HttpHeaders httpHeaders(false);
            httpHeaders.add(new HttpLocationHeader(location));
            httpHeaders.replace(new HttpConnectionHeader(HttpConnectionHeader::CLOSE));
            httpHeaders.setAsyncWebServerResponseHeaders(response);
            request->send(response);

        } else {
            // SPIFFS.begin();

            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOS);
            StreamString errorStr;
            Update.printError(errorStr);
            Logger_error(F("Firmware upgrade failed: %s"), errorStr.c_str());

            String message = F("<h2>Firmware update failed with an error:<br></h2><h3>");
            message += errorStr;
            message += F("</h3>");

            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();
            if (!plugin._sendFile(String('/') + FSPGM(update_fw_html), String(), httpHeaders, plugin._clientAcceptsGzip(request), request, new UpgradeTemplate(message))) {
                message += F("<br><a href=\"/\">Home</a>");
                request->send(200, FSPGM(mime_text_plain), message);
            }
        }

    } else {
        request->send(403);
    }
}

void WebServerPlugin::handlerUploadUpdate(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
#if STK500V1
    static File firmwareTempFile;
#endif
    if (index == 0 && !request->_tempObject && plugin.isAuthenticated(request) == true) {
        request->_tempObject = calloc(sizeof(UploadStatus_t), 1);
        Logger_notice(F("Firmware upload started"));
#if IOT_REMOTE_CONTROL
        RemoteControlPlugin::disableAutoSleep();
#endif
    }
    UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
    if (plugin._updateFirmwareCallback) {
        plugin._updateFirmwareCallback(index + len, request->contentLength());
    }
    if (status && !status->error) {
        PrintString out;
        bool error = false;
        if (!index) {
            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);

            size_t size;
            uint8_t command;

            uint8_t imageType = 0;

            if (String_equals(request->arg(FSPGM(image_type)), PSTR("u_flash"))) { // firmware selected
                imageType = 0;
            }
            else if (String_equals(request->arg(FSPGM(image_type)), PSTR("u_spiffs"))) { // spiffs selected
                imageType = 1;
            }
#if STK500V1
            else if (String_equals(request->arg(FSPGM(image_type)), PSTR("u_atmega"))) { // atmega selected
                imageType = 3;
            }
            else if (strstr_P(filename.c_str(), PSTR(".hex"))) { // auto select
                imageType = 3;
            }
#endif
            else if (strstr_P(filename.c_str(), PSTR("spiffs"))) { // auto select
                imageType = 2;
            }

#if STK500V1
            if (imageType == 3) {
                status->command = U_ATMEGA;
                firmwareTempFile = SPIFFS.open(FSPGM(stk500v1_tmp_file), fs::FileOpenMode::write);
                __LDBG_printf("ATmega fw temp file %u, filename %s", (bool)firmwareTempFile, String(FSPGM(stk500v1_tmp_file)).c_str());
            } else
#endif
            {
                if (imageType) {
#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
                    size = ((size_t) &_FS_end - (size_t) &_FS_start);
#else
                    size = 1048576;
#endif
                    command = U_SPIFFS;
                    // SPIFFS.end();
                    // SPIFFS.format();
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
                    error = true;
                }
            }
        }
#if STK500V1
        if (status->command == U_ATMEGA) {
            firmwareTempFile.write(data, len);
            if (final) {
                __LDBG_printf("Upload Success: %uB", firmwareTempFile.size());
                firmwareTempFile.close();
            }
        } else
#endif
        {
            if (!error && !Update.hasError()) {
                if (Update.write(data, len) != len) {
                    error = true;
                }
            }
            if (final) {
                if (Update.end(true)) {
                    __LDBG_printf("Update Success: %uB", index + len);
                } else {
                    error = true;
                }
            }
            if (error) {
#if DEBUG
                Update.printError(DEBUG_OUTPUT);
#endif
                status->error = true;
            }
        }
    }
}

void WebServerPlugin::end()
{
    if (_server) {
        delete _server;
        _server = nullptr;
    }
}

void WebServerPlugin::begin()
{
    auto mode = System::WebServer::getMode();
    if (mode == System::WebServer::ModeType::DISABLED) {
        MDNSService::announce();
        return;
    }

    auto cfg = System::WebServer::getConfig();
    String service = FSPGM(http);
    if (cfg.is_https) {
        service += 's';
    }
    MDNSService::addService(service, FSPGM(tcp, "tcp"), cfg.getPort());
    MDNSService::announce();


    _server = new AsyncWebServer(cfg.getPort());
    // server->addHandler(&events);

    _loginFailures.readFromSPIFFS();

#if REST_API_SUPPORT
    // // download /.mappings
    // WebServerPlugin::addHandler(F("/rest/KFC/webui_details"), [](AsyncWebServerRequest *request) {
    //     if (isAuthenticated(request)) {
    //         rest_api_kfc_webui_details(request);
    //     } else {
    //         request->send(403);
    //     }
    // });
    // server->addHandler(new AsyncFileUploadWebHandler(F("/rest/KFC/update_webui"), [](AsyncWebServerRequest *request) {
    //     if (request->_tempObject) {
    //         rest_api_kfc_update_webui(request);
    //     } else {
    //         request->send(403);
    //     }
    // }));
#endif

    _server->onNotFound(handlerNotFound);

    if (System::Flags::getConfig().is_webui_enabled) {
        WsWebUISocket::setup();
        WebServerPlugin::addHandler(F("/webui_get"), handlerWebUI);
    }

    WebServerPlugin::addHandler(F("/scan_wifi"), handlerScanWiFi);
    WebServerPlugin::addHandler(F("/zeroconf"), handlerZeroconf);
    WebServerPlugin::addHandler(F("/logout"), handlerLogout);
    WebServerPlugin::addHandler(F("/is_alive"), handlerAlive);
    WebServerPlugin::addHandler(F("/sync_time"), handlerSyncTime);
    WebServerPlugin::addHandler(F("/export_settings"), handlerExportSettings);
    WebServerPlugin::addHandler(F("/import_settings"), handlerImportSettings);
    WebServerPlugin::addHandler(F("/speedtest.zip"), handlerSpeedTestZip);
    WebServerPlugin::addHandler(F("/speedtest.bmp"), handlerSpeedTestImage);
#if WEBUI_ALERTS_ENABLED
    if (System::Flags::getConfig().is_webalerts_enabled) {
        WebServerPlugin::addHandler(F("/alerts"), handlerAlerts);
    }
#endif
    _server->on(String(F("/update")).c_str(), HTTP_POST, handlerUpdate, handlerUploadUpdate);

    _server->begin();
    __LDBG_printf("HTTP running on port %u", port);
}

bool WebServerPlugin::_sendFile(const FileMapping &mapping, const String &formName, HttpHeaders &httpHeaders, bool client_accepts_gzip, AsyncWebServerRequest *request, WebTemplate *webTemplate)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    __LDBG_printf("mapping=%s exists=%u form=%s gz=%u request=%p web_template=%p", mapping.getFilename(), mapping.exists(), formName.c_str(), client_accepts_gzip, request, webTemplate);

    if (!mapping.exists()) {
        return false;
    }

    auto &path = mapping.getFilenameString();
    bool isHtml = String_endsWith(path, SPGM(_html));
    if (webTemplate == nullptr) {
        if (path.charAt(0) == '/' && formName.length()) {
            auto plugin = PluginComponent::getTemplate(formName);
            if (plugin) {
                webTemplate = plugin->getWebTemplate(formName);
            }
            else if (nullptr != (plugin = PluginComponent::getForm(formName))) {
#if IOT_WEATHER_STATION
                __weatherStationDetachCanvas(true);
                request->onDisconnect(__weatherStationAttachCanvas); // unlock on disconnect
#endif
                Form *form = new SettingsForm(nullptr);
                plugin->createConfigureForm(PluginComponent::FormCallbackType::CREATE_GET, formName, *form, request);
                webTemplate = new ConfigTemplate(form);
            }
        }
    }
    if ((isHtml || String_endsWith(path, SPGM(_xml, ".xml"))) && webTemplate == nullptr) {
        webTemplate = new WebTemplate(); // default for all .html files
    }

    AsyncBaseResponse *response;
    if (webTemplate != nullptr) {
        webTemplate->setSelfUri(request->url());
        // process with template
        response = new AsyncTemplateResponse(FPSTR(getContentType(path)), mapping.open(FileOpenMode::read), webTemplate, [webTemplate](const String& name, DataProviderInterface &provider) {
            return TemplateDataProvider::callback(name, provider, *webTemplate);
        });
        httpHeaders.addNoCache();
    }
    else {
        // regular file
        response = new AsyncProgmemFileResponse(FPSTR(getContentType(path)), mapping.open(FileOpenMode::read));
        httpHeaders.replace(new HttpDateHeader(FSPGM(Expires), 86400 * 30));
        httpHeaders.replace(new HttpDateHeader(FSPGM(Last_Modified), mapping.getModificationTime()));
        if (_isPublic(path)) {
            httpHeaders.replace(new HttpCacheControlHeader(HttpCacheControlHeader::PUBLIC));
        }
    }
    if (mapping.isGz()) {
        httpHeaders.add(FSPGM(Content_Encoding), FSPGM(gzip));
    }
    httpHeaders.setAsyncBaseResponseHeaders(response);
    request->send(response);

    return true;
}


void WebServerPlugin::setUpdateFirmwareCallback(UpdateFirmwareCallback_t callback)
{
    _updateFirmwareCallback = callback;
}

bool WebServerPlugin::_handleFileRead(String path, bool client_accepts_gzip, AsyncWebServerRequest *request)
{
    __LDBG_printf("path=%s gz=%u request=%p", path.c_str(), client_accepts_gzip, request);
    WebServerSetCPUSpeedHelper setCPUSpeed;

    if (String_endsWith(path, '/')) {
        path += FSPGM(index_html);
    }

    String formName;
    auto mapping = FileMapping(path.c_str());
    if (path.charAt(0) == '/') {
        int pos;
        if (!mapping.exists() && (pos = path.indexOf('/', 3)) != -1 && String_endsWith(path, SPGM(_html))) {
            formName = path.substring(pos + 1, path.length() - 5);
            String path2 = path.substring(0, pos) + FSPGM(_html);
            mapping = FileMapping(path2.c_str());
            __DBG_printf("path=%s path2=%s form=%s mapping=%u name=%s", path.c_str(), path2.c_str(), formName.c_str(), mapping.exists(), mapping.getFilenameString().c_str());
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

    auto authType = isAuthenticated(request);
    bool isAuthenticated = (authType > AuthType::NONE);
    WebTemplate *webTemplate = nullptr;
    HttpHeaders httpHeaders;

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

            __SID(debug_printf_P(PSTR("blocked=%u username=%s match:user=%u pass=%u\n"), _loginFailures.isAddressBlocked(remote_addr), request->arg(FSPGM(username)).c_str(), (request->arg(FSPGM(username)) == username), (request->arg(FSPGM(password)) == password)));

            if (_loginFailures.isAddressBlocked(remote_addr) == false && request->arg(FSPGM(username)) == username && request->arg(FSPGM(password)) == password) {

                auto cookie = new HttpCookieHeader(FSPGM(SID), generate_session_id(username, password, nullptr), String('/'));
                authType = AuthType::PASSWORD;
                time_t keepTime = request->arg(FSPGM(keep, "keep")).toInt();
                if (keepTime) {
                    auto now = time(nullptr);
                    keepTime = (keepTime == 1 && IS_TIME_VALID(now)) ? now : keepTime; // check if the time was provied, otherwise use system time
                    if (IS_TIME_VALID(keepTime)) {
                        cookie->setExpires(keepTime + System::Device::getConfig().getWebUICookieLifetimeInSeconds());
                    }
                }
                __SID(debug_printf_P(PSTR("new SID cookie=%s\n"), cookie->getValue().c_str()));
                httpHeaders.add(cookie);

                __LDBG_printf("Login successful: type=%u cookie=%s", getAuthTypeStr(authType), cookie->getValue().c_str());
                isAuthenticated = true;
                Logger_security(F("Login successful from %s (%s)"), remote_addr.toString().c_str(), getAuthTypeStr(authType));
            }
            else {
                loginError = FSPGM(Invalid_username_or_password, "Invalid username or password");
                const FailureCounter &failure = _loginFailures.addFailure(remote_addr);
                Logger_security(F("Login from %s failed %d times since %s (%s)"), remote_addr.toString().c_str(), failure.getCounter(), failure.getFirstFailure().c_str(), getAuthTypeStr(authType));
                return _sendFile(FSPGM(_login_html, "/login.html"), String(), httpHeaders, client_accepts_gzip, request, new LoginTemplate(loginError));
            }
        }
        else {
            if (String_endsWith(path, SPGM(_html))) {
                httpHeaders.add(createRemoveSessionIdCookie());
                return _sendFile(FSPGM(_login_html), String(), httpHeaders, client_accepts_gzip, request, new LoginTemplate(loginError));
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

        if (path.charAt(0) == '/' && String_endsWith(path, SPGM(_html))) {
            // auto name = path.substring(1, path.length() - 5);
            auto plugin = PluginComponent::getForm(formName);
            if (plugin) {
#if IOT_WEATHER_STATION
                __weatherStationDetachCanvas(true);
#endif
                Form *form = new SettingsForm(request);
                plugin->createConfigureForm(PluginComponent::FormCallbackType::CREATE_POST, formName, *form, request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    plugin->createConfigureForm(PluginComponent::FormCallbackType::SAVE, formName, *form, request);
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

    return _sendFile(mapping, formName, httpHeaders, client_accepts_gzip, request, webTemplate);
}

WebServerPlugin::WebServerPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(WebServerPlugin)), _updateFirmwareCallback(nullptr), _server(nullptr)
{
    REGISTER_PLUGIN(this, "WebServerPlugin");
}

void WebServerPlugin::setup(SetupModeType mode)
{
    WebUIAlerts_readStorage()
    begin();
    if (mode == SetupModeType::DELAYED_AUTO_WAKE_UP) {
        invokeReconfigureNow(getName());
    }
}

void WebServerPlugin::reconfigure(const String &source)
{
    if (_server) {
        end();
        MDNSService::removeService(FSPGM(http, "http"), FSPGM(tcp));
        MDNSService::removeService(FSPGM(https, "https"), FSPGM(tcp));
    }
    begin();
}

void WebServerPlugin::shutdown()
{
    // auto mdns = PluginComponent::getPlugin<MDNSPlugin>(F("mdns"));
    // if (mdns) {
    //     mdns->shutdown();
    // }
    end();
}

void WebServerPlugin::getStatus(Print &output)
{
    auto mode = System::WebServer::getMode();
    output.print(F("Web server "));
    if (mode != System::WebServer::ModeType::DISABLED) {
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
        int count = _restCallbacks.size();
        if (count) {
            output.printf_P(PSTR(HTML_S(br) "%d Rest API endpoints"), count);
        }
    }
    else {
        output.print(FSPGM(disabled));
    }
}

void WebServerPlugin::createConfigureForm(PluginComponent::FormCallbackType type, const String &name, Form &form, AsyncWebServerRequest *request)
{
    if (type == PluginComponent::FormCallbackType::SAVE) {
        config.setConfigDirty(true);
        return;
    }
    else if (!isCreateFormCallbackType(type)) {
        return;
    }

    // form.setFormUI(F("Remote Access Configuration"));

    form.addGetterSetterType_P(SPGM(httpmode, "httpmode"), System::WebServer::getMode, System::WebServer::setMode, uint8_t)); //->setFormUI(new FormUI::UI(FormUI::Type::SELECT, FSPGM(HTTP_Server, "HTTP Server")))->setBoolItems());
    form.addValidator(FormRangeValidatorEnum<System::WebServer::ModeType>());

#if WEBSERVER_TLS_SUPPORT
    form.addValidator(FormMatchValidator(F("There is not enough free RAM for TLS support"), [](FormField &field) {
        return (field.getValue().toInt() != HTTP_MODE_SECURE) || (ESP.getFreeHeap() > 24000);
    }));
#endif

    auto &flags = System::Flags::getWriteableConfig();
    auto &cfg = System::WebServer::getWriteableConfig();

#if defined(ESP8266)
    form.addWriteableStruct("httperf", flags, is_webserver_performance_mode_enabled));
#endif

    form.add(F("httport"), cfg.getPortAsString(), [&cfg](const String &value, FormField &field, bool store) {
        if (store) {
            cfg.setPort(value.toInt(), cfg.isSecure()); // setMode() is executed already and cfg.is_https set
            field.setValue(cfg.getPortAsString());
        }
        return false;
    }); //->setFormUI(new FormUI::UI(FormUI::Type::INTEGER, FSPGM(Port, "Port"))));
    form.addValidator(FormNetworkPortValidator(true));

#if WEBSERVER_TLS_SUPPORT

    form.add(new FormObject<File2String>(F("ssl_cert"), File2String(FSPGM(server_crt)), nullptr));
    form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key)), nullptr));

#endif
    form.addCStrGetterSetter("btok", System::Device::getToken, System::Device::setToken)); //->setFormUI(new FormUI::UI(FormUI::Type::TEXT, F("Token for Web Server, Web Sockets and Tcp2Serial"))));
    form.addValidator(FormLengthValidator(System::Device::kTokenMinSize, System::Device::kTokenMaxSize));

    form.finalize();
}

WebServerPlugin &WebServerPlugin::getInstance()
{
    return plugin;
}

AsyncWebServer *WebServerPlugin::getWebServerObject()
{
    return plugin._server;
}

bool WebServerPlugin::addHandler(AsyncWebHandler *handler)
{
    if (!plugin._server) {
        return false;
    }
    plugin._server->addHandler(handler);
    __LDBG_printf("handler=%p", handler);
    return true;
}

AsyncCallbackWebHandler *WebServerPlugin::addHandler(const String &uri, ArRequestHandlerFunction onRequest)
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

WebServerPlugin::AsyncRestWebHandler *WebServerPlugin::addRestHandler(RestHandler &&handler)
{
    AsyncRestWebHandler *restHandler = nullptr;
    __LDBG_printf("uri=%s", handler.getURL());
    if (!plugin._server) {
        return restHandler;
    }
    if (plugin._restCallbacks.empty()) {
        __LDBG_printf("installing REST handler");
        restHandler = new AsyncRestWebHandler();
        plugin._server->addHandler(restHandler);
        __LDBG_printf("handler=%p", restHandler);
    }
    plugin._restCallbacks.emplace_back(handler);
    return restHandler;
}

bool WebServerPlugin::isRunning() const
{
    return (_server != nullptr);
}

WebServerPlugin::AuthType WebServerPlugin::isAuthenticated(AsyncWebServerRequest *request) const
{
    String SID;
    auto auth = request->getHeader(FSPGM(Authorization));
    if (auth) {
        auto &value = auth->value();
        __SID(debug_printf_P(PSTR("auth SID=%s remote=%s\n"), value.c_str(), request->client()->remoteIP().toString().c_str()));
        if (String_startsWith(value, FSPGM(Bearer_))) {
            auto token = value.c_str() + 7;
            const auto len = value.length() - 7;
            __SID(debug_printf_P(PSTR("token=%s device_token=%s len=%u\n"), token, System::Device::getToken(), len));
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
            __SID(debug_printf_P(PSTR("SID=%s remote=%s type=%s\n"), SID.c_str(), request->client()->remoteIP().toString().c_str(), request->methodToString()));
            if (SID.length() == 0) {
                return AuthType::NONE;
            }
            if (verify_session_id(SID.c_str(), System::Device::getUsername(), System::Device::getPassword())) {
                __SID(debug_printf_P(PSTR("valid SID=%s type=%s\n"), SID.c_str(), isRequestSID ? request->methodToString() : FSPGM(cookie, "cookie")));
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

WebServerPlugin::RestHandler::RestHandler(const __FlashStringHelper *url, Callback callback) : _url(url), _callback(callback)
{

}

const __FlashStringHelper *WebServerPlugin::RestHandler::getURL() const
{
    return _url;
}

AsyncWebServerResponse *WebServerPlugin::RestHandler::invokeCallback(AsyncWebServerRequest *request, RestRequest &rest)
{
    return _callback(request, rest);
}

// ------------------------------------------------------------------------
// RestRequest
// ------------------------------------------------------------------------

WebServerPlugin::RestRequest::RestRequest(AsyncWebServerRequest *request, const WebServerPlugin::RestHandler &handler, AuthType auth) :
    _request(request),
    _handler(handler),
    _auth(auth),
    _stream(),
    _reader(_stream),
    _readerError(false)
{
    _reader.initParser();
}

WebServerPlugin::AuthType WebServerPlugin::RestRequest::getAuth() const
{
    return _auth;
}

AsyncWebServerResponse *WebServerPlugin::RestRequest::createResponse(AsyncWebServerRequest *request)
{
    if (_auth == true && !_readerError) {
        return getHandler().invokeCallback(request, *this);
    }

    auto response = new AsyncJsonResponse();
    auto &json = response->getJsonObject();
    if (_auth == false) {
        json.add(FSPGM(status), 401);
        json.add(FSPGM(message), AsyncWebServerResponse::responseCodeToString(401));
    }
    else if (_readerError) {
        json.add(FSPGM(status), 400);
        json.add(FSPGM(message), _reader.getLastErrorMessage());
    }
    return response->finalize();
}

WebServerPlugin::RestHandler &WebServerPlugin::RestRequest::getHandler()
{
    return _handler;
}

JsonMapReader &WebServerPlugin::RestRequest::getJsonReader()
{
    return _reader;
}

void WebServerPlugin::RestRequest::writeBody(uint8_t *data, size_t len)
{
    if (!_readerError) {
        _stream.setData(data, len);
        if (!_reader.parseStream()) {
            __LDBG_printf("json parser: %s", _reader.getLastErrorMessage().c_str());
            _readerError = true;
        }
    }
}

bool WebServerPlugin::RestRequest::isUriMatch(const __FlashStringHelper *uri) const
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

WebServerPlugin::AsyncRestWebHandler::AsyncRestWebHandler() : AsyncWebHandler()
{
}

bool WebServerPlugin::AsyncRestWebHandler::canHandle(AsyncWebServerRequest *request)
{
    __LDBG_printf("url=%s auth=%d", request->url().c_str(), WebServerPlugin::getInstance().isAuthenticated(request));
    for(const auto &handler: plugin._restCallbacks) {
        auto url = request->url().c_str();
        auto handlerUrl = reinterpret_cast<PGM_P>(handler.getURL());
        const auto handlerUrlLen = strlen_P(handlerUrl);
        // match "/api/endpoint" and "/api/endpoint/*"
        if (strncmp_P(url, handlerUrl, handlerUrlLen) == 0 && (url[handlerUrlLen] == 0 || url[handlerUrlLen] == '/')) {
            request->addInterestingHeader(FSPGM(Authorization));
            request->_tempObject = new WebServerPlugin::RestRequest(request, handler, WebServerPlugin::getInstance().isAuthenticated(request));
            request->onDisconnect([this, request]() {
                delete reinterpret_cast<WebServerPlugin::RestRequest *>(request->_tempObject);
                request->_tempObject = nullptr;
            });
            return true;
        }
    }
    return false;
}

void WebServerPlugin::AsyncRestWebHandler::handleBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    __LDBG_printf("url=%s data='%*.*s' idx=%u len=%u total=%u", request->url().c_str(), len, len, data, index, len, total);
    auto &rest = *reinterpret_cast<WebServerPlugin::RestRequest *>(request->_tempObject);
    if (rest.getAuth() == true) {
        rest.writeBody(data, len);
    }
}

void WebServerPlugin::AsyncRestWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    __LDBG_printf("url=%s json data=", request->url().c_str());
    auto &rest = *reinterpret_cast<WebServerPlugin::RestRequest *>(request->_tempObject);
#if DEBUG_WEB_SERVER
    rest.getJsonReader().dump(DEBUG_OUTPUT);
#endif
    request->send(rest.createResponse(request));
}

#endif
