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
using KFCConfigurationClasses::Plugins;

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

PROGMEM_AT_MODE_HELP_COMMAND_DEF(MQTT, "MQTT", "<connect|disconnect|set|topics|autodiscovery>", "Manage MQTT\n"
    "\n"
    "    connect or con                              Connect to server\n"
    "    disconnect or dis[,<true|false>]            Disconnect from server and enalbe/disable auto reconnect\n"
    "    set,<enable,disable>                        Enable or disable MQTT\n"
    "    topics or top                               List subscribed topics\n"
    "    autodiscovery or auto                       Publish auto discovery\n"
    "\n",
    "Display MQTT status"
);

ATModeCommandHelpArray MQTTPlugin::atModeCommandHelp(size_t &size) const
{
    size = 1;
    return { PROGMEM_AT_MODE_HELP_COMMAND(MQTT) };
}

bool MQTTPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTT))) {
        auto clientPtr = MQTTClient::getClient();
        if (!clientPtr) {
            args.print(FSPGM(disabled));
        }
        else if (args.isQueryMode()) {
            args.printf_P(PSTR("status: %s"), clientPtr->connectionStatusString().c_str());
        }
        else if (args.requireArgs(1, 2)) {
            auto &client = *clientPtr;
            auto actionsStr = PSTR("connect|con|disconnect|dis|set|topics|top|autodiscovery|auto");
            auto action = stringlist_find_P_P(actionsStr, args.get(0), '|');
            switch(action) {
                case 0: // connext
                case 1: // con
                    if (Plugins::MQTTClient::isEnabled()) {
                        args.printf_P(PSTR("connecting to %s"), client.connectionStatusString().c_str());
                        client.setAutoReconnect(MQTT_AUTO_RECONNECT_TIMEOUT);
                        client.connect();
                    }
                    else {
                        args.print(F("disabled or not configured"));
                    }
                    break;
                case 2: // disconnect
                case 3: // dis
                    client.setAutoReconnect(args.isTrue(1) ? MQTT_AUTO_RECONNECT_TIMEOUT : 0);
                    client.disconnect(false);
                    args.print(F("disconnectd"));
                    break;
                case 4: // set
                    if  (args.isTrue(1)) {
                        System::Flags::getWriteableConfig().is_mqtt_enabled = true;
                        config.write();
                        args.print(F("enabled"));
                    }
                    else {
                        client.setAutoReconnect(0);
                        client.disconnect(true);
                        System::Flags::getWriteableConfig().is_mqtt_enabled = false;
                        config.write();
                        args.print(F("disconnected and disabled"));
                    }
                    break;
                case 5: // topics
                case 6: // top
                    for(const auto &topic: client.getTopics()) {
                        args.printf_P(PSTR("topic=%s component=%p name=%s"), topic.getTopic().c_str(), topic.getComponent(), topic.getComponent()->getComponentName());
                    }
                    break;
                case 7: // autodiscovery
                case 8: // auto
                    if (client.isConnected() && client.publishAutoDiscovery()) {
                        args.print(F("auto discovery started"));
                    }
                    else {
                        args.print(F("not connected, auto discovery running or not available"));
                    }
                    break;
                default:
                    args.invalidArgument(0, FPSTR(actionsStr), '|');
                    break;
            }

        }
        return true;
    }
    return false;
}

#endif

MQTTPlugin &MQTTPlugin::getPlugin()
{
    return plugin;
}
