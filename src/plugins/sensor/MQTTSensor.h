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
    MQTTSensor();
    virtual ~MQTTSensor();

    // virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    virtual void publishState(MQTTClient *client) = 0;
    virtual void getValues(JsonArray &json) = 0;
    virtual void createWebUI(WebUI &webUI, WebUIRow **row) = 0;
    //virtual void getStatus(PrintHtmlEntitiesString &output) override;
    virtual void getStatus(PrintHtmlEntitiesString &output) = 0;

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
