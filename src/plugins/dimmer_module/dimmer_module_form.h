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
    virtual PGM_P getConfigureForm() const override {
        return PSTR("dimmer_cfg");
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;
    virtual MQTTComponent::MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) = 0;
};
