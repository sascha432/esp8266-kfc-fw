/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include "mqtt_client.h"
#include "plugins.h"

namespace MQTT {

    class Plugin : public PluginComponent {
    public:
        Plugin();

        virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
        virtual void reconfigure(const String &source) override;
        virtual void shutdown() override;
        virtual void getStatus(Print &output) override;
        virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;
        #if MQTT_AUTO_DISCOVERY
            virtual void createMenu() override;
        #endif

        #if AT_MODE_SUPPORTED
            #if AT_MODE_HELP_SUPPORTED
                virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
            #endif
            virtual bool atModeHandler(AtModeArgs &args) override;
        #endif

        static Plugin &getPlugin();
    };

}
