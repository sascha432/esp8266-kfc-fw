/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <PluginComponent.h>
#include "dimmer_def.h"
#include "../src/plugins/mqtt/mqtt_client.h"
#include "../src/plugins/mqtt/mqtt_json.h"

#if IOT_DIMMER_X9C_POTI
#    include <x9c_xxx.h>
#endif

namespace Dimmer {

    class Plugin : public PluginComponent, public MQTTComponent {

    // PluginComponent
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

    // MQTTComponent
    public:
        virtual AutoDiscovery::EntityPtr getAutoDiscovery(FormatType format, uint8_t num) override;
        virtual uint8_t getAutoDiscoveryCount() const override {
            return 1;
        }
        virtual void onConnect() override;
        virtual void onMessage(const char *topic, const char *payload, size_t len);

    protected:
        void onJsonMessage(const MQTT::Json::Reader &json);
        void publishState();
        void on();
        void off();
        void setLevel(uint32_t level);
        void _publish();

    private:
        uint32_t _storedBrightness;
        uint32_t _brightness;
        Event::Timer _publishTimer;
        #if IOT_DIMMER_X9C_POTI
            X9C::Poti _poti;
        #endif
    };

    inline void Plugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
    {
    }

    inline void Plugin::getValues(WebUINS::Events &array)
    {
    }

    inline void Plugin::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
    {
    }

    inline void Plugin::reconfigure(const String &source)
    {
    }

    extern Plugin dimmer_plugin;

}
