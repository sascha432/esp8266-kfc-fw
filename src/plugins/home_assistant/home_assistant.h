/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if HOME_ASSISTANT_INTEGRATION == 0
#error HOME_ASSISTANT_INTEGRATION not set
#endif

#include <Arduino_compat.h>
#include <vector>
#include "../src/plugins/mqtt/mqtt_client.h"
#include "KFCRestApi.h"
#include "HassJsonReader.h"
#include "plugins.h"

#ifndef DEBUG_HOME_ASSISTANT
#define DEBUG_HOME_ASSISTANT                            0
#endif

class HassPlugin : public PluginComponent, public KFCRestAPI, public MQTTComponent {
// PluginComponent
public:
    using ActionEnum_t = KFCConfigurationClasses::Plugins::HomeAssistant::ActionEnum_t;
    using Action = KFCConfigurationClasses::Plugins::HomeAssistant::Action;

    typedef std::function<void(bool status)> StatusCallback_t;
    typedef std::function<void(HassJsonReader::GetState *state, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback)> GetStateCallback_t;
    typedef std::function<void(HassJsonReader::CallService *service, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback)> ServiceCallback_t;

    HassPlugin() : MQTTComponent(MQTTComponent::ComponentType::BINARY_SENSOR) {
        REGISTER_PLUGIN(this, "HassPlugin");
    }

    static HassPlugin &getInstance();

    virtual PGM_P getName() const {
        return PSTR("hass");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Home Assistant");
    }
    virtual PriorityType getSetupPriority() const {
        return PriorityType::HASS;
    }
    virtual OptionsType getOptions() const override {
        return EnumHelper::Bitset::all(OptionsType::SETUP_AFTER_DEEP_SLEEP, OptionsType::HAS_STATUS, OptionsType::HAS_CONFIG_FORM, OptionsType::HAS_AT_MODE);
    }

    virtual void getStatus(Print &output) override;

    virtual bool canHandleForm(const String &formName) const {
        return String_equals(formName, getName()) || String_equals(formName, PSTR("hass_edit")) || String_equals(formName, PSTR("hass_actions"));
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, FormUI::Form::BaseForm &form) override;

    virtual void setup(SetupModeType mode);
    virtual void reconfigure(PGM_P source);
    virtual bool hasReconfigureDependecy(PluginComponent *plugin) const {
        return plugin->nameEquals(FSPGM(http));
    }

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

// MQTTComponent
public:
    virtual MQTTAutoDiscoveryPtr nextAutoDiscovery(MQTT::FormatType format, uint8_t num) override {
        return nullptr;
    }
    virtual uint8_t getAutoDiscoveryCount() const override {
        return 0;
    }

    virtual void onConnect(MQTTClient *client) override;
    virtual void onMessage(MQTTClient *client, char *topic, char *payload, size_t len) override;

private:
    bool _mqttSplitTopics(String &state, String &set);
    void _mqttSet(const String &topic, int value);
    void _mqttGet(const String &topic, std::function<void(bool status, int)> callback);

private:
    typedef struct {
        String topic;
        int value;
    } TopicValue_t;

    typedef std::vector<TopicValue_t> TopicValueVector;

    TopicValueVector _topics;

// // WebUI
// public:
//     virtual void createWebUI(WebUIRoot &webUI) override;
//     virtual void getValues(JsonArray &array);
//     virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

// KFCRestApi
public:
    virtual void getRestUrl(String &url) const;
    virtual void getBearerToken(String &token) const;

private:
    uint8_t _apiId;

public:
    void executeAction(const Action &action, StatusCallback_t statusCallback);

    void getState(const String &entityId, uint8_t apiId, GetStateCallback_t callback, StatusCallback_t statusCallback);
    void callService(const String &service, uint8_t apiId, const JsonUnnamedObject &payload, ServiceCallback_t callback, StatusCallback_t statusCallback);

    static void _serviceCallback(HassJsonReader::CallService *service, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback);

private:
    String _getDomain(const String &entityId);

// web server
public:
    static void removeAction(AsyncWebServerRequest *request);

private:
    void _installWebhooks();
};
