/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"

#if DEBUG_CONFIG_CLASS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameDeviceConfig_t, "MainConfig().system.device.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameWebServerConfig_t, "MainConfig().system.webserver.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSettingsConfig_t, "MainConfig().network.settings.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameAlarm_t, "MainConfig().plugins.alarm.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSerial2TCPConfig_t, "MainConfig().plugins.serial2tcp.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameMqttConfig_t, "MainConfig().plugins.mqtt.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSyslogConfig_t, "MainConfig().plugins.syslog.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameNtpClientConfig_t, "MainConfig().plugins.ntpclient.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSensorConfig_t, "MainConfig().plugins.sensor.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameFlagsConfig_t, "MainConfig().system.flags.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSoftAPConfig_t, "MainConfig().network.softap.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameBlindsConfig_t, "MainConfig().plugins.blinds.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameDimmerConfig_t, "MainConfig().plugins.dimmer.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameClockConfig_t, "MainConfig().plugins.clock.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNamePingConfig_t, "MainConfig().plugins.ping.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameWeatherStationConfig_t, "MainConfig().plugins.weatherstation.cfg");

namespace KFCConfigurationClasses {

    System::Flags::ConfigFlags_t::ConfigFlags_t() :
        is_factory_settings(true),
        is_default_password(true),
        is_softap_enabled(true),
        is_softap_ssid_hidden(false),
        is_softap_standby_mode_enabled(true),
        is_softap_dhcpd_enabled(true),
        is_station_mode_enabled(false),
        is_station_mode_dhcp_enabled(true),
        use_static_ip_during_wakeup(false),
        is_led_on_when_connected(true),
        is_at_mode_enabled(true),
        is_mdns_enabled(true),
        is_ntp_client_enabled(true),
        is_syslog_enabled(false),
        is_web_server_enabled(true),
        is_webserver_performance_mode_enabled(true),
        is_mqtt_enabled(false),
        is_rest_api_enabled(),
        is_serial2tcp_enabled(true),
        is_webui_enabled(true),
        is_webalerts_enabled(),
        is_ssdp_enabled(true),
        __reserved(0),
        __reserved2(0)
    {
    }

    // "${zeroconf:" service "." proto "," variable "|" default_value "}"
    String createZeroConf(const __FlashStringHelper *service, const __FlashStringHelper *proto, const __FlashStringHelper *varName, const __FlashStringHelper *defaultValue)
    {
        auto str = PrintString(F("${zeroconf:%s.%s,%s"), service, proto, varName);
        if (defaultValue && pgm_read_byte(defaultValue)) {
            str.print('|');
            str.print(defaultValue);
        }
        str.print('}');
        return str;
    }


    const void *loadBinaryConfig(HandleType handle, uint16_t &length)
    {
        auto data = config.getBinaryV(handle, length);
        // __CDBG_printf("handle=%04x data=%p len=%u", handle, data, length);
        return data;
    }

    void *loadWriteableBinaryConfig(HandleType handle, uint16_t length)
    {
        auto data = config.getWriteableBinary(handle, length);
        __CDBG_printf("handle=%04x data=%p len=%u", handle, data, length);
        return data;
    }

    void storeBinaryConfig(HandleType handle, const void *data, uint16_t length)
    {
        __CDBG_printf("handle=%04x data=%p len=%u", handle, data, length);
        config.setBinary(handle, data, length);

    }


    const char *loadStringConfig(HandleType handle)
    {
        auto str = config.getString(handle);
        // __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        return str;
    }

    char *loadWriteableStringConfig(HandleType handle, uint16_t size)
    {
        return config.getWriteableString(handle, size);
    }

    void storeStringConfig(HandleType handle, const char *str)
    {
        __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        config.setString(handle, str);
    }

    void storeStringConfig(HandleType handle, const __FlashStringHelper *str)
    {
        __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        config.setString(handle, str);
    }

    void storeStringConfig(HandleType handle, const String &str)
    {
        __CDBG_printf("handle=%04x str=%s len=%u", handle, _S_STR(str), _S_STRLEN(str));
        config.setString(handle, str);
    }

    // --------------------------------------------------------------------
    // Flags

    void System::Flags::defaults()
    {
        ConfigFlags_t tmp = {};
        setConfig(tmp);
    }
    // --------------------------------------------------------------------
    // WebServer

    void System::WebServer::defaults()
    {
        WebServerConfig_t cfg = {};
        setConfig(cfg);
        System::Flags::getWriteableConfig().is_web_server_enabled = true;
    }

    System::WebServer::ModeType System::WebServer::getMode()
    {
        return !System::Flags::getConfig().is_web_server_enabled ? ModeType::DISABLED : (getConfig().is_https ? ModeType::SECURE : ModeType::UNSECURE);
    }

    void System::WebServer::setMode(ModeType mode)
    {
        if (mode == ModeType::DISABLED) {
            System::Flags::getWriteableConfig().is_web_server_enabled = false;
        }
        else {
            System::Flags::getWriteableConfig().is_web_server_enabled = true;
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

    // --------------------------------------------------------------------
    // Settings

    Network::Settings::SettingsConfig_t::SettingsConfig_t() :
        subnet(kCreateIPv4Address(255, 255, 255, 0)),
        gateway(kCreateIPv4Address(192, 168, 4, 1)),
        dns1(kCreateIPv4Address(8, 8, 8, 8)),
        dns2(kCreateIPv4Address(8, 8, 4, 4))
    {
        uint8_t mac[6];
        ::WiFi.macAddress(mac);
        uint8_t tmp = (mac[5] <= 1 || mac[5] >= 253 ? (mac[4] <= 1 || mac[4] >= 253 ? (mac[3] <= 1 || mac[3] >= 253 ? mac[3] : rand() % 98 + 1) : mac[4]) : mac[5]);
        local_ip = kCreateIPv4Address(192, 168, 4, 0) | (tmp << 24U);
    }

    void Network::Settings::defaults()
    {
        SettingsConfig_t cfg = {};
        setConfig(cfg);
        // auto flags = System::Flags::getConfig();
        // flags.setWifiMode(WIFI_AP);
        // flags.is_softap_standby_mode_enabled = true;
        // flags.is_station_mode_dhcp_enabled = true;
        // System::Flags::setConfig(flags);
    }

    // --------------------------------------------------------------------
    // SoftAP

    Network::SoftAP::SoftAPConfig_t::SoftAPConfig_t() :
        address(kCreateIPv4Address(192, 168, 4, 1)),
        subnet(kCreateIPv4Address(255, 255, 255, 0)),
        gateway(kCreateIPv4Address(192, 168, 4, 1)),
        dhcp_start(kCreateIPv4Address(192, 168, 4, 2)),
        dhcp_end(kCreateIPv4Address(192, 168, 4, 100)),
        channel(7),
        encryption_enum(kWiFiEncryptionTypeDefault)
    {
    }

    void Network::SoftAP::defaults()
    {
        SoftAPConfig_t cfg = {};
        setConfig(cfg);
    }

}
