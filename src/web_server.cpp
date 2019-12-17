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
#include <SpeedBooster.h>
#include "progmem_data.h"
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

#if DEBUG_WEB_SERVER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(status, "status");
PROGMEM_STRING_DEF(enabled, "enabled");
PROGMEM_STRING_DEF(disabled, "disabled");

AsyncWebServer *server = nullptr;
FailureCounterContainer loginFailures;

#define U_ATMEGA 254

struct UploadStatus_t {
    AsyncWebServerResponse *response;
    bool error;
    uint8_t command;
};


WebServerSetCPUSpeedHelper::WebServerSetCPUSpeedHelper() : SpeedBooster(
#if defined(ESP8266)
    config._H_GET(Config().flags).webServerPerformanceModeEnabled
#endif
) {
}


AsyncWebServer *get_web_server_object() {
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


bool web_server_is_public_path(const String &pathString) {
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

bool web_server_is_authenticated(AsyncWebServerRequest *request) {
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

bool web_server_client_accepts_gzip(AsyncWebServerRequest *request) {
    auto header = request->getHeader(String(FSPGM(Accept_Encoding)));
    if (!header) {
        return false;
    }
    auto acceptEncoding = header->value().c_str();
    return (strstr_P(acceptEncoding, PSTR("gzip")) || strstr_P(acceptEncoding, PSTR("deflate")));
}

void web_server_add_handler(AsyncWebHandler* handler) {
    server->addHandler(handler);
}

// server->on()
#define on(a, ...)    on(String(a).c_str(), __VA_ARGS__)

void web_server_not_found_handler(AsyncWebServerRequest *request) {

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


void web_server_scan_wifi_handler(AsyncWebServerRequest *request) {
    if (web_server_is_authenticated(request)) {
        HttpHeaders httpHeaders(false);
        httpHeaders.addNoCache();
        AsyncWebServerResponse *response = new AsyncNetworkScanResponse(request->arg(F("hidden")).toInt());
        httpHeaders.setWebServerResponseHeaders(response);
        request->send(response);
    } else {
        request->send(403);
    }
}

void web_server_logout_handler(AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(302);
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache(true);
    httpHeaders.add(HttpCookieHeader(FSPGM(SID)));
    httpHeaders.add(HttpLocationHeader(FSPGM(slash)));
    httpHeaders.setWebServerResponseHeaders(response);
    request->send(response);
}

void web_server_is_alive_handler(AsyncWebServerRequest *request) {
    WebServerSetCPUSpeedHelper setCPUSpeed;
    AsyncWebServerResponse *response = request->beginResponse(200, FSPGM(text_plain), String(request->arg(F("p")).toInt()));
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache();
    httpHeaders.setWebServerResponseHeaders(response);
    request->send(response);
}

void web_server_get_webui_json(AsyncWebServerRequest *request) {
    WebServerSetCPUSpeedHelper setCPUSpeed;
    AsyncJsonResponse *response = new AsyncJsonResponse();
    WsWebUISocket::createWebUIJSON(response->getJsonObject());
    response->updateLength();
    HttpHeaders httpHeaders;
    httpHeaders.addNoCache();
    httpHeaders.setWebServerResponseHeaders(response);
    request->send(response);
}

void web_server_update_handler(AsyncWebServerRequest *request) {
    if (request->_tempObject) {
        UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
        AsyncWebServerResponse *response = nullptr;
#if STK500V1
        if (status->command == U_ATMEGA) {
            if (stk500v1) {
                request->send_P(200, FSPGM(text_plain), PSTR("Upgrade already running"));
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
                httpHeaders.add(HttpLocationHeader(F("/serial_console.html")));
                httpHeaders.replace(HttpConnectionHeader(HttpConnectionHeader::HTTP_CONNECTION_CLOSE));
                httpHeaders.setWebServerResponseHeaders(response);
                request->send(response);
            }
        } else
#endif
        if (!Update.hasError()) {
            Logger_security(F("Firmware upgrade successful"));

            BlinkLEDTimer::setBlink(BlinkLEDTimer::SLOW);
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
            httpHeaders.add(HttpLocationHeader(location));
            httpHeaders.replace(HttpConnectionHeader(HttpConnectionHeader::HTTP_CONNECTION_CLOSE));
            httpHeaders.setWebServerResponseHeaders(response);
            request->send(response);

        } else {
            // SPIFFS.begin();

            BlinkLEDTimer::setBlink(BlinkLEDTimer::SOS);
            StreamString errorStr;
            Update.printError(errorStr);
            Logger_error(F("Firmware upgrade failed: %s"), errorStr.c_str());

            String message = F("<h2>Firmware update failed with an error:<br></h2><h3>");
            message += errorStr;
            message += F("</h3>");

            HttpHeaders httpHeaders(false);
            httpHeaders.addNoCache();
            if (!web_server_send_file(F("/update_fw.html"), httpHeaders, web_server_client_accepts_gzip(request), nullptr, request, new UpgradeTemplate(message))) {
                request->send(404);
            }
        }

    } else {
        request->send(403);
    }
}

void web_server_update_upload_handler(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
#if STK500V1
    static File firmwareTempFile;
#endif
    if (index == 0 && !request->_tempObject && web_server_is_authenticated(request)) {
        request->_tempObject = calloc(sizeof(UploadStatus_t), 1);
        Logger_notice(F("Firmware upload started"));
    }
    UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
    if (status && !status->error) {
        PrintString out;
        bool error = false;
        if (!index) {
            BlinkLEDTimer::setBlink(BlinkLEDTimer::FLICKER);

            size_t size;
            uint8_t command;

            uint8_t imageType = 0;

            if (constexpr_String_equals(request->arg(F("image_type")), PSTR("u_flash"))) { // firmware selected
                imageType = 0;
            }
            else if (constexpr_String_equals(request->arg(F("image_type")), PSTR("u_spiffs"))) { // spiffs selected
                imageType = 1;
            }
#if STK500V1
            else if (constexpr_String_equals(request->arg(F("image_type")), PSTR("u_atmega"))) { // atmega selected
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
                firmwareTempFile = SPIFFS.open(FSPGM(stk500v1_tmp_file), "w");
                _debug_printf_P(PSTR("ATmega fw temp file %u, filename %s\n"), (bool)firmwareTempFile, String(FSPGM(stk500v1_tmp_file)).c_str());
            } else
#endif
            {
                if (imageType) {
                    size = 1048576;
                    command = U_SPIFFS;
                    // SPIFFS.end();
                    // SPIFFS.format();
                } else {
                    size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                    command = U_FLASH;
                }
                status->command = command;
                _debug_printf_P(PSTR("Update Start: %s, image type %d, size %d, command %d\n"), filename.c_str(), imageType, (int)size, command);

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

void init_web_server() {

    if (config._H_GET(Config().flags).webServerMode == HTTP_MODE_DISABLED) {
        return;
    }

    server = _debug_new AsyncWebServer(config._H_GET(Config().http_port));
    // server->addHandler(&events);

    loginFailures.readFromSPIFFS();

#if REST_API_SUPPORT
    // // download /.mappings
    // server->on(F("/rest/KFC/webui_details"), [](AsyncWebServerRequest *request) {
    //     if (web_server_is_authenticated(request)) {
    //         rest_api_kfc_webui_details(request);
    //     } else {
    //         request->send(403);
    //     }
    // });
    // server->addHandler(_debug_new AsyncFileUploadWebHandler(F("/rest/KFC/update_webui"), [](AsyncWebServerRequest *request) {
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
//     server->on(F("/poll_mdns/"), [&httpHeaders](AsyncWebServerRequest *request) {

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
//                 response = request->beginResponse(204, FSPGM(text_html), _sharedEmptyString); // an emtpy response lets the client know to poll again
//             } else {
//                 response = request->beginResponse(503);
//             }
// #if DEBUG
//             httpHeaders.add(MDNS_get_cache_ttl_header());
// #endif
//             httpHeaders.setWebServerResponseHeaders(response);
//             request->send(response);
//         } else {
//             request->send(403);
//         }
//     });
// #endif

    server->on(F("/scan_wifi/"), web_server_scan_wifi_handler);
    server->on(F("/logout"), web_server_logout_handler);
    server->on(F("/is_alive"), web_server_is_alive_handler);
    server->on(F("/webui_get"), web_server_get_webui_json);
    server->on(F("/update"), HTTP_POST, web_server_update_handler, web_server_update_upload_handler);

    server->begin();
    _debug_printf_P(PSTR("HTTP running on port %u\n"), config._H_GET(Config().http_port));
}

PGM_P web_server_get_content_type(const String &path) {

    const char *cPath = path.c_str();
    auto pathLen = path.length();

    if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".html")) || !constexpr_strcmp_end_P(cPath, pathLen, PSTR(".htm"))) {
        return PSTR("text/html");
    } else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".css"))) {
        return PSTR("text/css");
    } else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".css"))) {
        return PSTR("text/css");
    } else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".json")))
        return PSTR("application/json");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".js")))
        return PSTR("application/javascript");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".png")))
        return PSTR("image/png");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".gif")))
        return PSTR("image/gif");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".jpg")))
        return PSTR("image/jpeg");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".ico")))
        return PSTR("image/x-icon");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".svg")))
        return PSTR("image/svg+xml");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".eot")))
        return PSTR("font/eot");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".woff")))
        return PSTR("font/woff");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".woff2")))
        return PSTR("font/woff2");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".ttf")))
        return PSTR("font/ttf");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".xml")))
        return PSTR("text/xml");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".pdf")))
        return PSTR("application/pdf");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".zip")))
        return PSTR("application/zip");
    else if (!constexpr_strcmp_end_P(cPath, pathLen, PSTR(".gz")))
        return PSTR("application/x-gzip");
    else
        return PSTR("text/plain");
}

