/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <PrintHtmlEntitiesString.h>
#include <WebUIComponent.h>
#include "../mqtt/mqtt_client.h"
#include "serial_handler.h"
#include "../dimmer_module/dimmer_base.h"
#include "../dimmer_module/dimmer_module_form.h"

#ifndef DEBUG_4CH_DIMMER
#define DEBUG_4CH_DIMMER                    0
#endif

#ifndef IOT_ATOMIC_SUN_BAUD_RATE
#define IOT_ATOMIC_SUN_BAUD_RATE            57600
#endif

#ifndef IOT_ATOMIC_SUN_MAX_BRIGHTNESS
#define IOT_ATOMIC_SUN_MAX_BRIGHTNESS       8333
#endif

//  channel order
#ifndef IOT_ATOMIC_SUN_CHANNEL_WW1
#define IOT_ATOMIC_SUN_CHANNEL_WW1          0
#define IOT_ATOMIC_SUN_CHANNEL_WW2          1
#define IOT_ATOMIC_SUN_CHANNEL_CW1          2
#define IOT_ATOMIC_SUN_CHANNEL_CW2          3
#endif

#ifndef STK500V1_RESET_PIN
#error STK500V1_RESET_PIN not defined
#endif

#if !defined(IOT_ATOMIC_SUN_V2) || !IOT_ATOMIC_SUN_V2
#error requires IOT_ATOMIC_SUN_V2=1
#endif

class ChannelsArray : public std::array<int16_t, 4>
{
public:
    void setAll(int16_t value) {
        auto ptr = data();
        for (uint8_t i = 0; i < size(); i++) {
            *ptr++ = value;
        }
    }

    int32_t getSum() const {
        int32_t sum = 0;
        auto ptr = data();
        for (uint8_t i = 0; i < size(); i++) {
            sum += *ptr++;
        }
        return sum;
    }

    int32_t getSum(int16_t minLevel) const {
        int32_t sum = 0;
        auto ptr = data();
        for (uint8_t i = 0; i < size(); i++) {
            if (*ptr > minLevel) {
                sum += *ptr;
            }
            ptr++;
        }
        return sum;
    }
};

typedef struct {
    struct {
        String set;
        String state;
        bool value;
    } state;
    struct {
        String set;
        String state;
        int32_t value;
    } brightness;
    struct {
        String set;
        String state;
        float value;
    } color;
    struct {
        String set;
        String state;
        String brightnessSet;
        String brightnessState;
    } channels[4];
    struct {
        String set;
        String state;
        bool value;
    } lockChannels;
} Driver_4ChDimmer_MQTTComponentData_t;

class Driver_4ChDimmer : public MQTTComponent, public Dimmer_Base, public DimmerModuleForm
{
public:
    Driver_4ChDimmer();

    virtual void readConfig() override;

// MQTT
public:
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

// WebUI
public:
    void _setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);
    void _getValues(JsonArray &array);

// dimmer
public:
    virtual bool on(uint8_t channel = -1) override;
    virtual bool off(uint8_t channel = -1) override;
    virtual int16_t getChannel(uint8_t channel) const override;
    virtual bool getChannelState(uint8_t channel) const override;
    virtual void setChannel(uint8_t channel, int16_t level, float time) override;
    virtual uint8_t getChannelCount() const override {
        return _channels.size();
    }

   void publishState(MQTTClient *client = nullptr);

    virtual void _onReceive(size_t length) override;

protected:
    void _begin();
    void _end();
    void _printStatus(Print &out);

private:
    void _createTopics();
    void _publishState(MQTTClient *client);

    void _setChannels(float fadetime);
    void _getChannels();

    void _channelsToBrightness();
    void _brightnessToChannels();
    void _setLockChannels(bool value);
    void _calcRatios();

protected:
    // using ChannelsArray = std::array<int16_t, 4>;

    Driver_4ChDimmer_MQTTComponentData_t _data;
    ChannelsArray _storedChannels;
    ChannelsArray _channels;
    float _ratio[2];

public:
    // channels are displayed in this order in the web ui
    // warm white
    int8_t channel_ww1;
    int8_t channel_ww2;
    // cold white
    int8_t channel_cw1;
    int8_t channel_cw2;

    static constexpr uint16_t COLOR_MIN = 15300;
    static constexpr uint16_t COLOR_MAX = 50000;
    static constexpr uint16_t COLOR_RANGE = (COLOR_MAX - COLOR_MIN);

    static constexpr uint8_t MAX_CHANNELS = 4;
    static constexpr int16_t MAX_LEVEL = IOT_ATOMIC_SUN_MAX_BRIGHTNESS;
    static constexpr int16_t MIN_LEVEL = (MAX_LEVEL / 100);    // below is treat as off
    static constexpr int16_t DEFAULT_LEVEL = MAX_LEVEL / 3;     // set this level if a channel is turned on without a brightness value
    static constexpr int32_t MAX_LEVEL_ALL_CHANNELS = MAX_LEVEL * MAX_CHANNELS;
};

class AtomicSunPlugin : public PluginComponent, public Driver_4ChDimmer {
public:
    AtomicSunPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request);

// WebUI
public:
    virtual void createWebUI(WebUI &webUI) override;
    virtual void getValues(JsonArray &array) override {
        Driver_4ChDimmer::_getValues(array);
    }
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override {
        Driver_4ChDimmer::_setValue(id, value, hasValue, state, hasState);
    }

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override {
        _atModeHelpGenerator(getName_P());
    }
    virtual bool atModeHandler(AtModeArgs &args) override {
        return _atModeHandler(args, *this, MAX_LEVEL);
    }
#endif
};

extern AtomicSunPlugin dimmer_plugin;
