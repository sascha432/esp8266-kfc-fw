/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    namespace Plugins {

        namespace MQTTConfigNS {

            void MqttClient::defaults()
            {
                System::Flags::getWriteableConfig().is_mqtt_enabled = true;
                setConfig(MqttConfigType());
                setHostname(CREATE_ZERO_CONF(F("mqtt"), FSPGM(tcp), FSPGM(address), F("192.168.4.1")));
                setBaseTopic(F("home/${object_id}"));
                setAutoDiscoveryPrefix(F("homeassistant"));
            }

            bool MqttClient::isEnabled()
            {
                return ::KFCConfigurationClasses::System::Flags::getConfig().is_mqtt_enabled && (getConfig()._get_enum_mode() != ModeType::DISABLED);
            }

        }

    }

}
