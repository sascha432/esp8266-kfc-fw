/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    namespace Plugins {

        namespace MQTTConfigNS {

            void MqttClient::defaults()
            {
                System::Flags::getWriteableConfig().is_mqtt_enabled = true;
                setConfig(MqttConfigType());
                setHostname(CREATE_ZERO_CONF(F("mqtt"), FSPGM(tcp), FSPGM(address), F("192.168.4.1")));
                setBaseTopic(F("home/${device_name}"));
                setAutoDiscoveryPrefix(F("homeassistant"));
                // setSharedTopic(F("home/kfcfw/shared"));
            }

            bool MqttClient::isEnabled()
            {
                return ::KFCConfigurationClasses::System::Flags::getConfig().is_mqtt_enabled && (getConfig()._get_enum_mode() != ModeType::DISABLED);
            }

            const uint8_t *MqttClient::getFingerPrint(uint16_t &size)
            {
                size = 0;
                return config.getBinary(_H(MainConfig().plugins.mqtt.fingerprint), size);
            }

            void MqttClient::setFingerPrint(const uint8_t *fingerprint, uint16_t size)
            {
                config.setBinary(_H(MainConfig().plugins.mqtt.fingerprint), fingerprint, size);
            }

        }

    }

}
