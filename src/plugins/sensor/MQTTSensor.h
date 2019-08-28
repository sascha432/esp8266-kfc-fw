/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_SENSOR

#include <Arduino_compat.h>
#include <PrintHtmlEntitiesString.h>
#include <vector>
#include <KFCJson.h>
#include "../mqtt/mqtt_client.h"

class MQTTSensor : public MQTTComponent {
public:
    typedef enum {
        UNKNOWN = 0,
        LM75A,
        BME280,
        BME680,
        CCS811
    } SensorEnumType_t;

    const uint8_t DEFAULT_UPDATE_RATE = 60;

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
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    virtual void publishState(MQTTClient *client) = 0;
    virtual void getValues(JsonArray &json) = 0;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) = 0;
    //virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) = 0;
    virtual SensorEnumType_t getType() const;

    void timerEvent(JsonArray &array);

    inline void setUpdateRate(uint8_t updateRate) {
        _updateRate = updateRate;
    }

protected:
    uint8_t _qos;

private:
    uint8_t _updateRate;
    unsigned long _nextUpdate;
};

#endif
