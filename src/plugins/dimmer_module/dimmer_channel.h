/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_DIMMER_MODULE

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

    MQTTAutoDiscovery *createAutoDiscovery(MQTTAutoDiscovery::Format_t format) override;
    void onConnect(MQTTClient *client) override;
    void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

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

private:
    void _createTopics();

    Driver_DimmerModule_MQTTComponentData_t _data;
    uint16_t _storedBrightness;
    Driver_DimmerModule *_dimmer;
    uint8_t _channel;
};

#endif
