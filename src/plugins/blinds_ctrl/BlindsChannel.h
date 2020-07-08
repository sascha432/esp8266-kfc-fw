/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "kfc_fw_config.h"
#include "../mqtt/mqtt_client.h"

class BlindsControl;

class BlindsChannel : public MQTTComponent {
public:
    typedef enum {
        UNKNOWN = 0,
        OPEN = 1,
        CLOSED = 2,
        STOPPED = 3,
    } StateEnum_t;

    typedef struct BlindsControllerChannel Channel_t;

    BlindsChannel();

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override {
        return 1;
    }
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    void _publishState(MQTTClient *client, uint8_t qos);

    void setState(StateEnum_t state);
    StateEnum_t getState() const;
    bool isOpen() const;
    bool isClosed() const;
    void setChannel(const Channel_t &channel);
    Channel_t &getChannel();
    void setNumber(uint8_t number);
    void setController(BlindsControl *controller);

    static const __FlashStringHelper *_stateStr(StateEnum_t state);

private:
    StateEnum_t _state;
    Channel_t _channel;
    uint8_t _number;
    BlindsControl *_controller;
};
