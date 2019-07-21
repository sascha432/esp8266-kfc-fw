
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if WEBSERVER_SUPPORT

#ifndef DEBUG_WEB_SERVER
#define DEBUG_WEB_SERVER 1
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <HttpHeaders.h>

class FSMapping;
class WebTemplate;

// boost CPU speed
class WebServerSetCPUSpeedHelper {
public:
    WebServerSetCPUSpeedHelper(const WebServerSetCPUSpeedHelper &helper) = delete;

    WebServerSetCPUSpeedHelper();
    ~WebServerSetCPUSpeedHelper();
#if defined(ESP8266)
private:
    static uint8_t _counter;
    bool _enabled;
#endif
};

String network_scan_html(int8_t num_networks);
bool web_server_handle_file_read(String path, bool client_accepts_gzip, AsyncWebServerRequest *request);
bool web_server_send_file(String path, HttpHeaders &httpHeaders, bool client_accepts_gzip, FSMapping *mapping, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);
bool web_server_is_authenticated(AsyncWebServerRequest *request);
void web_server_add_handler(AsyncWebHandler* handler);
AsyncWebServer *get_web_server_object();

#endif
