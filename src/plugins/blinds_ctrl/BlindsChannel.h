/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_BLINDS_CTRL

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

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
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

    static PGM_P _stateStr(StateEnum_t state);

private:
    StateEnum_t _state;
    Channel_t _channel;
    String _setTopic;
    String _stateTopic;
    uint8_t _number;
    BlindsControl *_controller;
};

#endif
