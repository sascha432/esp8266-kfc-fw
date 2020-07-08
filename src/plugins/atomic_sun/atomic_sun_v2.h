/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <HardwareSerial.h>
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

    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTTAutoDiscovery::Format_t format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;
    virtual void getValues(JsonArray &array) override;

    virtual bool on(uint8_t channel = -1) override;
    virtual bool off(uint8_t channel = -1) override;
    virtual int16_t getChannel(uint8_t channel) const override;
    virtual bool getChannelState(uint8_t channel) const override;
    virtual void setChannel(uint8_t channel, int16_t level, float time) override;
    virtual uint8_t getChannelCount() const override {
        return _channels.size();
    }

   void publishState(MQTTClient *client = nullptr);

    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override {
        readConfig();
        DimmerModuleForm::createConfigureForm(request, form);
    }

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
    uint8_t _qos;

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

class AtomicSunPlugin : public Driver_4ChDimmer {
public:
    AtomicSunPlugin() {
        REGISTER_PLUGIN(this);
    }
    virtual PGM_P getName() const {
        return PSTR("atomicsun");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Atomic Sun v2");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (AtomicSunPlugin::PluginPriorityEnum_t)100;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void shutdown() override;
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const;

    virtual bool hasWebUI() const override {
        return true;
    }
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override {
        return this;
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

#if AT_MODE_SUPPORTED
    virtual bool hasAtMode() const override;
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

extern AtomicSunPlugin dimmer_plugin;
