/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include "mqtt_plugin.h"

#if DEBUG_MQTT_CLIENT
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

static MQTTPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    MQTTPlugin,
    "mqtt",             // name
    "MQTT",             // friendly name
    "",                 // web_templates
    "mqtt",             // config_forms
    "network",          // reconfigure_dependencies
    PluginComponent::PriorityType::MQTT,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

MQTTPlugin::MQTTPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(MQTTPlugin))
{
    REGISTER_PLUGIN(this, "MQTTPlugin");
}

void MQTTPlugin::setup(SetupModeType mode)
{
    __LDBG_println();
    MQTTClient::setupInstance();
}

void MQTTPlugin::reconfigure(const String &source)
{
    __LDBG_printf(PSTR("source=%s"), source.c_str());
    // deletes old instance and if enabled, creates new one
    MQTTClient::setupInstance();
}

void MQTTPlugin::shutdown()
{
    __LDBG_println();
    // crashing sometimes
    // MQTTClient::deleteInstance();
}

void MQTTPlugin::getStatus(Print &output)
{
    auto client = MQTTClient::getClient();
    if (client) {
        output.print(client->connectionStatusString());
        output.printf_P(PSTR(HTML_S(br) "%u components, %u entities" HTML_S(br)), client->_components.size(), client->_componentsEntityCount);
    }
    else {
        output.print(FSPGM(Disabled));
    }
}


#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF(MQTT, "MQTT", "<connect|disconnect|force-disconnect|secure|unsecure|disable|topics>", "Connect or disconnect from server", "Display MQTT status");

void MQTTPlugin::atModeHelpGenerator()
{
    auto name = getName_P();
    at_mode_add_help(PROGMEM_AT_MODE_HELP_COMMAND_T(MQTT), name);
}

bool MQTTPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTT))) {
        auto client = MQTTClient::getClient();
        auto &serial = args.getStream();
        if (args.isQueryMode()) {
            if (client) {
                args.printf_P(PSTR("status: %s"), client->connectionStatusString().c_str());
            } else {
                serial.println(FSPGM(disabled));
            }
        } else if (client && args.requireArgs(1, 1)) {
            auto &client = *MQTTClient::getClient();
            if (args.isTrue(0)) {
                System::Flags::getWriteable().is_mqtt_enabled = true;
                MQTTClient::ClientConfig::getWriteableConfig().mode_enum = MQTTClient::ModeType::UNSECURE;
                config.write();
                args.printf_P(PSTR("MQTT unsecure %s"), FSPGM(enabled));
            }
            else if (args.toLowerChar(0) == 't') {
                for(const auto &topic: client.getTopics()) {
                    args.printf_P(PSTR("topic=%s component=%p name=%s"), topic.getTopic().c_str(), topic.getComponent(), topic.getComponent()->getComponentName());
                }
            }
            else if (args.toLowerChar(0) == 'c') {
                args.printf_P(PSTR("connect: %s"), client.connectionStatusString().c_str());
                client.setAutoReconnect(MQTT_AUTO_RECONNECT_TIMEOUT);
                client.connect();
            }
            else if (args.toLowerChar(0) == 'd') {
                args.print(F("disconnect"));
                client.disconnect(false);
            }
            else if (args.toLowerChar(0) == 'f') {
                args.print(F("force disconnect"));
                client.setAutoReconnect(0);
                client.disconnect(true);
            }
            else if (args.toLowerChar(0) == 's') {
                System::Flags::getWriteable().is_mqtt_enabled = true;
                MQTTClient::ClientConfig::getWriteableConfig().mode_enum = MQTTClient::ModeType::SECURE;
                config.write();
                args.printf_P(PSTR("MQTT secure %s"), FSPGM(enabled));
            }
            else if (args.isFalse(0)) {
                System::Flags::getWriteable().is_mqtt_enabled = false;
                MQTTClient::ClientConfig::getWriteableConfig().mode_enum = MQTTClient::ModeType::DISABLED;
                config.write();
                client.setAutoReconnect(0);
                client.disconnect(true);
                args.printf_P(PSTR("MQTT %s"), FSPGM(disabled));
            }
        }
        return true;
    }
    return false;
}

#endif

MQTTPlugin &MQTTPlugin::getPlugin() {
    return plugin;
}
