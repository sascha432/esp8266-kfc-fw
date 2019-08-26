/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "ESPAsyncWebServer.h"
#include "Form.h"
#include "PluginComponent.h"
#include "../mqtt/mqtt_component.h"

class DimmerModuleForm : public PluginComponent {
public:
    virtual bool canHandleForm(const String &formName) const override {
        return strcmp_P(formName.c_str(), PSTR("dimmer_cfg")) == 0;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) = 0;
};
