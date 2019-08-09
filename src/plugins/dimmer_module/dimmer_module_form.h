/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "ESPAsyncWebServer.h"
#include "Form.h"

void dimmer_module_create_settings_form(AsyncWebServerRequest *request, Form &form);
