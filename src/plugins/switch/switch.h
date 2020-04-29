/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_IOT_SWITCH
#define DEBUG_IOT_SWITCH                    1
#endif

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <WebUIComponent.h>
#include "../mqtt/mqtt_client.h"
#include "plugins.h"
#include "kfc_fw_config.h"

#ifndef IOT_SWITCH_ON_STATE
#define IOT_SWITCH_ON_STATE                         HIGH
#endif

// interval to publish the state of all channels, 0 to disable
#ifndef IOT_SWITCH_PUBLISH_MQTT_INTERVAL
#define IOT_SWITCH_PUBLISH_MQTT_INTERVAL            60000
#endif

// store states on file system
#ifndef IOT_SWITCH_STORE_STATES
#define IOT_SWITCH_STORE_STATES                     1
#endif

#ifndef IOT_SWITCH_STORE_STATES_WRITE_DELAY
#define IOT_SWITCH_STORE_STATES_WRITE_DELAY         30000
#endif

#ifndef IOT_SWITCH_CHANNEL_NUM
#error IOT_SWITCH_CHANNEL_NUM not defined
#endif

#ifndef IOT_SWITCH_CHANNEL_PINS
#error IOT_SWITCH_CHANNEL_PINS not defined
#endif

class SwitchPlugin : public PluginComponent, public MQTTComponent, public WebUIInterface {
public:
    SwitchPlugin();

// PluginComponent
public:
    virtual PGM_P getName() const override {
        return PSTR("switch");
    }
    virtual const __FlashStringHelper *getFriendlyName() const override {
        return F("Switch");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const override {
        return DEFAULT_PRIORITY;
    }

    virtual void setup(PluginSetupMode_t mode) override;
    virtual void restart() override;
    virtual void reconfigure(PGM_P source) override;

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    virtual PGM_P getConfigureForm() const override {
        return PSTR("switch");
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

// WebUIInterface
public:
    virtual bool hasWebUI() const override {
        return true;
    }
    virtual void createWebUI(WebUI &webUI) override;
    virtual WebUIInterface *getWebUIInterface() override {
        return this;
    }

    virtual void getValues(JsonArray &array);
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

// MQTTComponent
public:
    virtual void createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

private:
    void _publishState(MQTTClient *client, int8_t channel = -1);

#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    EventScheduler::Timer _updateTimer;
#endif
#if IOT_SWITCH_STORE_STATES
    EventScheduler::Timer _delayedWrite;
#endif

private:
    void _setChannel(uint8_t channel, bool state);
    bool _getChannel(uint8_t channel) const;
    String _getChannelName(uint8_t channel) const;
    void _readConfig();
    void _readStates();
    void _writeStates();

private:
    using SwitchConfig = KFCConfigurationClasses::Plugins::IOTSwitch::Switch_t;
    using SwitchStateEnum = KFCConfigurationClasses::Plugins::IOTSwitch::StateEnum_t;
    using WebUIEnum = KFCConfigurationClasses::Plugins::IOTSwitch::WebUIEnum_t;

    std::array<String, IOT_SWITCH_CHANNEL_NUM> _names;
    std::array<SwitchConfig, IOT_SWITCH_CHANNEL_NUM> _configs;
    uint32_t _states;
    const std::array<uint8_t, IOT_SWITCH_CHANNEL_NUM> _pins;
};