bool web_server_send_file(String path, HttpHeaders &httpHeaders, bool client_accepts_gzip, FSMapping *mapping, AsyncWebServerRequest *request, WebTemplate *webTemplate) {
    WebServerSetCPUSpeedHelper setCPUSpeed;
    AsyncWebServerResponse *response = nullptr;

    _debug_printf_P(PSTR("web_server_send_file(%s)\n"), path.c_str());
    if (!mapping) {
        if (!(mapping = Mappings::getInstance().find(path))) {
            _debug_printf_P(PSTR("Not found: %s\n"), path.c_str());
            if (webTemplate) {
                delete webTemplate;
            }
            return false;
        }
    }

    // _debug_printf_P(PSTR("Mapping %s, %s, %d, content type %s\n"), mapping->getPath(), mapping->getMappedPath(), mapping->getFileSize(), web_server_get_content_type(path));

    if (webTemplate == nullptr) {
        if (path.charAt(0) == '/' && constexpr_endsWith(path, PSTR(".html"))) {
            String filename = path.substring(1, path.length() - 5);
            auto plugin = PluginComponent::getTemplate(filename);
            if (plugin) {
                webTemplate = plugin->getWebTemplate(filename);
            } else if (nullptr != (plugin = PluginComponent::getForm(filename))) {
                Form *form = _debug_new SettingsForm(nullptr);
                plugin->createConfigureForm(nullptr, *form);
                webTemplate = _debug_new ConfigTemplate(form);
            }
        }
        if (webTemplate == nullptr) {
            if (constexpr_String_equals(path, PSTR("/network.html"))) {
                webTemplate = _debug_new ConfigTemplate(_debug_new NetworkSettingsForm(nullptr));
            } else if (constexpr_String_equals(path, PSTR("/wifi.html"))) {
                webTemplate = _debug_new ConfigTemplate(_debug_new WifiSettingsForm(nullptr));
            } else if (constexpr_String_equals(path, PSTR("/index.html"))) {
                webTemplate = _debug_new StatusTemplate();
            } else if (constexpr_String_equals(path, PSTR("/status.html"))) {
                webTemplate = _debug_new StatusTemplate();
// #if MDNS_SUPPORT
//                 MDNS_async_query_service(); // query service inside loop() and cache results
// #endif
            } else if (constexpr_endsWith(path, PSTR(".html"))) {
                webTemplate = _debug_new WebTemplate();
            }
        }
    }

    if (webTemplate != nullptr) {
         response = _debug_new AsyncTemplateResponse(FPSTR(web_server_get_content_type(path)), mapping, webTemplate);
         httpHeaders.addNoCache(request->method() == HTTP_POST);
    } else {
        response = new AsyncProgmemFileResponse(FPSTR(web_server_get_content_type(path)), mapping);
        httpHeaders.replace(HttpDateHeader(FSPGM(Expires), 86400 * 30));
        httpHeaders.replace(HttpDateHeader(FSPGM(Last_Modified), mapping->getModificatonTime()));
        if (web_server_is_public_path(path)) {
            HttpCacheControlHeader header = HttpCacheControlHeader();
            header.setPublic();
            header.setMaxAge(HttpCacheControlHeader::MAX_AGE_AUTO);
            httpHeaders.replace(header);
        }
    }
    if (mapping->isGzipped()) {
        httpHeaders.add(FSPGM(Content_Encoding), F("gzip"));
    }
    httpHeaders.setWebServerResponseHeaders(response);
    request->send(response);
    return true;
}


