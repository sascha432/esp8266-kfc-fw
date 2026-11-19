/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>
#include "mqtt_def.h"
#include "kfc_fw_config.h"

namespace MQTT {

    enum class ComponentType : uint8_t {
        SWITCH = 1,
        LIGHT,
        SENSOR,
        BINARY_SENSOR,
        DEVICE_AUTOMATION,
        FAN,
        BUTTON
    };

    enum class FormatType : uint8_t {
        JSON,       // topic and payload
        TOPIC,      // topic only
    };

    // result of an auto discovery run for the WebUI
    enum class StatusType : uint8_t {
        STARTED,
        SUCCESS,
        FAILURE,
    };

    class Component;
    class Client;
    class Plugin;

    namespace AutoDiscovery {

        class Entity;
        class Discovery;

        using EntityPtr = Entity *;
        using StatusCallback = std::function<void(StatusType status)>;

    };

    using namespace KFCConfigurationClasses::Plugins::MQTTConfigNS;

    using Plugins = KFCConfigurationClasses::PluginsType;
    using ComponentPtr = Component *;
    using ClientPtr = Client *;
    using NameType = const __FlashStringHelper *;
    using ComponentVector = std::vector<ComponentPtr>;

    // maximum time to establish the connection, the keepalive is added
    static constexpr uint32_t kConnectTimeout = 30; // seconds
    // the connection is closed if the server does not acknowledge a packet within this time
    static constexpr uint32_t kAckTimeout = 30000; // milliseconds

}
