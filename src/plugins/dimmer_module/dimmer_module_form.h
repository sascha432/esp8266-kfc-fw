/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "ESPAsyncWebServer.h"
#include "Form.h"

class DimmerModuleForm {
public:
    bool canHandleForm(const String &formName) const {
        return formName.equals(F("dimmer_cfg"));
    }
    void createConfigureForm(AsyncWebServerRequest *request, Form &form);
};
