/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "ESPAsyncWebServer.h"
#include "Form.h"
#include "PluginComponent.h"
#include "../mqtt/mqtt_component.h"

class DimmerModuleForm {
protected:
    void _createConfigureForm(PluginComponent::FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request);
};
