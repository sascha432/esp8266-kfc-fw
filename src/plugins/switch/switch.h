/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_IOT_SWITCH
#define DEBUG_IOT_SWITCH                            0
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

class SwitchPlugin : public PluginComponent, public MQTTComponent {
public:
    SwitchPlugin();

// PluginComponent
public:
    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

// WebUI
public:
    virtual void createWebUI(WebUIRoot &webUI) override;
    virtual void getValues(JsonArray &array) override;
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

// MQTTComponent
public:
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTT::FormatType format, uint8_t num) override;
    virtual uint8_t getAutoDiscoveryCount() const override;
    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

private:
    void _publishState(MQTTClient *client, int8_t channel = -1);

#if IOT_SWITCH_PUBLISH_MQTT_INTERVAL
    Event::Timer _updateTimer;
#endif
#if IOT_SWITCH_STORE_STATES
    Event::Timer _delayedWrite;
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
