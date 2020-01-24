/**
  Author: sascha_lammers@gmx.de
*/

#if HOME_ASSISTANT_INTEGRATION

#pragma once

#include <Arduino_compat.h>
#include "KFCRestApi.h"
#include "progmem_data.h"
#include "plugins.h"

#ifndef DEBUG_HOME_ASSISTANT
#define DEBUG_HOME_ASSISTANT                            1
#endif

class HassPlugin : public PluginComponent, public WebUIInterface, public KFCRestAPI {
// PluginComponent
public:
    HassPlugin() {
        REGISTER_PLUGIN(this, "HassPlugin");
    }

    virtual PGM_P getName() const {
        return PSTR("hass");
    }

    virtual bool hasStatus() const override {
        return true;
    }
    virtual void getStatus(Print &output) override;

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

// KFCRestApi
public:
    virtual void getRestUrl(String &url) const;
    virtual void getBearerToken(String &token) const;
};

#endif
