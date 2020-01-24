/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_ATOMIC_SUN_V2

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
#define DEBUG_4CH_DIMMER 0
#endif

#ifndef IOT_ATOMIC_SUN_BAUD_RATE
#define IOT_ATOMIC_SUN_BAUD_RATE            57600
#endif

#ifndef IOT_ATOMIC_SUN_MAX_BRIGHTNESS
#define IOT_ATOMIC_SUN_MAX_BRIGHTNESS       8333
#endif

#if !defined(IOT_SENSOR_HAVE_HLW8012) || IOT_SENSOR_HAVE_HLW8012 == 0
// calculate power if sensor is not available
#define IOT_ATOMIC_SUN_CALC_POWER           1
#else
#define IOT_ATOMIC_SUN_CALC_POWER           0
#endif

#ifndef STK500V1_RESET_PIN
#error STK500V1_RESET_PIN not defined
#endif

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

    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
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
    virtual uint8_t getChannelCount() const override;

   void publishState(MQTTClient *client = nullptr);

    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override {
        Dimmer_Base::readConfig();
        DimmerModuleForm::createConfigureForm(request, form);
    }
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

#if IOT_ATOMIC_SUN_CALC_POWER
    float _calcPower(uint8_t channel);
    float _calcTotalPower();
#endif

private:
#if DEBUG_4CH_DIMMER
    uint8_t endTransmission();
#else
    inline uint8_t endTransmission();
#endif

    Driver_4ChDimmer_MQTTComponentData_t _data;
    int16_t _storedChannels[4];
    int16_t _channels[4];
    float _ratio[2];
    uint8_t _qos;
    EventScheduler::Timer _publishTimer;

public:
    // channels are displayed in this order in the web ui
    // warm white
    static const int8_t CHANNEL_WW1 = 1;
    static const int8_t CHANNEL_WW2 = 3;
    // cold white
    static const int8_t CHANNEL_CW1 = 2;
    static const int8_t CHANNEL_CW2 = 0;

    static const uint16_t COLOR_MIN = 15300;
    static const uint16_t COLOR_MAX = 50000;
    static const uint16_t COLOR_RANGE = (COLOR_MAX - COLOR_MIN);
};

class AtomicSunPlugin : public Driver_4ChDimmer {
public:
    AtomicSunPlugin() {
        REGISTER_PLUGIN(this, "AtomicSunPlugin");
    }
    virtual PGM_P getName() const {
        return PSTR("atomicsun");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return (AtomicSunPlugin::PluginPriorityEnum_t)100;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void reconfigure(PGM_P source) override;
    virtual void restart() override;
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

    virtual MenuTypeEnum_t getMenuType() const override {
        return CUSTOM;
    }
    virtual void createMenu() override {
        bootstrapMenu.addSubMenu(F("Atomic Sun v2"), F("dimmer_cfg.html"), navMenu.config);
    }

};

extern AtomicSunPlugin dimmer_plugin;

#endif
