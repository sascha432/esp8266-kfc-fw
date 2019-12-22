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
    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) = 0;
};