bool web_server_handle_file_read(String path, bool client_accepts_gzip, AsyncWebServerRequest *request) {
    _debug_printf_P(PSTR("web_server_handle_file_read: %s\n"), path.c_str());
    WebServerSetCPUSpeedHelper setCPUSpeed;

    if (constexpr_endsWith(path, SPGM(slash))) {
        path += F("index.html");
    }

    if (constexpr_startsWith(path, PSTR("/settings/"))) { // deny access
        request->send(404);
        return false;
    }

    FSMapping *mapping = Mappings::getInstance().find(path);
    if (!mapping) {
        _debug_printf_P(PSTR("Not found: %s\n"), path.c_str());
        return false;
    }
    if (mapping->isGzipped() && !client_accepts_gzip) {
        _debug_printf_P(PSTR("Client does not accept gzip encoding: %s\n"), path.c_str());
        request->send_P(503, FSPGM(text_plain), PSTR("503: Client does not support gzip Content Encoding"));
        return true;
    }

    bool _is_authenticated = web_server_is_authenticated(request);
    WebTemplate *webTemplate = nullptr;
    HttpHeaders httpHeaders;

    if (!web_server_is_public_path(path) && !_is_authenticated) {
        String loginError = F("Your session has expired.");

        Logger_security(F("Authentication failed for %s"), IPAddress(request->client()->getRemoteAddress()).toString().c_str());

        httpHeaders.addNoCache(true);

        // no_cache_headers();
        if (request->method() == HTTP_POST && request->hasArg(F("username")) && request->hasArg(F("password"))) {
            IPAddress remote_addr = request->client()->remoteIP();

            if (loginFailures.isAddressBlocked(remote_addr) == false && request->arg(F("username")) == config._H_STR(Config().device_name) && request->arg(F("password")) == config._H_STR(Config().device_pass)) {
                HttpCookieHeader cookie = HttpCookieHeader(FSPGM(SID));
                cookie.setValue(generate_session_id(config._H_STR(Config().device_name), config._H_STR(Config().device_pass), NULL));
                cookie.setPath(FSPGM(slash));
                httpHeaders.add(cookie);

                if (request->arg(F("keep")) == FSPGM(1)) {
                    auto _time = time(nullptr);
                    if (IS_TIME_VALID(_time)) {
                        cookie.setExpires(_time + 86400 * 30);
                        httpHeaders.add(cookie);
                    }
                }

                _debug_printf_P(PSTR("Login successful, cookie %s\n"), cookie.getValue().c_str());
                _is_authenticated = true;
                Logger_security(F("Login successful from %s"), remote_addr.toString().c_str());
            } else {
                loginError = F("Invalid username or password.");
                const FailureCounter &failure = loginFailures.addFailure(remote_addr);
                Logger_security(F("Login from %s failed %d times since %s"), remote_addr.toString().c_str(), failure.getCounter(), failure.getFirstFailure().c_str());
                return web_server_send_file(FSPGM(login_html), httpHeaders, client_accepts_gzip, nullptr, request, new LoginTemplate(loginError));

            }
        } else {
            if (constexpr_endsWith(path, PSTR(".html"))) {
                return web_server_send_file(FSPGM(login_html), httpHeaders, client_accepts_gzip, nullptr, request, new LoginTemplate(loginError));
            } else {
                request->send(403);
                return true;
            }
        }
    }

    if (_is_authenticated && request->method() == HTTP_POST) {  // http POST processing

        _debug_printf_P(PSTR("HTTP post %s\n"), path.c_str());

        httpHeaders.addNoCache(true);

        if (path.charAt(0) == '/' && constexpr_endsWith(path, PSTR(".html"))) {
            auto plugin = PluginComponent::getForm(path.substring(1, path.length() - 5));
            if (plugin) {
                Form *form = new SettingsForm(request);
                plugin->createConfigureForm(request, *form);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                    plugin->invokeReconfigure(nullptr);
                } else {
                    config.clear();
                }
            }
        }
        if (!webTemplate) {
            if (constexpr_String_equals(path, PSTR("/wifi.html"))) {
                Form *form = new WifiSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                    config.setConfigDirty(true);
                    PluginComponent::getByName(PSTR("cfg"))->invokeReconfigure(PSTR("wifi"));
                } else {
                    config.clear();
                }
            } else if (constexpr_String_equals(path, PSTR("/network.html"))) {
                Form *form = new NetworkSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                    config.setConfigDirty(true);
                    PluginComponent::getByName(PSTR("cfg"))->invokeReconfigure(PSTR("network"));
                } else {
                    config.clear();
                }
            } else if (constexpr_String_equals(path, PSTR("/password.html"))) {
                Form *form = new PasswordSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    auto &flags = config._H_W_GET(Config().flags);
                    flags.isDefaultPassword = false;
                    config.write();
                    PluginComponent::getByName(PSTR("cfg"))->invokeReconfigure(PSTR("password"));
                } else {
                    config.clear();
                }
            } else if (constexpr_String_equals(path, PSTR("/reboot.html"))) {
                if (request->hasArg(F("yes"))) {
                    request->onDisconnect([]() {
                        config.restartDevice();
                    });
                    mapping = Mappings::getInstance().find(F("/rebooting.html"));
                } else {
                    request->redirect(F("/index.html"));
                }
            } else if (constexpr_String_equals(path, PSTR("/factory.html"))) {
                if (request->hasArg(F("yes"))) {
                    config.restoreFactorySettings();
                    config.write();
                    RTCMemoryManager::clear();
                    request->onDisconnect([]() {
                        config.restartDevice();
                    });
                    mapping = Mappings::getInstance().find(F("/rebooting.html"));
                } else {
                    request->redirect(F("/index.html"));
                }
            }
        }

