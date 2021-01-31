/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef DEBUG_IOT_SENSOR
#define DEBUG_IOT_SENSOR                    0
#endif

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <vector>
#include <KFCJson.h>
#include <KFCForms.h>
#include "../mqtt/mqtt_client.h"
#include "EnumBitset.h"

DECLARE_ENUM(MQTTSensorSensorType, uint8_t,
    UNKNOWN = 0,
    LM75A,
    BME280,
    BME680,
    CCS811,
    HLW8012,
    HLW8032,
    BATTERY,
    DS3231,
    INA219,
    DHTxx,
    DIMMER_METRICS,
    SYSTEM_METRICS
);

#if DEBUG_IOT_SENSOR
#define REGISTER_SENSOR_CLIENT(sensor)                  { ::printf(PSTR("registerClient " __FUNCTION__ " "__FILE__ ":" __LINE__ "\n"); registerClient(sensor); }
#else
#define REGISTER_SENSOR_CLIENT(sensor)                  registerClient(sensor);
#endif
#define UNREGISTER_SENSOR_CLIENT(sensor)                unregisterClient(sensor);

class MQTTSensor : public MQTTComponent {
public:
    static constexpr uint16_t DEFAULT_UPDATE_RATE = 10;
    static constexpr uint16_t DEFAULT_MQTT_UPDATE_RATE = 30;

    using SensorType = MQTTSensorSensorType;

    MQTTSensor();
    virtual ~MQTTSensor();

    // REGISTER_SENSOR_CLIENT(this) must be called in the constructor of the sensor
    template <class T>
    void registerClient(T sensor) {
        MQTTClient::safeRegisterComponent(sensor);
    }

    // UNREGISTER_SENSOR_CLIENT(this) must be called in the destructor of the sensor
    template <class T>
    void unregisterClient(T sensor) {
        MQTTClient::safeUnregisterComponent(sensor);
    }

    virtual void onConnect(MQTTClient *client) override;

    // using MQTT update rate
    virtual void publishState(MQTTClient *client) = 0;
    // using update rate
    virtual void getValues(JsonArray &json, bool timer) = 0;

    virtual void createWebUI(WebUIRoot &webUI, WebUIRow **row) = 0;
    virtual void getStatus(Print &output) = 0;

    virtual SensorType getType() const {
        return SensorType::UNKNOWN;
    }

    virtual bool getSensorData(String &name, StringVector &values) {
        return false;
    }

    virtual bool hasForm() const {
        return false;
    }

    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) {
    }

    virtual void configurationSaved(FormUI::Form::BaseForm *form) {
    }

    virtual void reconfigure(PGM_P source) {
    }

    virtual void shutdown() {
    }

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() {
    }
    virtual bool atModeHandler(AtModeArgs &args) {
        return false;
    }
#endif

    void timerEvent(JsonArray *array, MQTTClient *client);

    void setUpdateRate(uint16_t updateRate) {
        _updateRate = updateRate;
        _nextUpdate = 0;
    }

    void setMqttUpdateRate(uint16_t updateRate) {
        _mqttUpdateRate = updateRate;
        _nextMqttUpdate = 0;
    }

    void setNextMqttUpdate(uint16_t delay) {
        _nextMqttUpdate = time(nullptr) + delay;
    }

private:
    uint16_t _updateRate;
    uint16_t _mqttUpdateRate;
    time_t _nextUpdate;
    time_t _nextMqttUpdate;
};
