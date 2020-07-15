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
#include "kfc_fw_config_classes.h"
#include "blink_led_timer.h"
#include "fs_mapping.h"
#include "session.h"
#include "../include/templates.h"
#include "web_socket.h"
#include "WebUISocket.h"
#include "WebUIAlerts.h"
#include "kfc_fw_config.h"
#if STK500V1
#include "./plugins/stk500v1/STK500v1Programmer.h"
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

static WebServerPlugin plugin;

#if SECURITY_LOGIN_ATTEMPTS
FailureCounterContainer &loginFailures = plugin._loginFailures;
#endif


#define U_ATMEGA 254

#if ARDUINO_ESP8266_VERSION_COMBINED >= 0x020603
extern "C" uint32_t _FS_start;
extern "C" uint32_t _FS_end;
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
    config._H_GET(Config().flags).webServerPerformanceModeEnabled
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
    else if (String_endsWith(path, PSTR(".xml"))) {
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
        Scheduler.addTimer(2000, false, [callback](EventScheduler::TimerPtr timer) {
            callback();
        });
    });
}

// #if LOGGER
// void MyAsyncWebServerRequest::send(AsyncWebServerResponse *response) {
//     uint32_t remoteAddr =  client()->getRemoteAddress();
//     String Referer = arg(F("Referer"));

//     // response->_ackedLength

//     // 127.0.0.1 - frank "GET /apache_pb.gif HTTP/1.0" 200 2326 "http://www.example.com/start.html" "Mozilla/4.08 [en] (Win98; I ;Nav)"
// //
//     AsyncWebServerRequest::send(response);
//     _debug_printf_P("log %s %d\n", url().c_str(), response->_);
// }
// #endif


