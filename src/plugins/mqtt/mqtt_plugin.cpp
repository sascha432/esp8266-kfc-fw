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
using Plugins = KFCConfigurationClasses::PluginsType;

using namespace MQTT;

static Plugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Plugin,
    "mqtt",             // name
    "MQTT",             // friendly name
    "",                 // web_templates
    "mqtt",             // config_forms
    "network,station-*", // reconfigure_dependencies
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

void Plugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    Client::setupInstance();
}

void Plugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    Client::deleteInstance();
    Client::setupInstance();
}

void Plugin::getStatus(Print &output)
{
    auto client = Client::getClient();
    if (client) {
        client->printStatus(output);
    }
    else {
        output.print(FSPGM(Disabled));
    }
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

#if MQTT_AUTO_DISCOVERY
#    define COMMANDS "con|dis|set|top|auto|list"
#else
#    define COMMANDS "con|dis|set|top"
#endif

enum class Command {
    INVALID = -1,
    CONNECT,
    DISCONNECT,
    SET,
    TOPICS,
    AUTO_DISCOVERY,
    LIST
};

PROGMEM_AT_MODE_HELP_COMMAND_DEF(MQTT, "MQTT", "<" COMMANDS ">", "Manage MQTT\n"
    "    con[nect]                                   Connect to server\n"
    "    dis[connect][,<reconnect>]                  Disconnect from server, reconnect after the reconnect delay if true\n"
    "    set,<enable,disable>                        Enable or disable MQTT\n"
    "    top[ics]                                    List subscribed topics\n"
    #if MQTT_AUTO_DISCOVERY
    "    auto[discovery]                             Publish auto discovery\n"
    "    list[,full]                                 List auto discovery topics (and payloads)\n"
    #endif
    ,
    "Display MQTT status, outbox and the memory usage of the last auto discovery"
);

#if MQTT_AUTO_DISCOVERY

static void printAutoDiscovery(Print &output, bool full)
{
    output.printf_P(PSTR("Prefix: %s\nBase topic: %s\n"), Plugins::MqttClient::getAutoDiscoveryPrefix(), MQTT::Client::getBaseTopic().c_str());
    auto format = full ? FormatType::JSON : FormatType::TOPIC;
    for (auto component : MQTT::Client::getComponents()) {
        auto count = component->getAutoDiscoveryCount();
        for (uint8_t i = 0; i < count; i++) {
            std::unique_ptr<AutoDiscovery::Entity> entity(component->getAutoDiscovery(format, i));
            if (!entity) {
                continue;
            }
            output.print(entity->getTopic());
            if (full) {
                entity->finalize();
                output.print(':');
                output.print(entity->getPayload());
            }
            output.println();
            delay(5);
        }
    }
}

#endif

bool Plugin::atModeHandler(AtModeArgs &args)
{
    if (!args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(MQTT))) {
        return false;
    }
    auto clientPtr = Client::getClient();
    if (!clientPtr) {
        args.print(FSPGM(disabled));
        return true;
    }
    auto &client = *clientPtr;
    if (args.isQueryMode()) {
        client.printDebug(args.getStream());
        return true;
    }
    if (!args.requireArgs(1, 2)) {
        return true;
    }
    auto actionsStr = PSTR(COMMANDS);
    switch(static_cast<Command>(stringlist_find_P_P(actionsStr, args.get(0), '|'))) {
        case Command::CONNECT:
            client.connect();
            args.print(F("connecting"));
            break;
        case Command::DISCONNECT:
            client.disconnect(args.isTrue(1));
            args.print(F("disconnected"));
            break;
        case Command::SET:
            if (args.isTrue(1)) {
                System::Flags::getWriteableConfig().is_mqtt_enabled = true;
                config.write();
                args.print(F("enabled"));
            }
            else {
                client.disconnect(false);
                System::Flags::getWriteableConfig().is_mqtt_enabled = false;
                config.write();
                args.print(F("disconnected and disabled"));
            }
            break;
        case Command::TOPICS:
            for (const auto &subscription : client.getSubscriptions()) {
                args.print(F("topic=%s component=%p name=%s"), subscription.topic.c_str(), subscription.component, subscription.component->getName());
            }
            break;
        #if MQTT_AUTO_DISCOVERY
            case Command::AUTO_DISCOVERY:
                if (client.publishAutoDiscovery()) {
                    args.print(F("auto discovery starts in %u ms"), MQTT_DISCOVERY_REQUEST_DELAY_MS);
                }
                else {
                    args.print(F("not connected or auto discovery disabled"));
                }
                break;
            case Command::LIST:
                printAutoDiscovery(args.getStream(), args.equalsIgnoreCase(1, F("full")));
                break;
        #endif
        default:
            args.invalidArgument(0, FPSTR(actionsStr), '|');
            break;
    }
    return true;
}

#endif

Plugin &Plugin::getPlugin()
{
    return plugin;
}
