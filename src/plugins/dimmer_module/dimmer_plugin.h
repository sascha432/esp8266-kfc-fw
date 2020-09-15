/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <PluginComponent.h>
#include "dimmer_module.h"

class DimmerModulePlugin : public PluginComponent, public Driver_DimmerModule {
public:
    DimmerModulePlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    virtual void createWebUI(WebUIRoot &webUI) override;

    virtual void readConfig(DimmerModuleForm::ConfigType &cfg) {
        _readConfig(cfg);
    }

    virtual void writeConfig(DimmerModuleForm::ConfigType &cfg) {
        _writeConfig(cfg);
    }

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override {
        DimmerModuleForm::_createConfigureForm(type, formName, form, request);
    }

    virtual void getValues(JsonArray &array) override {
        _getValues(array);
    }
    virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override {
        _setValue(id, value, hasValue, state, hasState);
    }

#if AT_MODE_SUPPORTED
    virtual void atModeHelpGenerator() override {
        _atModeHelpGenerator(getName_P());
    }
    virtual bool atModeHandler(AtModeArgs &args) override {
        return _atModeHandler(args, *this, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
    }
#endif
};

extern DimmerModulePlugin dimmer_plugin;
