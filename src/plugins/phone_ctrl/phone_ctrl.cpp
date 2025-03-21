/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <KFCForms.h>
#include <KFCJson.h>
#include <Buffer.h>
#include "phone_ctrl.h"
#include "logger.h"
#include "web_server.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "Utility/ProgMemHelper.h"
#include <stl_ext/algorithm.h>

#if DEBUG_PHONE_CTRL_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
// class PhoneCtrlPlugin

class PhoneCtrlPlugin : public PluginComponent {
public:
    PhoneCtrlPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

#if AT_MODE_SUPPORTED
    #if AT_MODE_HELP_SUPPORTED
        virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    #endif
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

static PhoneCtrlPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    PhoneCtrlPlugin,
    "phonectrl",        // name
    "Phone Controller", // friendly name
    "",                 // web_templates
    "phone-ctrl",       // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::PHONE_CTRL,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

PhoneCtrlPlugin::PhoneCtrlPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(PhoneCtrlPlugin))
{
    REGISTER_PLUGIN(this, "PhoneCtrlPlugin");
}

void PhoneCtrlPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
}

void PhoneCtrlPlugin::reconfigure(const String &source)
{
}

void PhoneCtrlPlugin::shutdown()
{
}

void PhoneCtrlPlugin::getStatus(Print &output)
{
    // output.print(FSPGM(ping_monitor_service));
    // output.print(' ');
    // output.print(FSPGM(disabled));
}

void PhoneCtrlPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(ANSWER, "ANSWER", "<count=4>", "Answer doorbell");

#if AT_MODE_HELP_SUPPORTED

ATModeCommandHelpArrayPtr PingMonitorPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(PING)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

#endif

bool PhoneCtrlPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(ANSWER))) {
        return true;
    }
    return false;
}

#endif