/*        Config &config = _Config.get();


        } else if (constexpr_String_equals(path, PSTR("/pins.html"))) {
            if (request->hasArg(F("led_type"))) {
                BlinkLEDTimer::setBlink(BlinkLEDTimer::OFF);
                _Config.getOptions().setLedMode((LedMode_t)request->arg(F("led_type")).toInt());
                //FormBitValue {FLAGS2_LED_NONE, FLAGS2_LED_SINGLE, FLAGS2_LED_TWO, FLAGS2_LED_RGB}
                config.led_pin = request->arg(F("led_pin")).toInt();
                config_write(false);
                BlinkLEDTimer::setBlink(BlinkLEDTimer::SOLID);
            }
        */
    }

    return web_server_send_file(path, httpHeaders, client_accepts_gzip, mapping, request, webTemplate);
}

class WebServerPlugin : public PluginComponent {
public:
    WebServerPlugin() {
        register_plugin(this);
    }
    PGM_P getName() const;
    PluginPriorityEnum_t getSetupPriority() const override;
    bool allowSafeMode() const override;
    void setup(PluginSetupMode_t mode) override;
    void reconfigure(PGM_P source) override;
    bool hasReconfigureDependecy(PluginComponent *plugin) const override;
    bool hasStatus() const override;
    const String getStatus() override;
    bool canHandleForm(const String &formName) const override;
    void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
};