bool WebServerPlugin::_isPublic(const String &pathString) const
{
    auto path = pathString.c_str();
    if (strstr_P(path, PSTR(".."))) {
        return false;
    } else if (*path++ == '/') {
        return (
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

void WebServerPlugin::handlerLogout(AsyncWebServerRequest *request)
 {
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
        auto size = std::max(1024 * 64, (int)request->arg(F("size")).toInt());
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

        auto hostname = config.getDeviceName();

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
                httpHeaders.add(new HttpLocationHeader(F("/serial_console.html")));
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
                case U_FLASH:
                    WebTemplate::_aliveRedirection = String(FSPGM(update_fw_html, "update_fw.html")) + F("#u_flash");
                    location += '/';
                    location += FSPGM(rebooting_html);
                    break;
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
            if (!plugin._sendFile(String('/') + FSPGM(update_fw_html), httpHeaders, plugin._clientAcceptsGzip(request), request, new UpgradeTemplate(message))) {
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
                _debug_printf_P(PSTR("ATmega fw temp file %u, filename %s\n"), (bool)firmwareTempFile, String(FSPGM(stk500v1_tmp_file)).c_str());
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
                _debug_printf_P(PSTR("Upload Success: %uB\n"), firmwareTempFile.size());
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
                    _debug_printf_P(PSTR("Update Success: %uB\n"), index + len);
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
    auto flags = KFCConfigurationClasses::System::Flags::get();
    if (flags.webServerMode == HTTP_MODE_DISABLED) {
        MDNSService::announce();
        return;
    }

    auto port = config._H_GET(Config().http_port);
    if (flags.webServerMode != HTTP_MODE_DISABLED) {
        String service = FSPGM(http);
        if (flags.webServerMode == HTTP_MODE_SECURE) {
            service += 's';
        }
        MDNSService::addService(service, FSPGM(tcp, "tcp"), port);
    }
    MDNSService::announce();


    _server = new AsyncWebServer(port);
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

    if (!flags.disableWebUI) {
        WsWebUISocket::setup();
        WebServerPlugin::addHandler(F("/webui_get"), handlerWebUI);
    }

    WebServerPlugin::addHandler(F("/scan_wifi"), handlerScanWiFi);
    WebServerPlugin::addHandler(F("/logout"), handlerLogout);
    WebServerPlugin::addHandler(F("/is_alive"), handlerAlive);
    WebServerPlugin::addHandler(F("/sync_time"), handlerSyncTime);
    WebServerPlugin::addHandler(F("/export_settings"), handlerExportSettings);
    WebServerPlugin::addHandler(F("/import_settings"), handlerImportSettings);
    WebServerPlugin::addHandler(F("/speedtest.zip"), handlerSpeedTestZip);
    WebServerPlugin::addHandler(F("/speedtest.bmp"), handlerSpeedTestImage);
#if WEBUI_ALERTS_ENABLED
    if (!flags->disableWebAlerts) {
        WebServerPlugin::addHandler(F("/alerts"), handlerAlerts);
    }
#endif
    _server->on(String(F("/update")).c_str(), HTTP_POST, handlerUpdate, handlerUploadUpdate);

    _server->begin();
    _debug_printf_P(PSTR("HTTP running on port %u\n"), port);
}

bool WebServerPlugin::_sendFile(const FileMapping &mapping, HttpHeaders &httpHeaders, bool client_accepts_gzip, AsyncWebServerRequest *request, WebTemplate *webTemplate)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;

    const String &path = mapping.getFilenameString();
    _debug_printf_P(PSTR("file=%s exists=%d\n"), path.c_str(), mapping.exists());
    if (!mapping.exists()) {
        return false;
    }

    // _debug_printf_P(PSTR("Mapping %s, %s, %d, content type %s\n"), mapping->getPath(), mapping->getMappedPath(), mapping->getFileSize(), getContentType(path));

    if (webTemplate == nullptr) {
        if (path.charAt(0) == '/' && String_endsWith(path, SPGM(_html))) {
            String itemName = path.substring(1, path.length() - 5);
            auto plugin = PluginComponent::getTemplate(itemName);
            if (plugin) {
                webTemplate = plugin->getWebTemplate(itemName);
            }
            else if (nullptr != (plugin = PluginComponent::getForm(itemName))) {
                Form *form = new SettingsForm(nullptr);
                plugin->createConfigureForm(request, *form);
                webTemplate = new ConfigTemplate(form);
            }
        }
        if (webTemplate == nullptr) {
            if (path.charAt(0) == '/') {
                auto path2 = path.substring(1);
                if (String_equals(path2, SPGM(network_html))) {
                    webTemplate = new ConfigTemplate(new NetworkSettingsForm(nullptr));
                }
                else if (String_equals(path2, SPGM(wifi_html))) {
                    webTemplate = new ConfigTemplate(new WifiSettingsForm(nullptr));
                }
                else if (String_equals(path2, SPGM(device_html))) {
                    webTemplate = new ConfigTemplate(new DeviceSettingsForm(nullptr));
                }
                else if (String_equals(path2, SPGM(index_html))) {
                    webTemplate = new StatusTemplate();
                }
                else if (String_equals(path2, SPGM(status_html))) {
                    webTemplate = new StatusTemplate();
                }
                else if (String_endsWith(path2, SPGM(_html))) {
                    webTemplate = new WebTemplate();
                }
            }
        }
    }

    AsyncBaseResponse *response;
    if (webTemplate != nullptr) {
        response = new AsyncTemplateResponse(FPSTR(getContentType(path)), mapping.open(FileOpenMode::read), webTemplate, [webTemplate](const String& name, DataProviderInterface &provider) {
            return TemplateDataProvider::callback(name, provider, *webTemplate);
        });
        httpHeaders.addNoCache(request->method() == HTTP_POST);
    }
    else {
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
    _debug_printf_P(PSTR("path=%s\n"), path.c_str());
    WebServerSetCPUSpeedHelper setCPUSpeed;

    if (String_endsWith(path, '/')) {
        path += FSPGM(index_html);
    }

    if (String_startsWith(path, PSTR("/settings/"))) { // deny access
        request->send(404);
        return false;
    }

    auto mapping = FileMapping(path.c_str());
    if (!mapping.exists()) {
        _debug_printf_P(PSTR("Not found: %s\n"), path.c_str());
        return false;
    }
    if (mapping.isGz() && !client_accepts_gzip) {
        _debug_printf_P(PSTR("Client does not accept gzip encoding: %s\n"), path.c_str());
        request->send_P(503, FSPGM(mime_text_plain), PSTR("503: Client does not support gzip Content Encoding"));
        return true;
    }

    auto authType = isAuthenticated(request);
    bool isAuthenticated = (authType > AuthType::NONE);
    WebTemplate *webTemplate = nullptr;
    HttpHeaders httpHeaders;

    if (!_isPublic(path) && !isAuthenticated) {
        String loginError = F("Your session has expired.");

        if (request->hasArg(FSPGM(SID))) { // just report failures if the cookie is invalid
            Logger_security(F("Authentication failed for %s"), IPAddress(request->client()->getRemoteAddress()).toString().c_str());
        }

        httpHeaders.addNoCache(true);

        // no_cache_headers();
        if (request->method() == HTTP_POST && request->hasArg(FSPGM(username)) && request->hasArg(FSPGM(password))) {
            IPAddress remote_addr = request->client()->remoteIP();

            if (_loginFailures.isAddressBlocked(remote_addr) == false && request->arg(FSPGM(username)) == config.getDeviceName() && request->arg(FSPGM(password)) == config._H_STR(Config().device_pass)) {

                auto cookie = new HttpCookieHeader(FSPGM(SID), generate_session_id(config.getDeviceName(), config._H_STR(Config().device_pass), NULL), String('/'));
                authType = AuthType::PASSWORD;
                time_t keepTime = request->arg(F("keep")).toInt();
                if (keepTime) {
                    _debug_printf_P(PSTR("keep time %u\n"), keepTime);
                    auto now = time(nullptr);
                    keepTime = (keepTime == 1 && IS_TIME_VALID(now)) ? now : keepTime; // check if the time was provied, otherwise use system time
                    if (IS_TIME_VALID(keepTime)) {
                        cookie->setExpires(keepTime + KFCConfigurationClasses::System::Device::getWebUIKeepLoggedInSeconds());
                    }
                }
                _debug_printf_P(PSTR("cookie %s\n"), cookie->getValue().c_str());
                httpHeaders.add(cookie);

                _debug_printf_P(PSTR("Login successful: type=%u cookie=%s\n"), getAuthTypeStr(authType), cookie->getValue().c_str());
                isAuthenticated = true;
                Logger_security(F("Login successful from %s (%s)"), remote_addr.toString().c_str(), getAuthTypeStr(authType));
            }
            else {
                loginError = F("Invalid username or password.");
                const FailureCounter &failure = _loginFailures.addFailure(remote_addr);
                Logger_security(F("Login from %s failed %d times since %s (%s)"), remote_addr.toString().c_str(), failure.getCounter(), failure.getFirstFailure().c_str(), getAuthTypeStr(authType));
                return _sendFile(FSPGM(_login_html, "/login.html"), httpHeaders, client_accepts_gzip, request, new LoginTemplate(loginError));
            }
        }
        else {
            if (String_endsWith(path, SPGM(_html))) {
                httpHeaders.add(createRemoveSessionIdCookie());
                return _sendFile(FSPGM(_login_html), httpHeaders, client_accepts_gzip, request, new LoginTemplate(loginError));
            }
            else {
                request->send(403);
                return true;
            }
        }
    }

    if (isAuthenticated && request->method() == HTTP_POST) {  // http POST processing

        _debug_printf_P(PSTR("HTTP post %s\n"), path.c_str());

        httpHeaders.addNoCache(true);

        if (path.charAt(0) == '/' && String_endsWith(path, SPGM(_html))) {
            auto plugin = PluginComponent::getForm(path.substring(1, path.length() - 5));
            if (plugin) {
                Form *form = new SettingsForm(request);
                plugin->createConfigureForm(request, *form);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    plugin->configurationSaved(form);
                    config.write();
                    executeDelayed(request, [plugin]() {
                        plugin->invokeReconfigure(nullptr);
                    });
                    WebTemplate::_aliveRedirection = path.substring(1);
                    mapping = FileMapping(FSPGM(applying_html), true);
                }
                else {
                    plugin->configurationDiscarded(form);
                    config.discard();
                }
            }
        }
        _debug_printf_P(PSTR("webTemplate=%p\n"), webTemplate);
        if (!webTemplate) {
            if (path.charAt(0) == '/') {
                path.remove(0, 1);
                if (String_equals(path, SPGM(wifi_html, "wifi.html"))) {
                    Form *form = new WifiSettingsForm(request);
                    webTemplate = new ConfigTemplate(form);
                    if (form->validate()) {
                        config.write();
                        executeDelayed(request, []() {
                            PluginComponent::getByName(SPGM(cfg))->invokeReconfigure(SPGM(wifi));
                        });
                        WebTemplate::_aliveRedirection = FSPGM(wifi_html);
                        mapping = FileMapping(FSPGM(applying_html), true);
                    }
                    else {
                        config.discard();
                    }
                }
                else if (String_equals(path, SPGM(network_html, "network.html"))) {
                    Form *form = new NetworkSettingsForm(request);
                    webTemplate = new ConfigTemplate(form);
                    if (form->validate()) {
                        config.write();
                        executeDelayed(request, []() {
                            PluginComponent::getByName(SPGM(cfg))->invokeReconfigure(SPGM(network, "network"));
                        });
                        WebTemplate::_aliveRedirection = FSPGM(network_html);
                        mapping = FileMapping(FSPGM(applying_html), true);
                    }
                    else {
                        config.discard();
                    }
                }
                else if (String_equals(path, SPGM(device_html, "device.html"))) {
                    Form *form = new DeviceSettingsForm(request);
                    webTemplate = new ConfigTemplate(form);
                    if (form->validate()) {
                        config.write();
                        config.setConfigDirty(true);
                        executeDelayed(request, []() {
                            PluginComponent::getByName(SPGM(cfg))->invokeReconfigure(SPGM(device, "device"));
                        });
                        WebTemplate::_aliveRedirection = FSPGM(device_html);
                        mapping = FileMapping(FSPGM(applying_html), true);
                    }
                    else {
                        config.discard();
                    }
                }
                else if (String_equals(path, SPGM(password_html, "password.html"))) {
                    Form *form = new PasswordSettingsForm(request);
                    webTemplate = new ConfigTemplate(form);
                    if (form->validate()) {
                        auto flags = config._H_GET(Config().flags);
                        flags.isDefaultPassword = false;
                        config._H_SET(Config().flags, flags);
                        config.write();
                        executeDelayed(request, []() {
                            PluginComponent::getByName(SPGM(cfg))->invokeReconfigure(SPGM(password));
                        });
                        WebTemplate::_aliveRedirection = FSPGM(password_html);
                        mapping = FileMapping(FSPGM(applying_html), true);
                    }
                    else {
                        config.discard();
                    }
                }
                else if (String_equals(path, SPGM(reboot_html, "reboot.html"))) {
                    if (request->hasArg(FSPGM(yes))) {
                        bool safeMode = request->arg(FSPGM(safe_mode)).toInt();
                        executeDelayed(request, [safeMode]() {
                            config.restartDevice(safeMode);
                        });

                        WebTemplate::_aliveRedirection = FSPGM(status_html);
                        mapping = FileMapping(FSPGM(rebooting_html), true);
                    }
                    else {
                        request->redirect(FSPGM(index_html, "index.html"));
                    }
                }
                else if (String_equals(path, FSPGM(factory_html, "factory.html"))) {
                    if (request->hasArg(FSPGM(yes))) {
                        config.restoreFactorySettings();
                        config.write();
                        RTCMemoryManager::clear();
                        executeDelayed(request, []() {
                            config.restartDevice();
                        });
                        WebTemplate::_aliveRedirection = FSPGM(status_html);
                        mapping = FileMapping(FSPGM(rebooting_html), true);
                    } else {
                        request->redirect(String('/') + FSPGM(index_html));
                    }
                }
            }
        }

/*        Config &config = _Config.get();


        } else if (String_equals(path, PSTR("/pins.html"))) {
            if (request->hasArg(F("led_type"))) {
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
                _Config.getOptions().setLedMode((LedMode_t)request->arg(F("led_type")).toInt());
                //FormBitValue {FLAGS2_LED_NONE, FLAGS2_LED_SINGLE, FLAGS2_LED_TWO, FLAGS2_LED_RGB}
                config.led_pin = request->arg(F("led_pin")).toInt();
                config_write(false);
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOLID);
            }
        */
    }

    return _sendFile(mapping, httpHeaders, client_accepts_gzip, request, webTemplate);
}

void WebServerPlugin::setup(SetupModeType mode)
{
    WebUIAlerts_readStorage()
    begin();
    if (mode == SetupModeType::DELAYED_AUTO_WAKE_UP) {
        invokeReconfigureNow(getName());
    }
}

void WebServerPlugin::reconfigure(PGM_P source)
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
    auto flags = config._H_GET(Config().flags);
    output.print(F("Web server "));
    if (flags.webServerMode != HTTP_MODE_DISABLED) {
        output.printf_P(PSTR("running on port %u"), config._H_GET(Config().http_port));
#if WEBSERVER_TLS_SUPPORT
        output.print(F(", TLS "));
        if (flags.webServerMode == HTTP_MODE_SECURE) {
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

void WebServerPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    form.add<uint8_t>(F("http_enabled"), _H_FLAGS_VALUE(Config().flags, webServerMode));
    form.addValidator(new FormRangeValidator(0, HTTP_MODE_SECURE));

#if WEBSERVER_TLS_SUPPORT
    form.addValidator(new FormMatchValidator(F("There is not enough free RAM for TLS support"), [](FormField &field) {
        return (field.getValue().toInt() != HTTP_MODE_SECURE) || (ESP.getFreeHeap() > 24000);
    }));
#endif

#if defined(ESP8266)
    form.add<bool>(F("http_perf"), _H_FLAGS_BOOL_VALUE(Config().flags, webServerPerformanceModeEnabled));
#endif

    form.add<uint16_t>(F("http_port"), &config._H_W_GET(Config().http_port));
    form.addValidator(new FormTCallbackValidator<uint16_t>([](uint16_t port, FormField &field) {
#if WEBSERVER_TLS_SUPPORT
        if (field.getForm().getField(F("http_enabled"))->getValue().toInt() == HTTP_MODE_SECURE) {
            if (port == 0) {
                port = 443;
            }
        } else
#endif
        {
            if (port == 0) {
                port = 80;
            }
        }
        field.setValue(String(port));
        return true;
    }));
    form.addValidator(new FormRangeValidator(FSPGM(Invalid_port, "Invalid port"), 1, 65535));

#if SPIFFS_SUPPORT && WEBSERVER_TLS_SUPPORT

    form.add(new FormObject<File2String>(F("ssl_certificate"), File2String(FSPGM(server_crt)), nullptr));
    form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key)), nullptr));

#endif

    form.add(F("device_token"), _H_STR_VALUE(Config().device_token));
    form.addValidator(new FormLengthValidator(SESSION_DEVICE_TOKEN_MIN_LENGTH, sizeof(Config().device_token) - 1));

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
    debug_printf_P(PSTR("handler=%p\n"), handler);
    return true;
}

