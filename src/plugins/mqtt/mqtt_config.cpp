/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::MQTTClient::defaults()
    {
        System::Flags::getWriteable().is_mqtt_enabled = true;
        MqttConfig_t cfg = {};
        setConfig(cfg);
        setHostname(PSTR(CREATE_ZERO_CONF_DEFAULT("mqtt", "tcp", "address", "192.168.4.1")));
        setTopic(PSTR("home/${device_name}"));
        setAutoDiscoveryPrefix(PSTR("homeassistant"));
        __CDBG_dump(MQTTClient, cfg);
        __CDBG_dumpString(Hostname);
        __CDBG_dumpString(Username);
        __CDBG_dumpString(Password);
        __CDBG_dumpString(Topic);
        __CDBG_dumpString(AutoDiscoveryPrefix);
    }

    bool Plugins::MQTTClient::isEnabled()
    {
        return System::Flags(true).isMQTTEnabled();
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
