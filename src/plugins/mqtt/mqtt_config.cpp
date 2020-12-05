/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::MQTTClient::defaults()
    {
        System::Flags::getWriteableConfig().is_mqtt_enabled = true;
        setConfig(MqttConfig_t());
        setHostname(CREATE_ZERO_CONF(F("mqtt"), FSPGM(tcp), FSPGM(address), F("192.168.4.1")));
        setTopic(F("home/${device_name}"));
        setAutoDiscoveryPrefix(F("homeassistant"));
        // setSharedTopic(F("home/kfcfw/shared"));
    }

    bool Plugins::MQTTClient::isEnabled()
    {
        return ::KFCConfigurationClasses::System::Flags::getConfig().is_mqtt_enabled && (ConfigStructType::get_enum_mode(getConfig()) != ModeType::DISABLED);
    }

    const uint8_t *Plugins::MQTTClient::getFingerPrint(uint16_t &size)
    {
        size = 0;
        return config.getBinary(_H(MainConfig().plugins.mqtt.fingerprint), size);
    }

    void Plugins::MQTTClient::setFingerPrint(const uint8_t *fingerprint, uint16_t size)
    {
        config.setBinary(_H(MainConfig().plugins.mqtt.fingerprint), fingerprint, size);
    }

}
