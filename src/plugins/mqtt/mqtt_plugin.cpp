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

using namespace MQTT;

static Plugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Plugin,
    "mqtt",             // name
    "MQTT",             // friendly name
    "",                 // web_templates
    "mqtt",             // config_forms
    "network",          // reconfigure_dependencies
    PluginComponent::PriorityType::MQTT,
    PluginComponent::RTCMemoryId::NONE,
#if MQTT_AUTO_DISCOVERY
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
#else
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
#endif
    false,              // allow_safe_mode
    true,               // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

Plugin::Plugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(Plugin))
{
    REGISTER_PLUGIN(this, "MQTTPlugin");
}

void Plugin::setup(SetupModeType mode)
{
    __LDBG_println();
    Client::setupInstance();
}

void Plugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    Client::deleteInstance();
    Client::setupInstance();
}

void Plugin::shutdown()
{
    __LDBG_println();
#if MQTT_SET_LAST_WILL == 0
    auto client = Client::getClient();
    if (client) {
        // send last will manually and give system some time to push out the tcp buffer
        client->disconnect(false);
        delay(100);
    }
#endif
    // crashing sometimes
    // MQTTClient::deleteInstance();
}

void Plugin::getStatus(Print &output)
{
    auto client = Client::getClient();
    if (client) {
        output.print(client->connectionStatusString());
        output.printf_P(PSTR(HTML_S(br) "%u components, %u entities" HTML_S(br)), client->_components.size(), AutoDiscovery::List::size(client->_components));
    }
    else {
        output.print(FSPGM(Disabled));
    }
}


#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF(MQTT, "MQTT", "<connect|disconnect|set|topics|autodiscovery|list>", "Manage MQTT\n"
    "\n"
    "    connect or con                              Connect to server\n"
    "    disconnect or dis[,<true|false>]            Disconnect from server and enalbe/disable auto reconnect\n"
    "    set,<enable,disable>                        Enable or disable MQTT\n"
    "    topics or top                               List subscribed topics\n"
    "    autodiscovery or auto[,restart][,force]     Publish auto discovery\n"
    "    list[,<full|crc>]                           List auto discovery\n"
    "\n",
    "Display MQTT status"
);

ATModeCommandHelpArrayPtr Plugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = { PROGMEM_AT_MODE_HELP_COMMAND(MQTT), PROGMEM_AT_MODE_HELP_COMMAND(MQTT) };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool Plugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTT))) {
        auto clientPtr = Client::getClient();
        if (!clientPtr) {
            args.print(FSPGM(disabled));
        }
        else if (args.isQueryMode()) {
            args.printf_P(PSTR("status: %s"), clientPtr->connectionStatusString().c_str());
        }
        else if (args.requireArgs(1, 3)) {
            auto &client = *clientPtr;
            auto actionsStr = PSTR("connect|con|disconnect|dis|set|topics|top|autodiscovery|auto|list");
            auto action = stringlist_find_P_P(actionsStr, args.get(0), '|');
            switch(action) {
                case 0: // connext
                case 1: // con
                    if (Plugins::MQTTClient::isEnabled()) {
                        args.printf_P(PSTR("connecting to %s"), client.connectionStatusString().c_str());
                        client.setAutoReconnect();
                        client.connect();
                    }
                    else {
                        args.print(F("disabled or not configured"));
                    }
                    break;
                case 2: // disconnect
                case 3: // dis
                    if (args.isTrue(1)) {
                        client.setAutoReconnect();
                    }
                    else {
                        client.setAutoReconnect(0);
                    }
                    client.disconnect(false);
                    args.print(F("disconnected"));
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
                        args.printf_P(PSTR("topic=%s component=%p name=%s"), topic.getTopic().c_str(), topic.getComponent(), topic.getComponent()->getName());
                    }
                    break;
                case 7: // autodiscovery
                case 8: { // auto
                        // bool abort = args.isTrue(1) || args.has(F("restart"));
                        // bool force_update = args.has(F("force"));


                        // RunFlags flags = RunFlags::FORCE_NOW;
                        // //TODO

                        // if (!client.isConnected()) {

                        //     args.print(F("mqtt not connected"));
                        // }
                        // else if (!abort && client.isAutoDiscoveryRunning()) {
                        //     args.print(F("auto discovery already running"));
                        // }
                        // else if (client.publishAutoDiscovery(true, abort, force_update)) {
                        //     args.print(F("auto discovery started"));
                        // }
                        // else {
                        //     args.print(F("auto discovery not available"));
                        // }
                    } break;
                case 9: {
                        auto &stream = args.getStream();
                        if (args.equalsIgnoreCase(1, F("crc"))) {
                            auto list = client.getAutoDiscoveryList(FormatType::JSON);
                            for(auto crc: list.crc()) {
                                stream.printf_P(PSTR("%08x\n"), crc);
                            }
                        }
                        else {
                            FormatType format = args.equalsIgnoreCase(1, F("full")) ? FormatType::JSON : FormatType::TOPIC;
                            auto &stream = args.getStream();
                            auto list = client.getAutoDiscoveryList(format);
                            if (list.empty()) {
                                args.printf_P(PSTR("No components available"));
                            }
                            else {
                                for(auto iterator = list.begin(); iterator != list.end(); ++iterator) {
                                    const auto &entity = *iterator;
                                // for(const auto &entity: list) {
                                    if (entity) {
                                        if (format == FormatType::TOPIC) {
                                            stream.println(entity->getTopic());
                                            delay(5);
                                        }
                                        else {
                                            stream.print(entity->getTopic());
                                            delay(5);
                                            stream.print(':');
                                            stream.println(entity->getPayload());
                                            delay(10);
                                        }
                                    }
                                    else {
                                        stream.println(F("entity=nullptr"));
                                        // stream.printf_P(PSTR("entity=nullptr component=%p index=%u size=%u\n"), iterator._component, iterator._index, iterator._size);
                                    }
                                }
                            }
                        }
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

Plugin &Plugin::getPlugin()
{
    return plugin;
}
