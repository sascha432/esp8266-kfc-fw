/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <PluginComponent.h>
#include "dimmer_module.h"

namespace Dimmer {

    class Plugin : public PluginComponent, public Module {

    public:
        Plugin();

        virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
        virtual void reconfigure(const String &source) override;
        virtual void shutdown() override;
        virtual void getStatus(Print &output) override;
        virtual void createWebUI(WebUINS::Root &webUI) override;
        virtual void createMenu() override;

        virtual void readConfig(ConfigType &cfg) {
            _readConfig(cfg);
        }

        virtual void writeConfig(ConfigType &cfg) {
            _writeConfig(cfg);
        }

        virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override {
            Form::_createConfigureForm(type, formName, form);
        }

        virtual void getValues(WebUINS::Events &array) override {
            _getValues(array);
        }
        virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override {
            _setValue(id, value, hasValue, state, hasState);
        }

        ChannelsArray &getChannels() {
            return _channels;
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

    extern Plugin dimmer_plugin;

}

