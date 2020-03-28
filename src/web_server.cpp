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
#include <SpeedBooster.h>
#include "progmem_data.h"
#include "build.h"
#include "web_server.h"
#include "rest_api.h"
#include "async_web_response.h"
#include "async_web_handler.h"
#include "kfc_fw_config.h"
#include "blink_led_timer.h"
#include "failure_counter.h"
#include "fs_mapping.h"
#include "session.h"
#include "../include/templates.h"
#include "timezone.h"
#include "web_socket.h"
#include "WebUISocket.h"
#include "kfc_fw_config.h"
#include "plugins.h"
#if HUE_EMULATION
#include "plugins/hue/hue.h"
#endif
#if STK500V1
#include "plugins/stk500v1/STK500v1Programmer.h"
#endif
#if IOT_REMOTE_CONTROL
#include "./plugins/remote/remote.h"
#endif

#if DEBUG_WEB_SERVER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

AsyncWebServer *server = nullptr;
FailureCounterContainer loginFailures;
static UpdateFirmwareCallback_t updateFirmwareCallback = nullptr;

#define U_ATMEGA 254

#if defined(ARDUINO_ESP8266_RELEASE_2_6_3)
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


AsyncWebServer *get_web_server_object()
{
    return server;
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


bool web_server_is_public_path(const String &pathString)
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

bool web_server_is_authenticated(AsyncWebServerRequest *request)
{
    String SID;
    if ((request->hasArg(FSPGM(SID)) && (SID = request->arg(FSPGM(SID)))) || HttpCookieHeader::parseCookie(request, FSPGM(SID), SID)) {

        _debug_printf_P(PSTR("web_server_is_authenticated with SID '%s' from %s\n"), SID.c_str(), request->client()->remoteIP().toString().c_str());
        if (SID.length() == 0) {
            return false;
        }
        if (verify_session_id(SID.c_str(), config._H_STR(Config().device_name), config._H_STR(Config().device_pass))) {
            return true;
        }
    }
    _debug_println(F("web_server_is_authenticated failed"));
    return false;
}

bool web_server_client_accepts_gzip(AsyncWebServerRequest *request)
{
    auto header = request->getHeader(String(FSPGM(Accept_Encoding)));
    if (!header) {
        return false;
    }
    auto acceptEncoding = header->value().c_str();
    return (strstr_P(acceptEncoding, PSTR("gzip")) || strstr_P(acceptEncoding, PSTR("deflate")));
}

void web_server_add_handler(AsyncWebHandler* handler)
{
    if (server) {
        server->addHandler(handler);
    }
}

void web_server_add_handler(const String &uri, ArRequestHandlerFunction onRequest)
{
    if (server) {
        AsyncCallbackWebHandler *handler = new AsyncCallbackWebHandler();
        handler->setUri(uri);
        handler->onRequest(onRequest);
        server->addHandler(handler);
    }
}


void web_server_not_found_handler(AsyncWebServerRequest *request)
{

#if HUE_EMULATION
    if (HueEmulation::onNotFound(request)) {
        return; // request was handled
    }
#endif

    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (!web_server_handle_file_read(request->url(), web_server_client_accepts_gzip(request), request)) {
        request->send(404);
    }
}


void web_server_scan_wifi_handler(AsyncWebServerRequest *request)
{
    if (web_server_is_authenticated(request)) {
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        auto response = new AsyncNetworkScanResponse(request->arg(F("hidden")).toInt());
        httpHeaders.setAsyncBaseResponseHeaders(response);
        request->send(response);
    } else {
        request->send(403);
    }
}

void web_server_logout_handler(AsyncWebServerRequest *request)
 {
    auto response = request->beginResponse(302);
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache(true);
    httpHeaders.add(new HttpCookieHeader(FSPGM(SID)));
    httpHeaders.add(new HttpLocationHeader(FSPGM(slash)));
    httpHeaders.setAsyncWebServerResponseHeaders(response);
    request->send(response);
}

void web_server_is_alive_handler(AsyncWebServerRequest *request)
{
    auto response = request->beginResponse(200, FSPGM(mime_text_plain), String(request->arg(F("p")).toInt()));
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache();
    httpHeaders.setAsyncWebServerResponseHeaders(response);
    request->send(response);
}

void web_server_get_webui_json(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    auto response = new AsyncJsonResponse();
    WsWebUISocket::createWebUIJSON(response->getJsonObject());
    response->updateLength();
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache();
    httpHeaders.setAsyncBaseResponseHeaders(response);
    request->send(response);
}

void web_server_speed_test(AsyncWebServerRequest *request, bool zip)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
#if !defined(SPEED_TEST_NO_AUTH) || SPEED_TEST_NO_AUTH == 0
    if (web_server_is_authenticated(request)) {
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

void web_server_speed_test_zip(AsyncWebServerRequest *request)
{
    web_server_speed_test(request, true);
}

void web_server_speed_test_image(AsyncWebServerRequest *request)
{
    web_server_speed_test(request, false);
}

#include <JsonConfigReader.h>

void web_server_import_settings(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (web_server_is_authenticated(request)) {
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();

        PGM_P message;
        int count = -1;

        auto configArg = F("config");
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
                message = PSTR("Success");
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

void web_server_export_settings(AsyncWebServerRequest *request)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;
    if (web_server_is_authenticated(request)) {

        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();

        auto hostname = config._H_STR(Config().device_name);

        char timeStr[32];
        auto now = time(nullptr);
        struct tm *tm = timezone_localtime(&now);
        timezone_strftime_P(timeStr, sizeof(timeStr), PSTR("%Y%m%d_%H%M%S"), tm);
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

void web_server_update_handler(AsyncWebServerRequest *request)
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
            request->onDisconnect([]() {
                Logger_notice(F("Rebooting after upgrade"));
                Scheduler.addTimer(2000, false, [](EventScheduler::TimerPtr timer) {
                    config.restartDevice();
                });
            });

            String location;
            switch(status->command) {
                case U_FLASH:
                    location += F("/rebooting.html#u_flash");
                    break;
                case U_SPIFFS:
                    location += F("/update_fw.html#u_spiffs");
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
            if (!web_server_send_file(F("/update_fw.html"), httpHeaders, web_server_client_accepts_gzip(request), request, new UpgradeTemplate(message))) {
                message += F("<br><a href=\"/\">Home</a>");
                request->send(200, FSPGM(mime_text_plain), message);
            }
        }

    } else {
        request->send(403);
    }
}

void web_server_update_upload_handler(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
#if STK500V1
    static File firmwareTempFile;
#endif
    if (index == 0 && !request->_tempObject && web_server_is_authenticated(request)) {
        request->_tempObject = calloc(sizeof(UploadStatus_t), 1);
        Logger_notice(F("Firmware upload started"));
#if IOT_REMOTE_CONTROL
        RemoteControlPlugin::disableAutoSleep();
#endif
    }
    UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
    if (updateFirmwareCallback) {
        updateFirmwareCallback(index + len, request->contentLength());
    }
    if (status && !status->error) {
        PrintString out;
        bool error = false;
        if (!index) {
            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FLICKER);

            size_t size;
            uint8_t command;

            uint8_t imageType = 0;

            if (String_equals(request->arg(F("image_type")), PSTR("u_flash"))) { // firmware selected
                imageType = 0;
            }
            else if (String_equals(request->arg(F("image_type")), PSTR("u_spiffs"))) { // spiffs selected
                imageType = 1;
            }
#if STK500V1
            else if (String_equals(request->arg(F("image_type")), PSTR("u_atmega"))) { // atmega selected
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
#if defined(ARDUINO_ESP8266_RELEASE_2_6_3)
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

void init_web_server()
{
    if (config._H_GET(Config().flags).webServerMode == HTTP_MODE_DISABLED) {
        return;
    }

    server = new AsyncWebServer(config._H_GET(Config().http_port));
    // server->addHandler(&events);

    loginFailures.readFromSPIFFS();

#if REST_API_SUPPORT
    // // download /.mappings
    // web_server_add_handler(F("/rest/KFC/webui_details"), [](AsyncWebServerRequest *request) {
    //     if (web_server_is_authenticated(request)) {
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

#if HUE_EMULATION
    // adding handler here instead of the plugin setup method that it is more visible
    server->onRequestBody(HueEmulation::onRequestBody);
#endif

    WsWebUISocket::setup();

    server->onNotFound(web_server_not_found_handler);

// #if MDNS_SUPPORT
//     web_server_add_handler(F("/poll_mdns/"), [&httpHeaders](AsyncWebServerRequest *request) {

//         web_server_set_cpu_speed_for_request(request);

//         httpHeaders.addNoCache();
//         if (web_server_is_authenticated(request)) {
//             AsyncWebServerResponse *response;
//             bool isRunning = false;//MDNS_async_query_running();
//             String &result = MDNS_get_html_result();

//             _debug_printf_P(PSTR("HTTP MDS: Query running %d, response length %d\n"), isRunning, result.length());

//             if (result.length()) {
//                 response = request->beginResponse(200, FSPGM(text_html), result);
//             } else if (isRunning) {
//                 response = request->beginResponse(204, FSPGM(text_html), emptyString); // an emtpy response lets the client know to poll again
//             } else {
//                 response = request->beginResponse(503);
//             }
// #if DEBUG
//             httpHeaders.add(MDNS_get_cache_ttl_header());
// #endif
//             httpHeaders.setAsyncWebServerResponseHeaders(response);
//             request->send(response);
//         } else {
//             request->send(403);
//         }
//     });
// #endif

    web_server_add_handler(F("/scan_wifi/"), web_server_scan_wifi_handler);
    web_server_add_handler(F("/logout"), web_server_logout_handler);
    web_server_add_handler(F("/is_alive"), web_server_is_alive_handler);
    web_server_add_handler(F("/webui_get"), web_server_get_webui_json);
    web_server_add_handler(F("/export_settings"), web_server_export_settings);
    web_server_add_handler(F("/import_settings"), web_server_import_settings);
    web_server_add_handler(F("/speedtest.zip"), web_server_speed_test_zip);
    web_server_add_handler(F("/speedtest.bmp"), web_server_speed_test_image);
    server->on(String(F("/update")).c_str(), HTTP_POST, web_server_update_handler, web_server_update_upload_handler);

    server->begin();
    _debug_printf_P(PSTR("HTTP running on port %u\n"), config._H_GET(Config().http_port));
}


PGM_P web_server_get_content_type(const String &path)
{
    if (String_endsWith(path, PSTR(".html")) || String_endsWith(path, PSTR(".htm"))) {
        return SPGM(mime_text_html);
    }
    else if (String_endsWith(path, PSTR(".css"))) {
        return SPGM(mime_text_css);
    }
    else if (String_endsWith(path, PSTR(".json"))) {
        return SPGM(mime_application_json);
    }
    else if (String_endsWith(path, PSTR(".js"))) {
        return SPGM(mime_application_javascript);
    }
    else if (String_endsWith(path, PSTR(".png"))) {
        return SPGM(mime_image_png);
    }
    else if (String_endsWith(path, PSTR(".gif"))) {
        return SPGM(mime_image_gif);
    }
    else if (String_endsWith(path, PSTR(".jpg"))) {
        return SPGM(mime_image_jpeg);
    }
    else if (String_endsWith(path, PSTR(".ico"))) {
        return PSTR("image/x-icon");
    }
    else if (String_endsWith(path, PSTR(".svg"))) {
        return PSTR("image/svg+xml");
    }
    else if (String_endsWith(path, PSTR(".eot"))) {
        return PSTR("font/eot");
    }
    else if (String_endsWith(path, PSTR(".woff"))) {
        return PSTR("font/woff");
    }
    else if (String_endsWith(path, PSTR(".woff2"))) {
        return PSTR("font/woff2");
    }
    else if (String_endsWith(path, PSTR(".ttf"))) {
        return PSTR("font/ttf");
    }
    else if (String_endsWith(path, PSTR(".xml"))) {
        return SPGM(mime_text_xml);
    }
    else if (String_endsWith(path, PSTR(".pdf"))) {
        return PSTR("application/pdf");
    }
    else if (String_endsWith(path, PSTR(".zip"))) {
        return SPGM(mime_application_zip);
    }
    else if (String_endsWith(path, PSTR(".gz"))) {
        return SPGM(mime_application_x_gzip);
    }
    else {
        return SPGM(mime_text_plain);
    }
}

bool web_server_send_file(const FileMapping &mapping, HttpHeaders& httpHeaders, bool client_accepts_gzip, AsyncWebServerRequest* request, WebTemplate* webTemplate)
{
    WebServerSetCPUSpeedHelper setCPUSpeed;

    const String &path = mapping.getFilenameString();
    _debug_printf_P(PSTR("file=%s exists=%d\n"), path.c_str(), mapping.exists());
    if (!mapping.exists()) {
        return false;
    }

    // _debug_printf_P(PSTR("Mapping %s, %s, %d, content type %s\n"), mapping->getPath(), mapping->getMappedPath(), mapping->getFileSize(), web_server_get_content_type(path));

    if (webTemplate == nullptr) {
        if (path.charAt(0) == '/' && String_endsWith(path, PSTR(".html"))) {
            String filename = path.substring(1, path.length() - 5);
            auto plugin = PluginComponent::getTemplate(filename);
            if (plugin) {
                webTemplate = plugin->getWebTemplate(filename);
            } else if (nullptr != (plugin = PluginComponent::getForm(filename))) {
                Form *form = new SettingsForm(nullptr);
                plugin->createConfigureForm(request, *form);
                webTemplate = new ConfigTemplate(form);
            }
        }
        if (webTemplate == nullptr) {
            if (String_equals(path, PSTR("/network.html"))) {
                webTemplate = new ConfigTemplate(new NetworkSettingsForm(nullptr));
            } else if (String_equals(path, PSTR("/wifi.html"))) {
                webTemplate = new ConfigTemplate(new WifiSettingsForm(nullptr));
            } else if (String_equals(path, PSTR("/index.html"))) {
                webTemplate = new StatusTemplate();
            } else if (String_equals(path, PSTR("/status.html"))) {
                webTemplate = new StatusTemplate();
            } else if (String_endsWith(path, PSTR(".html"))) {
                webTemplate = new WebTemplate();
            }
        }
    }

    AsyncBaseResponse *response;
    if (webTemplate != nullptr) {
        response = new AsyncTemplateResponse(FPSTR(web_server_get_content_type(path)), mapping.open(FileOpenMode::read), webTemplate, [webTemplate](const String& name, DataProviderInterface &provider) {
            return TemplateDataProvider::callback(name, provider, *webTemplate);
        });
        httpHeaders.addNoCache(request->method() == HTTP_POST);
    } else {
        response = new AsyncProgmemFileResponse(FPSTR(web_server_get_content_type(path)), mapping.open(FileOpenMode::read));
        httpHeaders.replace(new HttpDateHeader(FSPGM(Expires), 86400 * 30));
        httpHeaders.replace(new HttpDateHeader(FSPGM(Last_Modified), mapping.getModificationTime()));
        if (web_server_is_public_path(path)) {
            httpHeaders.replace(new HttpCacheControlHeader(HttpCacheControlHeader::PUBLIC));
        }
    }
    if (mapping.isGz()) {
        httpHeaders.add(FSPGM(Content_Encoding), F("gzip"));
    }
    httpHeaders.setAsyncBaseResponseHeaders(response);
    request->send(response);

    return true;
}

void web_server_set_update_firmware_callback(UpdateFirmwareCallback_t callback)
{
    updateFirmwareCallback = callback;
}

bool web_server_handle_file_read(String path, bool client_accepts_gzip, AsyncWebServerRequest *request)
{
    _debug_printf_P(PSTR("web_server_handle_file_read: %s\n"), path.c_str());
    WebServerSetCPUSpeedHelper setCPUSpeed;

    if (String_endsWith(path, '/')) {
        path += F("index.html");
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

    bool _is_authenticated = web_server_is_authenticated(request);
    WebTemplate *webTemplate = nullptr;
    HttpHeaders httpHeaders;

    if (!web_server_is_public_path(path) && !_is_authenticated) {
        String loginError = F("Your session has expired.");

        if (request->hasArg(FSPGM(SID))) { // just report failures if the cookie is invalid
            Logger_security(F("Authentication failed for %s"), IPAddress(request->client()->getRemoteAddress()).toString().c_str());
        }

        httpHeaders.addNoCache(true);

        // no_cache_headers();
        if (request->method() == HTTP_POST && request->hasArg(F("username")) && request->hasArg(F("password"))) {
            IPAddress remote_addr = request->client()->remoteIP();

            if (loginFailures.isAddressBlocked(remote_addr) == false && request->arg(F("username")) == config._H_STR(Config().device_name) && request->arg(F("password")) == config._H_STR(Config().device_pass)) {
                auto cookie = new HttpCookieHeader(FSPGM(SID));
                cookie->setValue(generate_session_id(config._H_STR(Config().device_name), config._H_STR(Config().device_pass), NULL));
                cookie->setPath(FSPGM(slash));
                httpHeaders.add(cookie);

                if (request->arg(F("keep")) == FSPGM(1)) {
                    auto _time = time(nullptr);
                    if (IS_TIME_VALID(_time)) {
                        auto keepCookie = new HttpCookieHeader(FSPGM(SID));
                        *keepCookie = *cookie;
                        keepCookie->setExpires(_time + 86400 * 30);
                        httpHeaders.add(keepCookie);
                    }
                }

                _debug_printf_P(PSTR("Login successful, cookie %s\n"), cookie->getValue().c_str());
                _is_authenticated = true;
                Logger_security(F("Login successful from %s"), remote_addr.toString().c_str());
            } else {
                loginError = F("Invalid username or password.");
                const FailureCounter &failure = loginFailures.addFailure(remote_addr);
                Logger_security(F("Login from %s failed %d times since %s"), remote_addr.toString().c_str(), failure.getCounter(), failure.getFirstFailure().c_str());
                return web_server_send_file(FSPGM(login_html), httpHeaders, client_accepts_gzip, request, new LoginTemplate(loginError));
            }
        } else {
            if (String_endsWith(path, PSTR(".html"))) {
                return web_server_send_file(FSPGM(login_html), httpHeaders, client_accepts_gzip, request, new LoginTemplate(loginError));
            } else {
                request->send(403);
                return true;
            }
        }
    }

    if (_is_authenticated && request->method() == HTTP_POST) {  // http POST processing

        _debug_printf_P(PSTR("HTTP post %s\n"), path.c_str());

        httpHeaders.addNoCache(true);

        if (path.charAt(0) == '/' && String_endsWith(path, PSTR(".html"))) {
            auto plugin = PluginComponent::getForm(path.substring(1, path.length() - 5));
            if (plugin) {
                Form *form = new SettingsForm(request);
                plugin->createConfigureForm(request, *form);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                    plugin->invokeReconfigure(nullptr);
                } else {
                    config.discard();
                }
            }
        }
        _debug_printf_P(PSTR("web_server_handle_file_read: webTemplate=%p\n"), webTemplate);
        if (!webTemplate) {
            if (String_equals(path, PSTR("/wifi.html"))) {
                Form *form = new WifiSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                    PluginComponent::getByName(PSTR("cfg"))->invokeReconfigure(PSTR("wifi"));
                } else {
                    config.discard();
                }
            } else if (String_equals(path, PSTR("/network.html"))) {
                Form *form = new NetworkSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                    PluginComponent::getByName(PSTR("cfg"))->invokeReconfigure(PSTR("network"));
                } else {
                    config.discard();
                }
            } else if (String_equals(path, PSTR("/password.html"))) {
                Form *form = new PasswordSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    auto flags = config._H_GET(Config().flags);
                    flags.isDefaultPassword = false;
                    config._H_SET(Config().flags, flags);
                    config.write();
                    PluginComponent::getByName(PSTR("cfg"))->invokeReconfigure(PSTR("password"));
                } else {
                    config.discard();
                }
            } else if (String_equals(path, PSTR("/reboot.html"))) {
                if (request->hasArg(F("yes"))) {
                    if (request->arg(F("safe_mode")).toInt()) {
                        resetDetector.setSafeMode(1);
                    }
                    request->onDisconnect([]() {
                        config.restartDevice();
                    });
                    mapping = FileMapping(F("/rebooting.html"));
                } else {
                    request->redirect(F("/index.html"));
                }
            } else if (String_equals(path, PSTR("/factory.html"))) {
                if (request->hasArg(F("yes"))) {
                    config.restoreFactorySettings();
                    config.write();
                    RTCMemoryManager::clear();
                    request->onDisconnect([]() {
                        config.restartDevice();
                    });
                    mapping = FileMapping(F("/rebooting.html"));
                } else {
                    request->redirect(F("/index.html"));
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

    return web_server_send_file(mapping, httpHeaders, client_accepts_gzip, request, webTemplate);
}

class WebServerPlugin : public PluginComponent {
public:
    WebServerPlugin() {
        REGISTER_PLUGIN(this);
    }
    virtual PGM_P getName() const {
        return SPGM(http);
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Web server");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return PRIO_HTTP;
    }
    virtual bool allowSafeMode() const override {
        return true;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const override {
        return false;
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual MenuTypeEnum_t getMenuType() const override {
        return NONE;
    }
    virtual PGM_P getConfigureForm() const override {
        return PSTR("remote");
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
};

static WebServerPlugin plugin;

void WebServerPlugin::setup(PluginSetupMode_t mode)
{
    init_web_server();
    if (mode == PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP) {
        invokeReconfigureNow(getName());
    }
}

void WebServerPlugin::reconfigure(PGM_P source)
{
    if (server) {
        delete server;
        server = nullptr;
    }
    init_web_server();
}

void WebServerPlugin::restart()
{
    if (server) {
        delete server;
        server = nullptr;
    }
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
    form.addValidator(new FormRangeValidator(F("Invalid port"), 1, 65535));

#if SPIFFS_SUPPORT && WEBSERVER_TLS_SUPPORT

    form.add(new FormObject<File2String>(F("ssl_certificate"), File2String(FSPGM(server_crt)), nullptr));
    form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key)), nullptr));

#endif

    form.add(F("device_token"), _H_STR_VALUE(Config().device_token));
    form.addValidator(new FormLengthValidator(16, sizeof(Config().device_token) - 1));

    form.finalize();
}

#endif
