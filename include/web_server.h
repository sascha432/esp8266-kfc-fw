
/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <ESPAsyncWebServer.h>
#include <HttpHeaders.h>

class FSMapping;
class WebTemplate;

String network_scan_html(int8_t num_networks);
bool handle_file_read(String path, bool client_accepts_gzip, AsyncWebServerRequest *request);
bool send_file(String path, bool client_accepts_gzip, FSMapping *mapping, AsyncWebServerRequest *request, WebTemplate *webTemplate = nullptr);
bool is_authenticated(AsyncWebServerRequest *request);
bool init_request_filter(AsyncWebServerRequest *request);
AsyncWebServer *get_web_server_object();
