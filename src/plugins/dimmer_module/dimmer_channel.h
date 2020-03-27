/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "kfc_fw_config.h"
#include "../mqtt/mqtt_client.h"

typedef struct {
    struct {
        String set;
        String state;
        bool value;
    } state;
    struct {
        String set;
        String state;
        int16_t value;
    } brightness;
} Driver_DimmerModule_MQTTComponentData_t;

class Driver_DimmerModule;

class DimmerChannel : public MQTTComponent
{
public:
    DimmerChannel();

    void setup(Driver_DimmerModule *dimmer, uint8_t channel);

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    bool on();
    bool off();
    void publishState(MQTTClient *client = nullptr);

    inline bool getOnState() const {
        return _data.state.value;
    }
    inline int16_t getLevel() const {
        return _data.brightness.value;
    }
    void setLevel(int16_t level) {
        _data.brightness.value = level;
        _data.state.value = (level != 0);
    }

    inline void setStoredBrightness(uint16_t storedBrightness) {
        _storedBrightness = storedBrightness;
    }

private:
    void _createTopics();

    Driver_DimmerModule_MQTTComponentData_t _data;
    uint16_t _storedBrightness;
    Driver_DimmerModule *_dimmer;
    uint8_t _channel;
};
