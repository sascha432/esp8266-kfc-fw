/**
 * Author: sascha_lammers@gmx.de
 */

#if WEBSERVER_SUPPORT

#include <Arduino_compat.h>
#include <ArduinoOTA.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <EventScheduler.h>
#include "web_server.h"
#include "rest_api.h"
#include "async_web_response.h"
#include "async_web_handler.h"
#include "kfc_fw_config.h"
#include "debug_helper.h"
#include "failure_counter.h"
#include "fs_mapping.h"
#include "session.h"
#include "templates.h"
#include "timezone.h"
#include "web_socket.h"
#include "kfc_fw_config.h"
#include "plugins.h"

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

AsyncWebServer *get_web_server_object() {
    return server;
}

const String web_server_get_status() {
    PrintString out = F("Web server ");
    auto &_config = _Config.get();
    if (_Config.getOptions().isHttpServer()) {
        out.printf_P(PSTR("running on port %u"), _config.http_port);
        #if WEBSERVER_TLS_SUPPORT
            out.print(F(", TLS "));
            if (_Config.getOptions().isHttpServerTLS()) {
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
//     debug_printf_P("log %s %d\n", url().c_str(), response->_);
// }
// #endif

bool is_public(String path) {
    return path.indexOf(F("..")) == -1 && (path.startsWith(F("/css/")) || path.startsWith(F("/js/")) || path.startsWith(F("/images/")) || path.startsWith(F("/fonts/")));
}

bool is_authenticated(AsyncWebServerRequest *request) {
    String SID;
    if ((request->hasArg(FSPGM(SID)) && (SID = request->arg(FSPGM(SID)))) || HttpCookieHeader::parseCookie(request, FSPGM(SID), SID)) {

        debug_printf_P(PSTR("is_authenticated with SID '%s' from %s\n"), SID.c_str(), request->client()->remoteIP().toString().c_str());
        if (SID.length() == 0) {
            return false;
        }
        if (verify_session_id(SID.c_str(), _Config.get().device_name, _Config.get().device_pass)) {
            return true;
        }
    }
    debug_println(F("is_authenticated failed"));
    return false;
}

uint8_t set_cpu_speed_for_request_stack_counter = 0;

void set_cpu_speed_for_request(AsyncWebServerRequest *request) {

    set_cpu_speed_for_request_stack_counter++;
#if defined(ESP8266)
    if (set_cpu_speed_for_request_stack_counter == 1 && _Config.getOptions().isHttpPerformanceMode() && system_get_cpu_freq() != SYS_CPU_160MHZ) {
        system_update_cpu_freq(SYS_CPU_160MHZ);
    }
#endif

    String url = request->url();
#if DEBUG
    ulong start = millis();
    request->onDisconnect([start, url]() {
#else
    request->onDisconnect([url]() {
#endif
        set_cpu_speed_for_request_stack_counter--;
#if DEBUG
        int dur = millis() - start;
        debug_printf_P(PSTR("request %s took %.4fs @ %dMhz, stack %d\n"), url.c_str(), dur / 1000.0, system_get_cpu_freq(), set_cpu_speed_for_request_stack_counter);
#endif
#if defined(ESP8266)
    if (set_cpu_speed_for_request_stack_counter == 0 && _Config.getOptions().isHttpPerformanceMode()) {
        system_update_cpu_freq(SYS_CPU_80MHZ);
    }
#endif
    });
}

bool _client_accepts_gzip(AsyncWebServerRequest *request) {
    String acceptEncoding = request->header(FSPGM(Accept_Encoding));
    return (acceptEncoding.indexOf(F("gzip")) != -1 || acceptEncoding.indexOf(F("deflate")) != -1);
}

bool init_request_filter(AsyncWebServerRequest *request) {
    set_cpu_speed_for_request(request);
    return true;
}

void init_web_server(bool isSafeMode) {

    if (!_Config.getOptions().isHttpServer()) {
        return;
    }

    server = new AsyncWebServer(_Config.get().http_port);
    // server->addHandler(&events);

    loginFailures.readFromSPIFFS();

#if REST_API_SUPPORT
    // // download /.mappings
    // server->on(str_P(F("/rest/KFC/webui_details")), [](AsyncWebServerRequest *request) {
    //     if (is_authenticated(request)) {
    //         rest_api_kfc_webui_details(request);
    //     } else {
    //         request->send(403);
    //     }
    // });
    server->addHandler(new AsyncFileUploadWebHandler(F("/rest/KFC/update_webui"), [](AsyncWebServerRequest *request) {
        if (request->_tempObject) {
            rest_api_kfc_update_webui(request);
        } else {
            request->send(403);
        }
    }));
#endif

#if HUE_EMULATION

    server->onRequestBody(hue_onRequestBody);
    hue_register_port(_Config.get().http_port);

#endif

    server->onNotFound([](AsyncWebServerRequest *request) {

#if HUE_EMULATION
        if (hue_onNotFound(request)) {
            return;
        }
#endif

        set_cpu_speed_for_request(request);

        if (!handle_file_read(request->url(), _client_accepts_gzip(request), request)) {
            request->send(404);
        }

    });

    HttpHeaders &httpHeaders = HttpHeaders::getInstance();

// #if MDNS_SUPPORT
//     server->on(str_P(F("/poll_mdns/")), [&httpHeaders](AsyncWebServerRequest *request) {

//         set_cpu_speed_for_request(request);

//         httpHeaders.addNoCache();
//         if (is_authenticated(request)) {
//             AsyncWebServerResponse *response;
//             bool isRunning = false;//MDNS_async_query_running();
//             String &result = MDNS_get_html_result();

//             debug_printf_P(PSTR("HTTP MDS: Query running %d, response length %d\n"), isRunning, result.length());

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
//     }).setFilter(init_request_filter);
// #endif

    server->on(str_P(F("/scan_wifi/")), [&httpHeaders](AsyncWebServerRequest *request) {
        httpHeaders.addNoCache();
        if (is_authenticated(request)) {
            AsyncWebServerResponse *response = new AsyncNetworkScanResponse(request->arg(F("hidden")).toInt());
            httpHeaders.setWebServerResponseHeaders(response);
            request->send(response);
        } else {
            request->send(403);
        }
    }).setFilter(init_request_filter);

    server->on(str_P(F("/logout")), [&httpHeaders](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(302);
        httpHeaders.init();
        httpHeaders.addNoCache(true);
        httpHeaders.add(HttpCookieHeader(FSPGM(SID)));
        // httpHeaders.add((HttpCookieHeader(F("_SID")))->hasExpired());
        httpHeaders.add(HttpLocationHeader(FSPGM(slash)));
        httpHeaders.setWebServerResponseHeaders(response);
        request->send(response);
    }).setFilter(init_request_filter);

    server->on(str_P(F("/is_alive")), [&httpHeaders](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(200, FSPGM(text_plain), String(request->arg(F("p")).toInt()));
        httpHeaders.init();
        httpHeaders.addNoCache();
        httpHeaders.setWebServerResponseHeaders(response);
        request->send(response);
    }).setFilter(init_request_filter);

    server->on(str_P(F("/update")), HTTP_POST, [&httpHeaders](AsyncWebServerRequest *request) {
        if (request->_tempObject) {
            UploadStatus_t *status = reinterpret_cast<UploadStatus_t *>(request->_tempObject);
            AsyncWebServerResponse *response = nullptr;
            if (!Update.hasError()) {
                Logger_security(F("Firmware upgrade successful"));

                if (status->command == U_FLASH) {
                    config_set_blink(BLINK_SLOW);
                    request->onDisconnect([]() {
                        Logger_notice(F("Rebooting after upgrade"));
                        Scheduler.addTimer(2000, false, [](EventScheduler::EventTimerPtr timer) {
                            config_restart();
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

                if (!send_file(F("/update_fw.html"), _client_accepts_gzip(request), nullptr, request, new UpgradeTemplate(message))) {
                    request->send(404);
                }
            }

        } else {
            request->send(403);
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (index == 0 && !request->_tempObject && is_authenticated(request)) {
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
                if (request->arg(F("image_type")).equals(F("u_flash"))) { // firmware selected
                    spiffs = 0;
                } else if (request->arg(F("image_type")).equals(F("u_spiffs"))) { // spiffs selected
                    spiffs = 1;
                } else if (filename.indexOf(F("spiffs")) != -1) { // auto select
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
                debug_printf_P(PSTR("Update Start: %s, spiffs %d, size %d, command %d\n"), filename.c_str(), spiffs, (int)size, command);

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
                    debug_printf_P(PSTR("Update Success: %uB\n"), index + len);
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
   }).setFilter(init_request_filter);

    server->begin();
    debug_printf_P(PSTR("HTTP running on port %hu\n"), _Config.get().http_port);
}

void web_server_reconfigure() {
    if (server) {
        delete server;
        server = nullptr;
    }
    init_web_server(false);
}

String get_content_type(const String &path) {
    if (path.endsWith(F(".html")))
        return F("text/html");
    else if (path.endsWith(F(".htm")))
        return F("text/html");
    else if (path.endsWith(F(".css")))
        return F("text/css");
    else if (path.endsWith(F(".json")))
        return F("application/json");
    else if (path.endsWith(F(".js")))
        return F("application/javascript");
    else if (path.endsWith(F(".png")))
        return F("image/png");
    else if (path.endsWith(F(".gif")))
        return F("image/gif");
    else if (path.endsWith(F(".jpg")))
        return F("image/jpeg");
    else if (path.endsWith(F(".ico")))
        return F("image/x-icon");
    else if (path.endsWith(F(".svg")))
        return F("image/svg+xml");
    else if (path.endsWith(F(".eot")))
        return F("font/eot");
    else if (path.endsWith(F(".woff")))
        return F("font/woff");
    else if (path.endsWith(F(".woff2")))
        return F("font/woff2");
    else if (path.endsWith(F(".ttf")))
        return F("font/ttf");
    else if (path.endsWith(F(".xml")))
        return F("text/xml");
    else if (path.endsWith(F(".pdf")))
        return F("application/pdf");
    else if (path.endsWith(F(".zip")))
        return F("application/zip");
    else if (path.endsWith(F(".gz")))
        return F("application/x-gzip");
    else
        return F("text/plain");
}

bool send_file(String path, bool client_accepts_gzip, FSMapping *mapping, AsyncWebServerRequest *request, WebTemplate *webTemplate) {

    AsyncWebServerResponse *response = nullptr;
    HttpHeaders &httpHeaders = HttpHeaders::getInstance();

    debug_printf_P(PSTR("send_file(%s)\n"), path.c_str());
    if (!mapping) {
        if (!(mapping = Mappings::getInstance().find(path))) {
            debug_printf_P(PSTR("Not found: %s\n"), path.c_str());
            if (webTemplate) {
                delete webTemplate;
            }
            return false;
        }
    }

    // debug_printf_P(PSTR("Mapping %s, %s, %d, content type %s\n"), mapping->getPath(), mapping->getMappedPath(), mapping->getFileSize(), get_content_type(path).c_str());

    if (webTemplate == nullptr) {
        if (path.charAt(0) == '/' && path.endsWith(F(".html"))) {
            auto plugin = get_plugin_by_name(path.substring(1, path.length() - 5));
            if (plugin && plugin->configureForm) {
                Form *form = new SettingsForm(nullptr);
                plugin->configureForm(nullptr, *form);
                webTemplate = new ConfigTemplate(form);
            }
        }
        if (webTemplate == nullptr) {
            if (path.equals(F("/network.html"))) {
                webTemplate = new ConfigTemplate(new NetworkSettingsForm(nullptr));
            } else if (path.equals(F("/wifi.html"))) {
                webTemplate = new ConfigTemplate(new WifiSettingsForm(nullptr));
#if SYSTEM_STATS
            } else if (path.equals(F("/syst.html"))) {
                webTemplate = new StatusTemplate();
#endif
            } else if (path.equals(F("/index.html"))) {
                webTemplate = new StatusTemplate();
            } else if (path.equals(F("/status.html"))) {
                webTemplate = new StatusTemplate();
// #if MDNS_SUPPORT
//                 MDNS_async_query_service(); // query service inside loop() and cache results
// #endif
            } else if (path.endsWith(F(".html"))) {
                webTemplate = new WebTemplate();
            }
        }
    }

    if (webTemplate != nullptr) {
         response = new AsyncTemplateResponse(get_content_type(path), mapping, webTemplate);
         httpHeaders.addNoCache(request->method() == HTTP_POST);
    } else {
        response = new AsyncProgmemFileResponse(get_content_type(path), mapping);
        httpHeaders.replace(HttpDateHeader(FSPGM(Expires), 86400 * 30));
        httpHeaders.replace(HttpDateHeader(FSPGM(Last_Modified), mapping->getModificatonTime()));
        if (is_public(path)) {
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


bool handle_file_read(String path, bool client_accepts_gzip, AsyncWebServerRequest *request) {
    debug_printf_P(PSTR("handle_file_read: %s\n"), path.c_str());

    if (path.endsWith(FSPGM(slash))) {
        path += F("index.html");
    }

    if (path.startsWith(F("/settings/"))) { // deny access
        request->send(404);
        return false;
    }

    FSMapping *mapping = Mappings::getInstance().find(path);
    if (!mapping) {
        debug_printf_P(PSTR("Not found: %s\n"), path.c_str());
        return false;
    }
    if (mapping->isGzipped() && !client_accepts_gzip) {
        debug_printf_P(PSTR("Client does not accept gzip encoding: %s\n"), path.c_str());
        request->send_P(503, FSPGM(text_plain), PSTR("503: Client does not support gzip Content Encoding"));
        return true;
    }

    bool _is_authenticated = is_authenticated(request);
    WebTemplate *webTemplate = nullptr;
    HttpHeaders &httpHeaders = HttpHeaders::getInstance();

    if (!is_public(path) && !_is_authenticated) {
        String loginError = F("Your session has expired.");

        Logger_security(F("Authentication failed for %s"), to_c_str(IPAddress(request->client()->getRemoteAddress())));

        httpHeaders.addNoCache(true);

        // no_cache_headers();
        if (request->method() == HTTP_POST && request->hasArg(F("username")) && request->hasArg(F("password"))) {
            Config &config = _Config.get();
            IPAddress remote_addr = request->client()->remoteIP();

            if (loginFailures.isAddressBlocked(remote_addr) == false && request->arg(F("username")) == config.device_name && request->arg(F("password")) == config.device_pass) {
                HttpCookieHeader cookie = HttpCookieHeader(FSPGM(SID));
                cookie.setValue(generate_session_id(config.device_name, config.device_pass, NULL));
                cookie.setPath(FSPGM(slash));
                if (request->arg(F("keep")) == FSPGM(1)) {
                    cookie.setExpires(time(NULL) + 86400 * 30);
                }
                httpHeaders.add(cookie);

                debug_printf_P(PSTR("Login successful, cookie %s\n"), cookie.getValue().c_str());
                _is_authenticated = true;
                Logger_security(F("Login successful from %s"), to_c_str(remote_addr));
            } else {
                loginError = F("Invalid username or password.");
                const FailureCounter &failure = loginFailures.addFailure(remote_addr);
                Logger_security(F("Login from %s failed %d times since %s"), to_c_str(remote_addr), failure.getCounter(), failure.getFirstFailure().c_str());
                return send_file(FSPGM(login_html), client_accepts_gzip, nullptr, request, new LoginTemplate(loginError));

            }
        } else {
            if (path.endsWith(F(".html"))) {
                return send_file(FSPGM(login_html), client_accepts_gzip, nullptr, request, new LoginTemplate(loginError));
            } else {
                request->send(403);
                return true;
            }
        }
    }

    if (_is_authenticated && request->method() == HTTP_POST) {  // http POST processing

        debug_printf_P(PSTR("HTTP post %s\n"), path.c_str());

        httpHeaders.addNoCache(true);

        if (path.charAt(0) == '/' && path.endsWith(F(".html"))) {
            auto plugin = get_plugin_by_name(path.substring(1, path.length() - 5));
            if (plugin && plugin->configureForm) {
                Form *form = new SettingsForm(request);
                plugin->configureForm(request, *form);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config_write();
                }
                if (plugin->reconfigurePlugin) {
                    plugin->reconfigurePlugin();
                }
            }
        }
        if (!webTemplate) {
            if (path.equals(F("/wifi.html"))) {
                Form *form = new WifiSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config_write();
                }
            } else if (path.equals(F("/network.html"))) {
                Form *form = new NetworkSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config_write();
                }
            } else if (path.equals(F("/password.html"))) {
                Form *form = new PasswordSettingsForm(request);
                webTemplate = new ConfigTemplate(form);
                if (form->validate()) {
                    config_write();
                }
            } else if (path.equals(F("/reboot.html"))) {
                if (request->hasArg(F("yes"))) {
                    request->onDisconnect([]() {
                        config_restart();
                    });
                    mapping = Mappings::getInstance().find(F("/rebooting.html"));
                } else {
                    request->redirect(F("/index.html"));
                }
            }
        }

/*        Config &config = _Config.get();


        } else if (path.equals(F("/pins.html"))) {
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

    return send_file(path, client_accepts_gzip, mapping, request, webTemplate);
}

void web_server_create_settings_form(AsyncWebServerRequest *request, Form &form) {

    auto &_config = _Config.get();

    form.add<ConfigFlags_t, 3>(F("http_enabled"), &_config.flags, std::array<ConfigFlags_t, 3>({FormBitValue_UNSET_ALL, FLAGS_HTTP_ENABLED, FLAGS_HTTP_TLS}));
#  if WEBSERVER_TLS_SUPPORT
    addValidator(new FormMatchValidator(F("There is not enough free RAM for TLS support"), [](FormField *field) {
        return (field->getValue().toInt() != HTTP_MODE_SECURE) || (ESP.getFreeHeap() > 24000);
    }));
#  endif

    form.add<ConfigFlags_t, 2>(F("http_perf"), &_Config.get().flags, std::array<ConfigFlags_t, 2>({FormBitValue_UNSET_ALL, FLAGS_HTTP_PERF_160}));
    form.add<uint16_t>(F("http_port"), _config.http_port, [&](uint16_t port, FormField *field) {
#  if WEBSERVER_TLS_SUPPORT
        assert(field->getForm()->getField(0)->getName().equals(F("http_enabled")) == true);
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
        _config.http_port = port;
    });
    form.addValidator(new FormRangeValidator(F("Invalid port"), 0, 65535));

#  if SPIFFS_SUPPORT && WEBSERVER_TLS_SUPPORT

    form.add(new FormObject<File2String>(F("ssl_certificate"), File2String(FSPGM(server_crt))));
    form.add(new FormObject<File2String>(F("ssl_key"), File2String(FSPGM(server_key))));

#  endif

    form.finalize();
}

void add_plugin_web_server() {
    Plugin_t plugin;

    init_plugin(F("remote"), plugin, 10);
    plugin.setupPlugin = init_web_server;
    plugin.statusTemplate = web_server_get_status;
    plugin.configureForm = web_server_create_settings_form;
    plugin.reconfigurePlugin = web_server_reconfigure;
    register_plugin(plugin);
}

#endif
