/**
 * Author: sascha_lammers@gmx.de
 */

#if WEBSERVER_SUPPORT

#include <Arduino_compat.h>
#include <ArduinoOTA.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include "progmem_data.h"
#include "web_server.h"
#include "rest_api.h"
#include "async_web_response.h"
#include "async_web_handler.h"
#include "kfc_fw_config.h"
#include "failure_counter.h"
#include "fs_mapping.h"
#include "session.h"
#include "templates.h"
#include "timezone.h"
#include "web_socket.h"
#include "kfc_fw_config.h"
#include "plugins.h"

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

struct UploadStatus_t {
    AsyncWebServerResponse *response;
    bool error;
    uint8_t command;
};

#if defined(ESP8266)
uint8_t WebServerSetCPUSpeedHelper::_counter = 0;
#endif

WebServerSetCPUSpeedHelper::WebServerSetCPUSpeedHelper() {
#if defined(ESP8266)
    _enabled = config._H_GET(Config().flags).webServerPerformanceModeEnabled;
    _debug_printf_P(PSTR("WebServerSetCPUSpeedHelper(): counter=%d, CPU=%d, boost=%d\n"), _counter, system_get_cpu_freq(), _enabled);
    _counter++;
    if (_counter == 1 && system_get_cpu_freq() != SYS_CPU_160MHZ && _enabled) {
        system_update_cpu_freq(SYS_CPU_160MHZ);
    }
#endif
}

WebServerSetCPUSpeedHelper::~WebServerSetCPUSpeedHelper() {
#if defined(ESP8266)
    _counter--;
    if (_counter == 0 && _enabled) {
        system_update_cpu_freq(SYS_CPU_80MHZ);
    }
    _debug_printf_P(PSTR("~WebServerSetCPUSpeedHelper(): counter=%d, CPU=%d, boost=%d\n"), _counter, system_get_cpu_freq(), _enabled);
#endif
}

AsyncWebServer *get_web_server_object() {
    return server;
}

const String web_server_get_status() {
    auto flags = config._H_GET(Config().flags);
    PrintString out = F("Web server ");
    if (flags.webServerMode != HTTP_MODE_DISABLED) {
        out.printf_P(PSTR("running on port %u"), config._H_GET(Config().http_port));
        #if WEBSERVER_TLS_SUPPORT
            out.print(F(", TLS "));
            if (flags.webServerMode == HTTP_MODE_SECURE) {
                out += SPGM(enabled);
            } else {
                out += SPGM(disabled);
            }
        #endif
    } else {
        out += SPGM(disabled);
    }
    return out;
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
    auto acceptEncoding = request->header(FSPGM(Accept_Encoding)).c_str();
    return (strstr_P(acceptEncoding, PSTR("gzip")) || strstr_P(acceptEncoding, PSTR("deflate")));
}

void web_server_add_handler(AsyncWebHandler* handler) {
    server->addHandler(handler);
}

// server->on()
#define on(a, ...)    on(String(a).c_str(), __VA_ARGS__)

