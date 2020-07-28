/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"

#if DEBUG

const char *handleNameDeviceConfig_t PROGMEM = "MainConfig().system.device.cfg";
const char *handleNameWebServerConfig_t = "MainConfig().system.webserver";
const char *handleNameSettingsConfig_t = "MainConfig().system.settings";
const char *handleNameAlarm_t = "MainConfig().plugins.alarm.cfg";
const char *handleNameSerial2TCPConfig_t = "MainConfig().plugins.serial2tcp.cfg";
const char *handleNameMqttConfig_t = "MainConfig().plugins.mqtt.cfg";
const char *handleNameSyslogConfig_t = "MainConfig().plugins.syslog.cfg";
const char *handleNameNtpClientConfig_t = "MainConfig().plugins.ntpclient.cfg";

#endif

namespace KFCConfigurationClasses {

    const void *loadBinaryConfig(HandleType handle, uint16_t &length)
    {
        return config.getBinaryV(handle, length);
    }

    void *loadWriteableBinaryConfig(HandleType handle, uint16_t length)
    {
        return config.getWriteableBinary(handle, length);
    }

    void storeBinaryConfig(HandleType handle, const void *data, uint16_t length)
    {
        config.setBinary(handle, data, length);
    }

    const char *loadStringConfig(HandleType handle)
    {
        return config.getString(handle);
    }

    void storeStringConfig(HandleType handle, const char *str)
    {
        config.setString(handle, str);
    }

    // --------------------------------------------------------------------
    // Flags

    System::Flags::Flags() : _flags()
    {
        _flags.is_factory_settings = true;
        _flags.is_default_password = true;
        _flags.is_at_mode_enabled = true;
        _flags.setWifiMode(WIFI_AP_STA);
        _flags.is_station_mode_dhcp_enabled = true;
        _flags.is_softap_dhcpd_enabled = true;
        _flags.is_softap_standby_mode_enabled = true;
        _flags.is_mdns_enabled = true;
        _flags.is_ntp_client_enabled = true;
        _flags.is_web_server_enabled = true;
        _flags.is_webserver_performance_mode_enabled = true;
        _flags.is_led_on_when_connected = true;
        _flags.use_static_ip_during_wakeup = true;
        _flags.is_serial2tcp_enabled = true;
        _flags.is_syslog_enabled = false;
        _flags.is_mdns_enabled = true;
        _flags.is_webui_enabled = true;
    }

    System::Flags System::Flags::getFlags()
    {
        return Flags(config._H_GET(Config().flags));
    }

    ConfigFlags System::Flags::get()
    {
        return config._H_GET(Config().flags);
    }

    ConfigFlags &System::Flags::getWriteable()
    {
        return config._H_W_GET(Config().flags);
    }

    void System::Flags::set(ConfigFlags flags)
    {
        config._H_SET(Config().flags, flags);
    }

    void System::Flags::write()
    {
        config._H_SET(Config().flags, _flags);
    }

    bool System::Flags::isMQTTEnabled() const
    {
        if (_flags.is_mqtt_enabled) {
            if (Plugins::MQTTClient::getConfig().mode_enum != Plugins::MQTTClient::ModeType::DISABLED) {
                return true;
            }
        }
        return false;
    }

    void System::Flags::setMQTTEnabled(bool state)
    {
        _flags.is_mqtt_enabled = state;
        if (Plugins::MQTTClient::getConfig().mode_enum == Plugins::MQTTClient::ModeType::DISABLED) {
            auto &cfg = Plugins::MQTTClient::getWriteableConfig();
            cfg.mode_enum = (cfg.port == 8883) ? Plugins::MQTTClient::ModeType::SECURE : Plugins::MQTTClient::ModeType::UNSECURE;
        }
    }

    bool System::Flags::isSyslogEnabled() const
    {
        return _flags.is_syslog_enabled;
    }

    void System::Flags::setSyslogEnabled(bool state)
    {
        _flags.is_syslog_enabled = state;
    }

    // --------------------------------------------------------------------
    // WebServer

    void System::WebServer::defaults()
    {
        WebServerConfig_t cfg = {};
        setConfig(cfg);
        System::Flags::getWriteable().is_web_server_enabled = true;
    }

    System::WebServer::ModeType System::WebServer::getMode()
    {
        return !System::Flags::get().is_web_server_enabled ? ModeType::DISABLED : (getConfig().is_https ? ModeType::SECURE : ModeType::UNSECURE);
    }

    void System::WebServer::setMode(ModeType mode)
    {
        if (mode == ModeType::DISABLED) {
            System::Flags::getWriteable().is_web_server_enabled = false;
        }
        else {
            System::Flags::getWriteable().is_web_server_enabled = true;
            getWriteableConfig().is_https = (mode == ModeType::SECURE);
        }
    }

    // --------------------------------------------------------------------
    // Device

    void System::Device::defaults()
    {
        DeviceConfig_t cfg = {};
        setConfig(cfg);
        setTitle(SPGM(KFC_Firmware));
    }

    // --------------------------------------------------------------------
    // Firmare

    void System::Firmware::defaults()
    {
        setPluginBlacklist(emptyString);
    }

    const uint8_t *System::Firmware::getElfHash(uint16_t &length)
    {
        length = getElfHashSize();
        return config.getBinary(_H(MainConfig().system.firmware.elf_sha1), length);
    }

    uint8_t System::Firmware::getElfHashHex(String &str)
    {
        uint16_t length = 0;
        str = String();
        auto ptr = getElfHash(length);
        if (ptr && length == getElfHashSize()) {
            bin2hex_append(str, ptr, length);
        }
        return static_cast<uint8_t>(str.length());
    }

    void System::Firmware::setElfHash(const uint8_t *data)
    {
        if (data) {
            config.setBinary(_H(MainConfig().system.firmware.elf_sha1), data, getElfHashSize());
        }
    }

    void System::Firmware::setElfHashHex(const char *data)
    {
        if (data) {
            uint8_t buf[getElfHashSize()];
            hex2bin(buf, sizeof(buf), data);
            setElfHash(buf);
        }
    }

    // --------------------------------------------------------------------
    // Network

    void Network::Settings::defaults()
    {
        SettingsConfig_t cfg = {};
        setConfig(cfg);

        auto flags = System::Flags();
        flags->setWifiMode(WIFI_AP);
        flags->is_softap_standby_mode_enabled = true;
        flags->is_station_mode_dhcp_enabled = true;
        flags.write();
    }

    Network::SoftAP::SoftAP()
    {
        _address = IPAddress(192, 168, 4, 1);
        _subnet = IPAddress(255, 255, 255, 0);
        _gateway = IPAddress(192, 168, 4, 1);
        _dhcpStart = IPAddress(192, 168, 4, 2);
        _dhcpEnd = IPAddress(192, 168, 4, 100);
        _encryption = kWiFiEncryptionTypeDefault;
        _channel = 7;
    }

    Network::SoftAP Network::SoftAP::read()
    {
        return config._H_GET(MainConfig().network.softAp);
    }

    Network::SoftAP &Network::SoftAP::getWriteable()
    {
        return config._H_W_GET(MainConfig().network.softAp);
    }

    void Network::SoftAP::defaults()
    {
        auto softAp = SoftAP();
        softAp.write();

        auto flags = System::Flags();
        flags->is_softap_dhcpd_enabled = true;
        flags.write();
    }

    void Network::SoftAP::write()
    {
        config._H_SET(MainConfig().network.softAp, *this);
    }

}
