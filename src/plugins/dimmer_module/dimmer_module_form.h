/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <ESPAsyncWebServer.h>
#include <Form.h>
#include <PluginComponent.h>
#include <kfc_fw_config.h>
#include "../src/plugins/mqtt/mqtt_component.h"

class DimmerModuleForm {
protected:
    using Plugins = KFCConfigurationClasses::Plugins;
    using ConfigType = Plugins::DimmerConfig::DimmerConfig_t;

    virtual void readConfig(ConfigType &cfg) = 0;
    virtual void writeConfig(ConfigType &cfg) = 0;

    void _createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request);
};