void web_server_not_found_handler(AsyncWebServerRequest *request) {

    WebServerSetCPUSpeedHelper setCPUSpeed;
#if HUE_EMULATION
    if (hue_onNotFound(request)) {
        return;
    }
#endif

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
    // httpHeaders.add((HttpCookieHeader(F("_SID")))->hasExpired());
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

void web_server_update_handler(AsyncWebServerRequest *request) {
    if (request->_tempObject) {
        UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
        AsyncWebServerResponse *response = nullptr;
        if (!Update.hasError()) {
            Logger_security(F("Firmware upgrade successful"));

            if (status->command == U_FLASH) {
                config_set_blink(BLINK_SLOW);
                request->onDisconnect([]() {
                    Logger_notice(F("Rebooting after upgrade"));
                    Scheduler.addTimer(2000, false, [](EventScheduler::TimerPtr timer) {
                        config.restartDevice();
                    });
                });
            } else {
                config_set_blink(BLINK_OFF);
            }

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

            config_set_blink(BLINK_SOS);
            PrintString errorStr;
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
    if (index == 0 && !request->_tempObject && web_server_is_authenticated(request)) {
        request->_tempObject = calloc(sizeof(UploadStatus_t), 1);
        Logger_notice(F("Firmware upload started"));
    }
    UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
    if (status && !status->error) {
        PrintString out;
        bool error = false;
        if (!index) {
            config_set_blink(BLINK_FLICKER);

            Update.runAsync(true);
            size_t size;
            uint8_t command;

            uint8_t spiffs = 0;

            if (constexpr_String_equals(request->arg(F("image_type")), PSTR("u_flash"))) { // firmware selected
                spiffs = 0;
            } else if (constexpr_String_equals(request->arg(F("image_type")), PSTR("u_spiffs"))) { // spiffs selected
                spiffs = 1;
            } else if (strstr_P(filename.c_str(), PSTR("spiffs"))) { // auto select
                spiffs = 2;
            }

            if (spiffs) {
                size = 1048576;
                command = U_SPIFFS;
                // SPIFFS.end();
                // SPIFFS.format();
            } else {
                size = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                command = U_FLASH;
            }
            status->command = command;
            _debug_printf_P(PSTR("Update Start: %s, spiffs %d, size %d, command %d\n"), filename.c_str(), spiffs, (int)size, command);

            if (!Update.begin(size, command)) {
                error = true;
            }
        }
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

    server->onRequestBody(hue_onRequestBody);
    hue_register_port(config.get<uint16_t>(_H(Config().http_port)));

#endif

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
    server->on(F("/update"), HTTP_POST, web_server_update_handler, web_server_update_upload_handler);

    server->begin();
    _debug_printf_P(PSTR("HTTP running on port %u\n"), config._H_GET(Config().http_port));
}

void web_server_reconfigure() {
    if (server) {
        delete server;
        server = nullptr;
    }
    init_web_server();
}

PGM_P web_server_get_content_type(const String &path) {

    const char *cPath = path.c_str();
    size_t pathLen = path.length();

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
            auto plugin = get_plugin_by_name(path.substring(1, path.length() - 5));
            if (plugin) {
                auto callback = plugin->getConfigureForm();
                if (callback) {
                    Form *form = _debug_new SettingsForm(nullptr);
                    callback(nullptr, *form);
                    webTemplate = _debug_new ConfigTemplate(form);
                }
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
                if (request->arg(F("keep")) == FSPGM(1)) {
                    cookie.setExpires(time(NULL) + 86400 * 30);
                }
                httpHeaders.add(cookie);

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
            auto plugin = get_plugin_by_name(path.substring(1, path.length() - 5));
            if (plugin) {
                auto callback = plugin->getConfigureForm();
                if (callback) {
                    Form *form = new SettingsForm(request);
                    callback(request, *form);
                    webTemplate = new ConfigTemplate(form);
                    if (form->validate()) {
                        config.write();
                    }
                    plugin->callReconfigurePlugin();
                }
            }
        }
        if (!webTemplate) {
            if (constexpr_String_equals(path, PSTR("/wifi.html"))) {
                Form *form = new WifiSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                }
            } else if (constexpr_String_equals(path, PSTR("/network.html"))) {
                Form *form = new NetworkSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config.write();
                }
            } else if (constexpr_String_equals(path, PSTR("/password.html"))) {
                Form *form = new PasswordSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    auto &flags = config._H_W_GET(Config().flags);
                    flags.isDefaultPassword = false;
                    config.write();
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
            }
        }

/*        Config &config = _Config.get();


        } else if (constexpr_String_equals(path, PSTR("/pins.html"))) {
            if (request->hasArg(F("led_type"))) {
                config_set_blink(BLINK_OFF);
                _Config.getOptions().setLedMode((LedMode_t)request->arg(F("led_type")).toInt());
                //FormBitValue {FLAGS2_LED_NONE, FLAGS2_LED_SINGLE, FLAGS2_LED_TWO, FLAGS2_LED_RGB}
                config.led_pin = request->arg(F("led_pin")).toInt();
                config_write(false);
                config_set_blink(BLINK_SOLID);
            }
        */
    }

    return web_server_send_file(path, httpHeaders, client_accepts_gzip, mapping, request, webTemplate);
}

void web_server_create_settings_form(AsyncWebServerRequest *request, Form &form) {

    form.add<uint8_t>(F("http_enabled"), config._H_GET(Config().flags).webServerMode, [](uint8_t value, FormField *) {
        auto &flags = config._H_W_GET(Config().flags);
        flags.webServerMode = value;

    });
    form.addValidator(new FormRangeValidator(0, HTTP_MODE_SECURE));
#  if WEBSERVER_TLS_SUPPORT
    addValidator(new FormMatchValidator(F("There is not enough free RAM for TLS support"), [](FormField *field) {
        return (field->getValue().toInt() != HTTP_MODE_SECURE) || (ESP.getFreeHeap() > 24000);
    }));
#  endif

    form.add<bool>(F("http_perf"), config._H_GET(Config().flags).webServerPerformanceModeEnabled, [](bool value, FormField *) {
        auto &flags = config._H_W_GET(Config().flags);
        flags.webServerPerformanceModeEnabled = value;
    });
    form.add<uint16_t>(F("http_port"), config._H_GET(Config().http_port), [&](uint16_t port, FormField *field) {
#  if WEBSERVER_TLS_SUPPORT
        if (field->getForm()->getField(0)->getValue().toInt() == HTTP_MODE_SECURE) {
            if (port == 0) {
                port = 443;
            }
        } else
#  endif
        {
            if (port == 0) {
                port = 80;
            }
            field->setValue(String(port));
        }
        config._H_SET(Config().http_port, port);
    });
    form.addValidator(new FormRangeValidator(F("Invalid port"), 0, 65535));

#  if SPIFFS_SUPPORT && WEBSERVER_TLS_SUPPORT

    form.add(new FormObject<File2String>(F("ssl_certificate"), File2String(FSPGM(server_crt))));
    form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key))));

#  endif

    form.finalize();
}

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ http,
/* setupPriority            */ PLUGIN_PRIO_HTTP,
/* allowSafeMode            */ true,
/* autoSetupWakeUp          */ false,
/* rtcMemoryId              */ 0,
/* setupPlugin              */ init_web_server,
/* statusTemplate           */ web_server_get_status,
/* configureForm            */ web_server_create_settings_form,
/* reconfigurePlugin        */ web_server_reconfigure,
/* reconfigure Dependencies */ nullptr,
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ nullptr
);

#endif
