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
    Sensor_LM75A(const JsonString &name, TwoWire &wire, uint8_t address = 0x48);
    virtual ~Sensor_LM75A();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void getValues(NamedJsonArray &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

    float readSensor(uint8_t address = 255) {
        return _readSensor(address);
    }
    uint8_t getAddress() const {
        return _address;
    }

private:
    friend Sensor_CCS811;

    String _getId();
    float _readSensor(uint8_t address = 255);

    JsonString _name;
    TwoWire &_wire;
    uint8_t _address;
};

#endif
