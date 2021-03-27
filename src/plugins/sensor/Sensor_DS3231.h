/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR_HAVE_DS3231

#include <Arduino_compat.h>
#include <Wire.h>
#include <vector>
#include <Wire.h>
#include "WebUIComponent.h"
#include "plugins.h"
#include "MQTTSensor.h"

#if !RTC_SUPPORT
#error RTC_SUPPORT not enabled
#endif

#if !defined(RTC_DEVICE_DS3231) || RTC_DEVICE_DS3231 == 0
#error RTC_DEVICE_DS3231 not set
#else
#include <RTClib.h>
#endif

class Sensor_DS3231 : public MQTT::Sensor {
public:
    Sensor_DS3231(const JsonString &name);
    virtual ~Sensor_DS3231();

    virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState() override;
    virtual void getValues(JsonArray &json, bool timer) override;
    virtual void getValues(NamedJsonArray &array, bool timer) override;
    virtual void createWebUI(WebUINS::Root &webUI) override;
    virtual void getStatus(Print &output) override;
    virtual bool getSensorData(String &name, StringVector &values) override;

private:
    float _readSensorTemp();
    time_t _readSensorTime();
    int8_t _readSensorLostPower();
    String _getTimeStr();

    JsonString _name;
    TwoWire *_wire;
};

#endif
