/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_MOTION_SENSOR

#include <Arduino_compat.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

using KFCConfigurationClasses::Plugins;


class Sensor_Motion : public MQTT::Sensor {
public:

    Sensor_Motion(const String &name);
    virtual ~Sensor_Motion();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;

    virtual bool hasForm() const {
        return true;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

    virtual void reconfigure(PGM_P source) override;

private:
    String _name;
};

#endif
