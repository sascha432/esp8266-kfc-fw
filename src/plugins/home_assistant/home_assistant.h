/**
  Author: sascha_lammers@gmx.de
*/

#if HOME_ASSISTANT_INTEGRATION

#pragma once

#include <Arduino_compat.h>
#include "KFCRestApi.h"
#include "progmem_data.h"
#include "HassJsonReader.h"
#include "plugins.h"

#ifndef DEBUG_HOME_ASSISTANT
#define DEBUG_HOME_ASSISTANT                            0
#endif

class HassPlugin : public PluginComponent, /*public WebUIInterface, */public KFCRestAPI {
// PluginComponent
public:
    typedef KFCConfigurationClasses::Plugins::HomeAssistant::ActionEnum_t ActionEnum_t;
    typedef KFCConfigurationClasses::Plugins::HomeAssistant::Action Action;
    typedef std::function<void(bool status)> StatusCallback_t;
    typedef std::function<void(HassJsonReader::GetState *state, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback)> GetStateCallback_t;
    typedef std::function<void(HassJsonReader::CallService *service, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback)> ServiceCallback_t;

    HassPlugin() {
        REGISTER_PLUGIN(this);
    }

    static HassPlugin &getInstance();

    virtual PGM_P getName() const {
        return PSTR("hass");
    }
    virtual const __FlashStringHelper *getFriendlyName() const {
        return F("Home Assistant");
    }
    virtual PluginPriorityEnum_t getSetupPriority() const {
        return PRIO_HASS;
    }
    virtual bool autoSetupAfterDeepSleep() const override {
        return true;
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

    // virtual MenuTypeEnum_t getMenuType() const override {
    //     return CUSTOM;
    // }
    // virtual void createMenu() override {
    //     bootstrapMenu.addSubMenu(F("Home Assistant"), F("hass.html"), navMenu.config);
    // }

    virtual PGM_P getConfigureForm() const override {
        return getName();
    }
    virtual void createConfigureForm(AsyncWebServerRequest *request, Form &form) override;

#if AT_MODE_SUPPORTED
    // at mode command handler
    virtual bool hasAtMode() const override {
        return true;
    }
    virtual void atModeHelpGenerator() override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif

// // WebUIInterface
// public:
//     virtual bool hasWebUI() const override {
//         return true;
//     }
//     virtual void createWebUI(WebUI &webUI) override;
//     virtual WebUIInterface *getWebUIInterface() override {
//         return this;
//     }

//     virtual void getValues(JsonArray &array);
//     virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState);

// KFCRestApi
public:
    virtual void getRestUrl(String &url) const;
    virtual void getBearerToken(String &token) const;

public:
    void executeAction(const Action &action, StatusCallback_t statusCallback);

    void getState(const String &entityId, GetStateCallback_t callback, StatusCallback_t statusCallback);
    void callService(const String &service, const JsonUnnamedObject &payload, ServiceCallback_t callback, StatusCallback_t statusCallback);

    static void _serviceCallback(HassJsonReader::CallService *service, KFCRestAPI::HttpRequest &request, StatusCallback_t statusCallback);
private:
    String _getDomain(const String &entityId);

};

#endif
