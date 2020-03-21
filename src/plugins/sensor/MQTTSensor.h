/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR

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
    INA219
);

#if DEBUG_IOT_SENSOR
#define REGISTER_SENSOR_CLIENT(sensor)                  { debug_printf_P(PSTR("registerClient %s\n"), __CLASS__); registerClient(sensor); }
#else
#define REGISTER_SENSOR_CLIENT(sensor)                  registerClient(sensor);
#endif

class MQTTSensor : public MQTTComponent {
public:
    const uint8_t DEFAULT_UPDATE_RATE = 60;

    using SensorType = MQTTSensorSensorType;

    MQTTSensor();
    virtual ~MQTTSensor();

    template <class T>
    void registerClient(T client) {
        auto mqttClient = MQTTClient::getClient();
        if (mqttClient) {
            mqttClient->registerComponent(client);
        }
    }

    //virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    //virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override {
#if DEBUG_IOT_SENSOR
        debug_println();
#endif
    }

    virtual void publishState(MQTTClient *client) = 0;
    virtual void getValues(JsonArray &json) = 0;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) = 0;
    virtual void getStatus(PrintHtmlEntitiesString &output) = 0;

    virtual SensorType getType() const {
        return SensorType::UNKNOWN;
    }

    virtual bool hasForm() const {
        return false;
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) {
    }

    virtual void reconfigure() {
    }
    virtual void restart() {
    }

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() {
    }
    virtual bool atModeHandler(AtModeArgs &args) {
        return false;
    }
#endif

    void timerEvent(JsonArray &array);

    inline void setUpdateRate(uint8_t updateRate) {
        _updateRate = updateRate;
        _nextUpdate = 0;
    }

protected:
    uint8_t _qos;

private:
    uint8_t _updateRate;
    time_t _nextUpdate;
};

#endif
