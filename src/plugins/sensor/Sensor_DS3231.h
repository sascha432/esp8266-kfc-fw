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

class Sensor_DS3231 : public MQTTSensor {
public:
    Sensor_DS3231(const JsonString &name);

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;

    virtual void publishState(MQTTClient *client) override;
    virtual void getValues(JsonArray &json) override;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual MQTTSensorSensorType getType() const override;

private:
    float _readSensorTemp();
    time_t _readSensorTime();
    int8_t _readSensorLostPower();
    String _getTimeStr();

    JsonString _name;
    TwoWire *_wire;
};

#endif
