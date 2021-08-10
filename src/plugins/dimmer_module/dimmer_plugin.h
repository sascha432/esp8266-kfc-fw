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

        virtual void readConfig(ConfigType &cfg);
        virtual void writeConfig(ConfigType &cfg);
        virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

        virtual void getValues(WebUINS::Events &array) override;
        virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

        ChannelsArray &getChannels();

        #if AT_MODE_SUPPORTED
            virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
            virtual bool atModeHandler(AtModeArgs &args) override;
        #endif
    };

    inline void Plugin::shutdown()
    {
        Module::shutdown();
    }

    inline void Plugin::readConfig(ConfigType &cfg)
    {
        Base::readConfig(cfg);
    }

    inline void Plugin::writeConfig(ConfigType &cfg)
    {
        Base::writeConfig(cfg);
    }

    inline void Plugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
    {
        Form::createConfigureForm(type, formName, form);
    }

    inline void Plugin::getValues(WebUINS::Events &array)
    {
        Base::getValues(array);
    }

    inline void Plugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
    {
        Base::setValue(id, value, hasValue, state, hasState);
    }

    inline Plugin::ChannelsArray &Plugin::getChannels()
    {
        return _channels;
    }

    inline void Plugin::reconfigure(const String &source)
    {
        Base::readConfig(_config);
        Module::shutdown();
        Module::setup();
    }

    #if AT_MODE_SUPPORTED
        inline ATModeCommandHelpArrayPtr Plugin::atModeCommandHelp(size_t &size) const
        {
            return Base::atModeCommandHelp(size);
        }

        inline bool Plugin::atModeHandler(AtModeArgs &args)
        {
            return Base::atModeHandler(args, *this, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        }
    #endif

    extern Plugin dimmer_plugin;

    #if IOT_DIMMER_MODULE_INTERFACE_UART

        inline void Base::onData(Stream &client)
        {
            while(client.available()) {
                dimmer_plugin._wire.feed(client.read());
            }
        }

        inline void Base::onReceive(int length)
        {
            dimmer_plugin._onReceive(length);
        }

    #else

        inline void Base::fetchMetrics(Event::CallbackTimerPtr timer)
        {
            timer->updateInterval(Event::milliseconds(kMetricsDefaultUpdateRate));
            // using dimmer_plugin avoids adding extra static variable to Base
            dimmer_plugin._fetchMetrics();
        }

        inline void Base::_fetchMetrics()
        {
            auto metrics = _wire.getMetrics();
            if (metrics) {
                _updateMetrics(static_cast<dimmer_metrics_t>(metrics));
            }
        }

    #endif

}
