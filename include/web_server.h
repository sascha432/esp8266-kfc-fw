
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if WEBSERVER_SUPPORT

#ifndef DEBUG_WEB_SERVER
#define DEBUG_WEB_SERVER 0
#endif

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <HttpHeaders.h>
#include <SpeedBooster.h>

class WebServerSetCPUSpeedHelper : public SpeedBooster {
public:
    WebServerSetCPUSpeedHelper();
};

class WebTemplate;

typedef std::function<void(size_t position, size_t size)> UpdateFirmwareCallback_t;

String network_scan_html(int8_t num_networks);
void web_server_set_update_firmware_callback(UpdateFirmwareCallback_t callback);
bool web_server_handle_file_read(String path, bool client_accepts_gzip, AsyncWebServerRequest *request);
bool web_server_send_file(String path, HttpHeaders &httpHeaders, bool client_accepts_gzip, const struct FSMappingEntry_tag *mapping, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);
bool web_server_is_authenticated(AsyncWebServerRequest *request);
void web_server_add_handler(AsyncWebHandler* handler);
void web_server_add_handler(const String &uri, ArRequestHandlerFunction onRequest);
AsyncWebServer *get_web_server_object();

#endif
