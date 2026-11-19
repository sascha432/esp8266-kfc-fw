/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

namespace KFCConfigurationClasses {

    namespace Plugins {

        // --------------------------------------------------------------------
        // MQTT::Client

        namespace MQTTConfigNS {

            enum class ModeType : uint8_t {
                MIN = 0,
                DISABLED = MIN,
                UNSECURE,
                SECURE,
                MAX
            };

            enum class QosType : uint8_t {
                MIN = 0,
                AT_MOST_ONCE = 0,
                AT_LEAST_ONCE = 1,
                EXACTLY_ONCE = 2,
                MAX,
                // the client waits for the acknowledgment of each auto discovery message before it sends the next one
                AUTO_DISCOVERY = AT_LEAST_ONCE,
                ONLINE_STATUS = AT_LEAST_ONCE,
                DEFAULT = 0xff,
            };

            static_assert(QosType::AUTO_DISCOVERY != QosType::AT_MOST_ONCE, "QoS 1 or 2 required");

            struct __attribute__packed__ MqttConfigType {
                using Type = MqttConfigType;

                AUTO_DEFAULT_PORT_CONST_SECURE(1883, 8883);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery, 1, 0, 1, 1);
                uint32_t _free0 : 1; // unused, keeps the stored layout
                CREATE_UINT32_BITFIELD_MIN_MAX(keepalive, 10, 0, 900, 15, 1);
                uint32_t _free1 : 16; // unused, keeps the stored layout
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_min, 16, 250, 60000, 1000, 1000);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_max, 16, 1000, 60000, 15000, 1000);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_incr, 7, 0, 100, 10);
                // the auto discovery starts after a random delay between 0 and this time after connecting
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery_delay, 10, 10, 900, 30, 1); // seconds
                uint32_t _free2 : 7; // avoid warning
                CREATE_ENUM_BITFIELD(mode, ModeType);
                CREATE_ENUM_BITFIELD(qos, QosType);
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, kPortDefault, kPortDefaultSecure, static_cast<ModeType>(mode) == ModeType::SECURE);

                MqttConfigType() :
                    auto_discovery(kDefaultValueFor_auto_discovery),
                    _free0(0),
                    keepalive(kDefaultValueFor_keepalive),
                    _free1(0),
                    auto_reconnect_min(kDefaultValueFor_auto_reconnect_min),
                    auto_reconnect_max(kDefaultValueFor_auto_reconnect_max),
                    auto_reconnect_incr(kDefaultValueFor_auto_reconnect_incr),
                    auto_discovery_delay(kDefaultValueFor_auto_discovery_delay),
                    mode(cast(ModeType::UNSECURE)),
                    qos(cast(QosType::AT_LEAST_ONCE)),
                    __port(kDefaultValueFor___port)
                {}

            };

            static constexpr size_t kConfigTypeSize = sizeof(MqttConfigType);

            class MqttClient : public KFCConfigurationClasses::ConfigGetterSetter<MqttConfigType, _H(MainConfig().plugins.mqtt.cfg) CIF_DEBUG(, &handleNameMqttConfig_t)> {
            public:
                static void defaults();
                static bool isEnabled();

                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Hostname, 1, 128);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Username, 0, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Password, 6, 32);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, BaseTopic, 4, 64);
                CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, AutoDiscoveryPrefix, 1, 32);
            };

        }
    }
}