AsyncCallbackWebHandler *WebServerPlugin::addHandler(const String &uri, ArRequestHandlerFunction onRequest)
{
    if (!plugin._server) {
        return nullptr;
    }
    auto handler = new AsyncCallbackWebHandler();
    debug_printf_P(PSTR("handler=%p uri=%s\n"), handler, uri.c_str());
    handler->setUri(uri);
    handler->onRequest(onRequest);
    plugin._server->addHandler(handler);
    return handler;
}

WebServerPlugin::AsyncRestWebHandler *WebServerPlugin::addRestHandler(RestHandler &&handler)
{
    AsyncRestWebHandler *restHandler = nullptr;
    debug_printf_P(PSTR("uri=%s\n"), handler.getURL());
    if (!plugin._server) {
        return restHandler;
    }
    if (plugin._restCallbacks.empty()) {
        debug_printf_P(PSTR("installing REST handler\n"));
        restHandler = new AsyncRestWebHandler();
        plugin._server->addHandler(restHandler);
        debug_printf_P(PSTR("handler=%p\n"), restHandler);
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
    //TODO xxx
    String SID;
    auto auth = request->getHeader(FSPGM(Authorization));
    auto hasSID = request->hasArg(FSPGM(SID));
    if (auth) {
        auto &value = auth->value();
        _debug_printf_P(PSTR("Authorization='%s' from %s\n"), value.c_str(), request->client()->remoteIP().toString().c_str());
        if (String_startsWith(value, FSPGM(Bearer_))) {
            auto token = value.c_str() + 7;
            const auto len = value.length() - 7;
            // _debug_printf_P(PSTR("token='%s'/'%s' len=%u\n"), token, session_get_device_token(), len);
            if (len >= SESSION_DEVICE_TOKEN_MIN_LENGTH && !strcmp(token, session_get_device_token())) {
                return AuthType::BEARER;
            }
        }
        _debug_println(F("Authorization header failed"));
    }
    else if ((hasSID && (SID = request->arg(FSPGM(SID)))) || HttpCookieHeader::parseCookie(request, FSPGM(SID), SID)) {
        _debug_printf_P(PSTR("SID='%s' from %s\n"), SID.c_str(), request->client()->remoteIP().toString().c_str());
        if (SID.length() == 0) {
            return AuthType::NONE;
        }
        if (verify_session_id(SID.c_str(), config.getDeviceName(), config._H_STR(Config().device_pass))) {
            return hasSID ? AuthType::SID : AuthType::SID_COOKIE;
        }
        debug_println(F("SID cookie/param failed"));
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
            _debug_printf_P(PSTR("json parser: %s\n"), _reader.getLastErrorMessage().c_str());
            _readerError = true;
        }
    }
}

