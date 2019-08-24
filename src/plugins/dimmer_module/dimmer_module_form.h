/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "ESPAsyncWebServer.h"
#include "Form.h"
#include "PluginComponent.h"

class DimmerModuleForm : public PluginComponent {
public:
    virtual bool canHandleForm(const String &formName) const override {
        return strcmp_P(formName.c_str(), PSTR("dimmer_cfg")) == 0;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
};
