/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include <kfc_fw_config.h>

namespace KFCConfigurationClasses {

    void Plugins::MQTTClient::defaults()
    {
        System::Flags::getWriteable().mqttEnabled = true;
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
        return System::Flags::get().mqttEnabled && (getConfig().mode_enum != ModeType::DISABLED);
    }

    // Plugins::MQTTClient::MqttConfig_t Plugins::MQTTClient::getConfig()
    // {
    //     return config._H_GET(MainConfig().plugins.mqtt.cfg);
    // }

    // void Plugins::MQTTClient::setConfig(const MqttConfig_t &params)
    // {
    //     config._H_SET(MainConfig().plugins.mqtt.cfg, params);
    // }

    // Plugins::MQTTClient::MqttConfig_t &Plugins::MQTTClient::getWriteableConfig()
    // {
    //     return config._H_W_GET(MainConfig().plugins.mqtt.cfg);
    // }

    // const char *Plugins::MQTTClient::getHostname()
    // {
    //     return config._H_STR(MainConfig().plugins.mqtt.hostname);
    // }

    // const char *Plugins::MQTTClient::getUsername()
    // {
    //     return config._H_STR(MainConfig().plugins.mqtt.username);
    // }

    // const char *Plugins::MQTTClient::getPassword()
    // {
    //     return config._H_STR(MainConfig().plugins.mqtt.password);
    // }

    const uint8_t *Plugins::MQTTClient::getFingerPrint(uint16_t &size)
    {
        size = 0;
        return config.getBinary(_H(MainConfig().plugins.mqtt.fingerprint), size);
    }

    // const char *Plugins::MQTTClient::getTopic()
    // {
    //     return config._H_STR(MainConfig().plugins.mqtt.topic);
    // }

    // const char *Plugins::MQTTClient::getAutoDiscoveryPrefix()
    // {
    //     return config._H_STR(MainConfig().plugins.mqtt.discovery_prefix);
    // }

    // void Plugins::MQTTClient::setHostname(const char *hostname)
    // {
    //     // TODO extract function
    //     const char *ptr;
    //     if ((ptr = strchr_P(hostname, ':')) != nullptr) {
    //         String tmp = FPSTR(hostname);
    //         String_rtrim(tmp);
    //         uint16_t port = 0;
    //         int pos = tmp.indexOf(':');
    //         if (pos != -1 && tmp.length() - pos <= 6) {
    //             char *end = nullptr;
    //             port = (uint16_t)strtoul(tmp.c_str() + pos + 1, &end, 10);
    //             if (end && *end) {
    //                 // invalid value
    //                 port = 0;
    //             }
    //         }
    //         if (port) {
    //             config._H_SET_STR(MainConfig().plugins.mqtt.hostname, tmp.substring(0, pos - 1));
    //             getWriteableConfig().port = (uint16_t)atoi(tmp.c_str() + pos + 1);
    //             return;
    //         }
    //     }
    //     config._H_SET_STR(MainConfig().plugins.mqtt.hostname, hostname);
    // }

    // void Plugins::MQTTClient::setUsername(const char *username)
    // {
    //     config._H_SET_STR(MainConfig().plugins.mqtt.username, username);
    // }

    // void Plugins::MQTTClient::setPassword(const char *password)
    // {
    //     config._H_SET_STR(MainConfig().plugins.mqtt.password, password);
    // }

    void Plugins::MQTTClient::setFingerPrint(const uint8_t *fingerprint, uint16_t size)
    {
        config.setBinary(_H(MainConfig().plugins.mqtt.fingerprint), fingerprint, size);
    }

    // void Plugins::MQTTClient::setTopic(const char *topic)
    // {
    //     config._H_SET_STR(MainConfig().plugins.mqtt.topic, topic);
    // }

    // void Plugins::MQTTClient::setAutoDiscoveryPrefix(const char *discovery)
    // {
    //     config._H_SET_STR(MainConfig().plugins.mqtt.discovery_prefix, discovery);
    // }

}