static WebServerPlugin plugin;

PGM_P WebServerPlugin::getName() const {
    return PSTR("http");
}

WebServerPlugin::PluginPriorityEnum_t WebServerPlugin::getSetupPriority() const {
    return PRIO_HTTP;
}

bool WebServerPlugin::allowSafeMode() const {
    return true;
}

void WebServerPlugin::setup(PluginSetupMode_t mode) {
    init_web_server();
}

void WebServerPlugin::reconfigure(PGM_P source) {
    if (server) {
        delete server;
        server = nullptr;
    }
    init_web_server();
}

bool WebServerPlugin::hasReconfigureDependecy(PluginComponent *plugin) const {
    return false;
}

bool WebServerPlugin::hasStatus() const {
    return true;
}

const String WebServerPlugin::getStatus() {
    auto flags = config._H_GET(Config().flags);
    PrintString out = F("Web server ");
    if (flags.webServerMode != HTTP_MODE_DISABLED) {
        out.printf_P(PSTR("running on port %u"), config._H_GET(Config().http_port));
        #if WEBSERVER_TLS_SUPPORT
            out.print(F(", TLS "));
            if (flags.webServerMode == HTTP_MODE_SECURE) {
                out += FSPGM(enabled);
            } else {
                out += FSPGM(disabled);
            }
        #endif
    } else {
        out += FSPGM(disabled);
    }
    return out;
}