bool WebServerPlugin::RestRequest::isUriMatch(const __FlashStringHelper *uri) const
{
    auto handlerUri = reinterpret_cast<PGM_P>(_handler.getURL());
    auto matchUri = reinterpret_cast<PGM_P>(uri);
    if (matchUri == nullptr || pgm_read_byte(matchUri) == 0) {
        return strcmp_P(_request->url().c_str(), handlerUri) == 0;
    }

    // check if the length is ok
    const auto handlerUriLen = strlen_P(handlerUri);
    auto &requestUrl = _request->url();
    if (handlerUriLen > requestUrl.length()) {
        return false;
    }

    // pointer to the end of the base uri
    auto ptr = _request->url().c_str() + handlerUriLen;
    // check if the handler has a trailing slash
    if (pgm_read_byte(&handlerUri[handlerUriLen - 1]) != '/') {
        if (*ptr++ != '/') { // check if the url has a slash as well
            return false;
        }
    }
    return strcmp_P(ptr, matchUri) == 0;
}


// ------------------------------------------------------------------------
// AsyncRestWebHandler
// ------------------------------------------------------------------------

WebServerPlugin::AsyncRestWebHandler::AsyncRestWebHandler() : AsyncWebHandler()
{
}

bool WebServerPlugin::AsyncRestWebHandler::canHandle(AsyncWebServerRequest *request)
{
    _debug_printf_P(PSTR("url=%s auth=%d\n"), request->url().c_str(), WebServerPlugin::getInstance().isAuthenticated(request));
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
    _debug_printf_P(PSTR("url=%s data='%*.*s' idx=%u len=%u total=%u\n"), request->url().c_str(), len, len, data, index, len, total);
    auto &rest = *reinterpret_cast<WebServerPlugin::RestRequest *>(request->_tempObject);
    if (rest.getAuth() == true) {
        rest.writeBody(data, len);
    }
}

void WebServerPlugin::AsyncRestWebHandler::handleRequest(AsyncWebServerRequest *request)
{
    _debug_printf_P(PSTR("url=%s json data=\n"), request->url().c_str());
    auto &rest = *reinterpret_cast<WebServerPlugin::RestRequest *>(request->_tempObject);
#if DEBUG_WEB_SERVER
    rest.getJsonReader().dump(DEBUG_OUTPUT);
#endif
    request->send(rest.createResponse(request));
}

#endif
