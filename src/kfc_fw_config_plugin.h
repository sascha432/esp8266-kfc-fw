/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "kfc_fw_config.h"

class KFCConfigurationPlugin : public PluginComponent {
public:
    KFCConfigurationPlugin();

    virtual void setup(SetupModeType mode) override;
    virtual void reconfigure(const String &source) override;
    virtual void createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request) override;
    virtual WebTemplate *getWebTemplate(const String &templateName) override;
};