bool WebServerPlugin::canHandleForm(const String &formName) const {
    return strcmp_P(formName.c_str(), PSTR("remote")) == 0;
}

void WebServerPlugin::createConfigureForm(AsyncWebServerRequest *request, Form &form) {

    form.add<uint8_t>(F("http_enabled"), _H_STRUCT_FORMVALUE(Config().flags, uint8_t, webServerMode));
    form.addValidator(new FormRangeValidator(0, HTTP_MODE_SECURE));
#  if WEBSERVER_TLS_SUPPORT
    form.addValidator(new FormMatchValidator(F("There is not enough free RAM for TLS support"), [](FormField &field) {
        return (field.getValue().toInt() != HTTP_MODE_SECURE) || (ESP.getFreeHeap() > 24000);
    }));
#  endif

    form.add<bool>(F("http_perf"), _H_STRUCT_FORMVALUE(Config().flags, bool, webServerPerformanceModeEnabled));
    form.add<uint16_t>(F("http_port"), &config._H_W_GET(Config().http_port));
    form.addValidator(new FormTCallbackValidator<uint16_t>([](uint16_t port, FormField &field) {
#  if WEBSERVER_TLS_SUPPORT
        if (field.getForm().getField(F("http_enabled"))->getValue().toInt() == HTTP_MODE_SECURE) {
            if (port == 0) {
                port = 443;
            }
        } else
#  endif
        {
            if (port == 0) {
                port = 80;
            }
            field.setValue(String(port));
        }
        return true;
    }));
    form.addValidator(new FormRangeValidator(F("Invalid port"), 1, 65535));

#  if SPIFFS_SUPPORT && WEBSERVER_TLS_SUPPORT

    form.add(new FormObject<File2String>(F("ssl_certificate"), File2String(FSPGM(server_crt)), nullptr));
    form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key)), nullptr));

#  endif

    form.finalize();
}

#endif
