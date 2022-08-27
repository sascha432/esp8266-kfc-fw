/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <PluginComponent.h>
#include "dimmer_module.h"

namespace Dimmer {

    class ChannelsArray;

    class Plugin : public PluginComponent, public Module {

    public:
        Plugin();

        virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
        virtual void reconfigure(const String &source) override;
        virtual void shutdown() override;
        virtual void getStatus(Print &output) override;
        virtual void createWebUI(WebUINS::Root &webUI) override;
        virtual void createMenu() override;

        virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

        virtual void getValues(WebUINS::Events &array) override;
        virtual void setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) override;

        ChannelsArray &getChannels();

        #if AT_MODE_SUPPORTED
            #if AT_MODE_HELP_SUPPORTED
                virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
            #endif
            virtual bool atModeHandler(AtModeArgs &args) override;
        #endif
    };

    inline void Plugin::shutdown()
    {
        Module::shutdown();
    }

    inline void Plugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
    {
        Base::createConfigureForm(type, formName, form, request);
    }

    inline void Plugin::getValues(WebUINS::Events &array)
    {
        Base::getValues(array);
    }

    inline void Plugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
    {
        Base::setValue(id, value, hasValue, state, hasState);
    }

    inline void Plugin::reconfigure(const String &source)
    {
        Base::readConfig(_config);
        Module::shutdown();
        Module::setup();
    }

    #if AT_MODE_SUPPORTED
        #if AT_MODE_HELP_SUPPORTED

            inline ATModeCommandHelpArrayPtr Plugin::atModeCommandHelp(size_t &size) const
            {
                return Base::atModeCommandHelp(size);
            }

        #endif

        inline bool Plugin::atModeHandler(AtModeArgs &args)
        {
            return Base::atModeHandler(args, *this, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        }

    #endif

    extern Plugin dimmer_plugin;

    #if IOT_DIMMER_MODULE_INTERFACE_UART

        inline void Base::onData(Stream &client)
        {
            #define DEBUG_INCOMING_DATA 0
            #if DEBUG_INCOMING_DATA
                PrintString str1;
                String str2;
            #endif
            while(client.available()) {
                #if DEBUG_INCOMING_DATA
                    auto data = static_cast<uint8_t>(client.read());
                    str1.printf_P(PSTR("%02x"), data);
                    str2 += isprint(data) ? static_cast<char>(data) : '.';
                    dimmer_plugin._wire.feed(data);
                #else
                    dimmer_plugin._wire.feed(client.read());
                #endif
            }
            #if DEBUG_INCOMING_DATA
                __DBG_printf("feed %s [%s]", str1.c_str(), str2.c_str());
            #endif
            #undef DEBUG_INCOMING_DATA
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
