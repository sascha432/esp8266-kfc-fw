/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_LM75A

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

class Sensor_CCS811;

class Sensor_LM75A : public MQTT::Sensor {
public:
    Sensor_LM75A(const String &name, uint8_t address = 0x48, TwoWire &wire = Wire);
    virtual ~Sensor_LM75A();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(WebUINS::Events &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    float readSensor() {
        return _readSensor();
    }

    uint8_t getAddress() const {
        return _address;
    }

    const String &getSensorName() const {
        return _name;
    }

private:
    friend Sensor_CCS811;

    String _getId();
    float _readSensor();

    String _name;
    TwoWire *_wire;
    uint8_t _address;
};

#endif
