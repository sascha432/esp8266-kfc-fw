/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <Form.h>
#include <PluginComponent.h>
#include <kfc_fw_config.h>
#include "../src/plugins/mqtt/component.h"

using Plugins = KFCConfigurationClasses::PluginsType;

namespace Dimmer {

    using ConfigType = KFCConfigurationClasses::Plugins::DimmerConfigNS::DimmerConfig::DimmerConfig_t;

    class Form {
    protected:
        virtual void readConfig(ConfigType &cfg) = 0;
        virtual void writeConfig(ConfigType &cfg) = 0;

        void createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form);
    };

}
