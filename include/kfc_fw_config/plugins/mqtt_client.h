/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config/base.h"
#include "ConfigurationHelper.h"

        // --------------------------------------------------------------------
        // MQTTClient

        class MQTTClientConfig {
        public:
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
                PERSISTENT_STORAGE = AT_LEAST_ONCE,
                AUTO_DISCOVERY = AT_LEAST_ONCE,
                DEFAULT = 0xff,
            };

            static_assert(QosType::AUTO_DISCOVERY != QosType::AT_MOST_ONCE, "QoS 1 or 2 required");

            AUTO_DEFAULT_PORT_CONST_SECURE(1883, 8883);

            typedef struct __attribute__packed__ MqttConfig_t {
                using Type = MqttConfig_t;
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery, 1, 0, 1, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(enable_shared_topic, 1, 0, 1, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(keepalive, 10, 0, 900, 15, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery_rebroadcast_interval, 16, 15, 43200, 24 * 60, 3600); // minutes
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_min, 16, 250, 60000, 5000, 500);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_max, 16, 5000, 60000, 60000, 1000);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_incr, 7, 0, 100, 10);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery_delay, 10, 10, 900, 30, 1); // seconds
                uint32_t _free2 : 7; // avoid warning
                CREATE_ENUM_BITFIELD(mode, ModeType);
                CREATE_ENUM_BITFIELD(qos, QosType);
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, kPortDefault, kPortDefaultSecure, static_cast<ModeType>(mode) == ModeType::SECURE);

                // minutes
                uint32_t getAutoDiscoveryRebroadcastInterval() const {
                    return auto_discovery && auto_discovery_delay ? auto_discovery_rebroadcast_interval : 0;
                }

                MqttConfig_t() :
                    auto_discovery(kDefaultValueFor_auto_discovery),
                    enable_shared_topic(kDefaultValueFor_enable_shared_topic),
                    keepalive(kDefaultValueFor_keepalive),
                    auto_discovery_rebroadcast_interval(kDefaultValueFor_auto_discovery_rebroadcast_interval),
                    auto_reconnect_min(kDefaultValueFor_auto_reconnect_min),
                    auto_reconnect_max(kDefaultValueFor_auto_reconnect_max),
                    auto_reconnect_incr(kDefaultValueFor_auto_reconnect_incr),
                    auto_discovery_delay(kDefaultValueFor_auto_discovery_delay),
                    mode(cast(ModeType::UNSECURE)),
                    qos(cast(QosType::EXACTLY_ONCE)),
                    __port(kDefaultValueFor___port)
                {}

            } MqttConfig_t;

            static constexpr size_t MqttConfig_tSize = sizeof(MqttConfig_t);
        };

        class MQTTClient : public MQTTClientConfig, public KFCConfigurationClasses::ConfigGetterSetter<MQTTClientConfig::MqttConfig_t, _H(MainConfig().plugins.mqtt.cfg) CIF_DEBUG(, &handleNameMqttConfig_t)> {
        public:
            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Hostname, 1, 128);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Username, 0, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Password, 6, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, BaseTopic, 4, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, GroupTopic, 4, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, AutoDiscoveryPrefix, 1, 32);
            // CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, SharedTopic, 4, 128);

            static const uint8_t *getFingerPrint(uint16_t &size);
            static void setFingerPrint(const uint8_t *fingerprint, uint16_t size);
            static constexpr size_t kFingerprintMaxSize = 20;
        };
