/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <kfc_fw_config.h>
#include <EventScheduler.h>
#include "../src/plugins/mqtt/mqtt_json.h"
#include "../src/plugins/mqtt/mqtt_client.h"

using KFCConfigurationClasses::Plugins;

class Driver_DimmerModule;

class DimmerChannel : public MQTTComponent
{
public:
    static constexpr int16_t MAX_LEVEL = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
    static constexpr int16_t MIN_LEVEL = MAX_LEVEL / 100;
    static constexpr int16_t DEFAULT_LEVEL = MAX_LEVEL / 2;
   static constexpr uint16_t kWebUIMaxUpdateRate = 150;
    static constexpr uint16_t kMQTTMaxUpdateRate = 600;
    static constexpr uint8_t kMQTTUpdateRateMultiplier = kMQTTMaxUpdateRate / kWebUIMaxUpdateRate;

    static constexpr uint8_t kWebUIUpdateFlag = 0x01;
    static constexpr uint8_t kMQTTUpdateFlag = 0x02;
    static constexpr uint8_t kUpdateAllFlag = kWebUIUpdateFlag|kMQTTUpdateFlag;
    static constexpr uint8_t kStartTimerFlag = 0xff;

public:
    using ConfigType = Plugins::DimmerConfig::DimmerConfig_t;

    enum class TopicType : uint8_t {
        COMMAND_SET,
        COMMAND_STATE,
    };

public:
    DimmerChannel(Driver_DimmerModule *dimmer = nullptr, uint8_t channel = 0);

    void setup(Driver_DimmerModule *dimmer, uint8_t channel)
    {
        _dimmer = dimmer;
        _channel = channel;
        _topic = _createTopics(TopicType::COMMAND_SET, false);
    }

#if IOT_DIMMER_MODULE_HAS_BUTTONS
    DimmerChannel(const DimmerChannel &) = delete;
    DimmerChannel &operator=(const DimmerChannel &) = delete;

    ~DimmerChannel() {
        if (_delayTimer) {
            delete _delayTimer;
        }
    }
#endif

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTT::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) final;

    bool on(float transition = NAN);
    bool off(ConfigType *config = nullptr, float transition = NAN, int32_t level = -1);
    void publishState();

    bool getOnState() const;
    int16_t getLevel() const;
    void setLevel(int32_t level, float transition = NAN);
    void setStoredBrightness(int32_t store);
    uint16_t getStorededBrightness() const;

protected:
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    int _offDelayPrecheck(int16_t level, ConfigType *config = nullptr, int16_t storeLevel = -1);
#endif

    void onMessage(MQTTClient *client, const MQTT::JsonReader &json);
    virtual void _publishMQTT();
    virtual void _publishWebUI();

private:
    void _publish();
    bool _set(int32_t level, float transition = NAN);
    String _createTopics(TopicType type, bool full = true) const;

    Driver_DimmerModule *_dimmer;
    String _topic;
    Event::Timer _publishTimer;
    uint32_t _publishLastTime;
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    Event::Timer *_delayTimer;
#endif
    uint16_t _storedBrightness;
    uint16_t _brightness;
    uint8_t _channel;
    uint8_t _publishFlag;
    uint8_t _mqttCounter;
};

inline bool DimmerChannel::getOnState() const
{
    return _brightness != 0;
}

inline int16_t DimmerChannel::getLevel() const
{
    return _brightness;
}

inline void DimmerChannel::setStoredBrightness(int32_t store)
{
    if (store > 1) {
        _storedBrightness = store;
    }
}

inline uint16_t DimmerChannel::getStorededBrightness() const
{
    return _storedBrightness;
}
