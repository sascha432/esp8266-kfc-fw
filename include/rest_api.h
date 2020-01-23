/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "web_server.h"

// void rest_api_kfc_webui_details(AsyncWebServerRequest *request);
void rest_api_kfc_update_webui(AsyncWebServerRequest *request);
void rest_api_setup();
