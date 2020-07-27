/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"

namespace KFCConfigurationClasses {

    const void *loadBinaryConfig(HandleType handle, uint16_t length)
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
        _flags.isFactorySettings = true;
        _flags.isDefaultPassword = true;
        _flags.atModeEnabled = true;

        _flags.wifiMode = WIFI_AP_STA;
        _flags.stationModeDHCPEnabled = true;
        _flags.softAPDHCPDEnabled = true;

        _flags.mqttEnabled = true;

        _flags.ntpClientEnabled = true;

#if defined(ESP32) && WEBSERVER_TLS_SUPPORT
        _flags.webServerMode = HTTP_MODE_SECURE;
#else
        _flags.webServerMode = HTTP_MODE_UNSECURE;
#endif
        _flags.webServerPerformanceModeEnabled = true;

        _flags.ledMode = true;

        _flags.useStaticIPDuringWakeUp = true;

        _flags.serial2TCPEnabled = true;

        _flags.syslogEnabled = false;

        _flags.apStandByMode = true;

        _flags.enableMDNS = true;
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
        if (_flags.mqttEnabled) {
            if (Plugins::MQTTClient::getConfig().mode_enum != Plugins::MQTTClient::ModeType::DISABLED) {
                return true;
            }
        }
        return false;
    }

    void System::Flags::setMQTTEnabled(bool state)
    {
        _flags.mqttEnabled = state;
        if (Plugins::MQTTClient::getConfig().mode_enum == Plugins::MQTTClient::ModeType::DISABLED) {
            auto &cfg = Plugins::MQTTClient::getWriteableConfig();
            cfg.mode_enum = (cfg.port == 8883) ? Plugins::MQTTClient::ModeType::SECURE : Plugins::MQTTClient::ModeType::UNSECURE;
        }
    }

    bool System::Flags::isSyslogEnabled() const
    {
        return _flags.syslogEnabled;
    }

    void System::Flags::setSyslogEnabled(bool state)
    {
        _flags.syslogEnabled = state;
    }


    // --------------------------------------------------------------------
    // Device

    void System::Device::defaults()
    {
        DeviceSettings_t settings;
        settings._safeModeRebootTime = 0;
        settings._webUIKeepLoggedInDays = 30;
        settings._statusLedMode = static_cast<uint8_t>(StatusLEDModeEnum::SOLID_WHEN_CONNECTED);
        config._H_SET(MainConfig().system.device.settings, settings);

        setSafeModeRebootTime(0);
        setTitle(FSPGM(KFC_Firmware));
    }

    const char *System::Device::getName()
    {
        return config._H_STR(Config().device_name);
    }

    const char *System::Device::getTitle()
    {
        return config._H_STR(Config().device_title);
    }

    const char *System::Device::getPassword()
    {
        return config._H_STR(Config().device_pass);
    }

    const char *System::Device::getToken()
    {
        return config._H_STR(Config().device_token);
    }

    void System::Device::setName(const String &name)
    {
        config._H_SET_STR(Config().device_name, name);
    }

    void System::Device::setTitle(const String &title)
    {
        config._H_SET_STR(Config().device_title, title);
    }

    void System::Device::setPassword(const String &password)
    {
        config._H_SET_STR(Config().device_pass, password);
    }

    void System::Device::setToken(const String &token)
    {
        config._H_SET_STR(Config().device_token, token);
    }

    void System::Device::setSafeModeRebootTime(uint16_t minutes)
    {
        auto settings = config._H_GET(MainConfig().system.device.settings);
        settings._safeModeRebootTime = minutes;
        config._H_SET(MainConfig().system.device.settings, settings);
    }

    uint16_t System::Device::getSafeModeRebootTime()
    {
        return config._H_GET(MainConfig().system.device.settings)._safeModeRebootTime;
    }

    uint16_t System::Device::getWebUIKeepLoggedInDays()
    {
        return config._H_GET(MainConfig().system.device.settings)._webUIKeepLoggedInDays;
    }

    uint32_t System::Device::getWebUIKeepLoggedInSeconds()
    {
        return getWebUIKeepLoggedInDays() * 86400U;
    }

    void System::Device::setWebUIKeepLoggedInDays(uint16_t days)
    {
        auto settings = config._H_GET(MainConfig().system.device.settings);
        settings._webUIKeepLoggedInDays = days;
        config._H_SET(MainConfig().system.device.settings, settings);
    }

    void System::Device::setStatusLedMode(StatusLEDModeEnum mode)
    {
        auto settings = config._H_GET(MainConfig().system.device.settings);
        settings._statusLedMode = static_cast<uint8_t>(mode);
        config._H_SET(MainConfig().system.device.settings, settings);
    }

    System::Device::StatusLEDModeEnum System::Device::getStatusLedMode()
    {
        return static_cast<StatusLEDModeEnum>(config._H_GET(MainConfig().system.device.settings)._statusLedMode);
    }

    // --------------------------------------------------------------------
    // Firmare

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

    Network::Settings::Settings()
    {
        uint8_t mac[6];
        WiFi.macAddress(mac);
        _localIp = IPAddress(192, 168, 4, mac[5] <= 1 || mac[5] >= 253 ? (mac[4] <= 1 || mac[4] >= 253 ? (mac[3] <= 1 || mac[3] >= 253 ? mac[3] : rand() % 98 + 1) : mac[4]) : mac[5]);
        _gateway = IPAddress(192, 168, 4, 1);
        _subnet = IPAddress(255, 255, 255, 0);
        _dns1 = IPAddress(8, 8, 8, 8);
        _dns2 = IPAddress(8, 8, 4, 4);
    }

    void Network::Settings::write()
    {
        config._H_SET(MainConfig().network.settings, *this);
    }

    Network::Settings Network::Settings::read()
    {
        return config._H_GET(MainConfig().network.settings);
    }

    void Network::Settings::defaults()
    {
        auto settings = Settings();
        settings.write();

        auto flags = System::Flags();
        flags->wifiMode = WIFI_AP;
        flags->apStandByMode = true;
        flags->stationModeDHCPEnabled = true;
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
        return (Network::SoftAP)config._H_GET(MainConfig().network.softAp);
    }

    void Network::SoftAP::defaults()
    {
        auto softAp = SoftAP();
        softAp.write();

        auto flags = System::Flags();
        flags->softAPDHCPDEnabled = true;
        flags.write();
    }

    void Network::SoftAP::write()
    {
        config._H_SET(MainConfig().network.softAp, *this);
    }

    const char *Network::WiFiConfig::getSSID()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._ssid);
    }

    const char *Network::WiFiConfig::getPassword()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._password);
    }

    void Network::WiFiConfig::setSSID(const String &ssid)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._ssid, ssid);
    }

    void Network::WiFiConfig::setPassword(const String &password)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._password, password);
    }


    const char *Network::WiFiConfig::getSoftApSSID()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._softApSsid);
    }

    const char *Network::WiFiConfig::getSoftApPassword()
    {
        return config._H_STR(MainConfig().network.WiFiConfig._softApPassword);
    }

    void Network::WiFiConfig::setSoftApSSID(const String &ssid)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._softApSsid, ssid);
    }

    void Network::WiFiConfig::setSoftApPassword(const String &password)
    {
        config._H_SET_STR(MainConfig().network.WiFiConfig._softApPassword, password);
    }

}
