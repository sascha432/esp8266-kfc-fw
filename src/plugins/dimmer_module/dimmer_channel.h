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
    static constexpr int16_t MAX_LEVEL = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
    static constexpr int16_t MIN_LEVEL = MAX_LEVEL / 100;
    static constexpr int16_t DEFAULT_LEVEL = MAX_LEVEL / 2;
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

    bool getOnState() const;
    int16_t getLevel() const;
    void setLevel(int16_t level);
    void setStoredBrightness(uint16_t store);

private:
    void _createTopics();

    Driver_DimmerModule_MQTTComponentData_t _data;
    uint16_t _storedBrightness;
    Driver_DimmerModule *_dimmer;
    uint8_t _channel;
};
