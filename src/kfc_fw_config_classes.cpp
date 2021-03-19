/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.h>
#include "kfc_fw_config.h"
#include <Utility/ProgMemHelper.h>

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
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameRemoteConfig_t, "MainConfig().plugins.remote.cfg");

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
        use_static_ip_during_wakeup(true),
        __reserved(false),
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
        is_webalerts_enabled(true),
        is_ssdp_enabled(true),
        is_netbios_enabled(true),
        is_log_login_failures_enabled(false),
        __reserved2(0)
    {
    }

    WebServerTypes::ModeType System::Flags::ConfigFlags_t::get_webserver_mode(const Type &obj) {
        return obj.is_web_server_enabled == false ? WebServerTypes::ModeType::DISABLED : (WebServer::getConfig().is_https ? WebServerTypes::ModeType::SECURE : WebServerTypes::ModeType::UNSECURE);
    }
    void System::Flags::ConfigFlags_t::set_webserver_mode(Type &obj, WebServerTypes::ModeType mode) {
        if (mode == WebServerTypes::ModeType::DISABLED) {
            obj.is_web_server_enabled = false;
        }
        else {
            obj.is_web_server_enabled = true;
            WebServer::getWriteableConfig().is_https = (mode == WebServerTypes::ModeType::SECURE);
        }
    }

    System::DeviceConfig::DeviceConfig_t::DeviceConfig_t() :
        config_version(FIRMWARE_VERSION),
        safe_mode_reboot_timeout_minutes(kDefaultValueFor_safe_mode_reboot_timeout_minutes),
        zeroconf_timeout(kDefaultValueFor_zeroconf_timeout),
        webui_cookie_lifetime_days(kDefaultValueFor_webui_cookie_lifetime_days),
        zeroconf_logging(false),
        status_led_mode(cast_int_status_led_mode(StatusLEDModeType::SOLID_WHEN_CONNECTED))
    {
    }

    FormUI::Container::List createFormPinList(uint8_t from, uint8_t to)
    {
#if defined(ESP8266)
        PROGMEM_DEF_LOCAL_PSTR_ARRAY(pinNames, "GPIO0", "U0TXD", "GPIO2", "U0RXD", "GPIO4", "GPIO5", "", "", "", "", "", "", "GPIO12", "GPIO13", "GPIO14", "GPIO15", "GPIO16/RTC_GPIO0");
#else
        #ifndef NUM_DIGITAL_PINS
        #define NUM_DIGITAL_PINS 16
        #endif
        #ifndef isFlashInterfacePin
        #define isFlashInterfacePin(i) false
        #endif
        PROGMEM_DEF_LOCAL_PSTR_ARRAY_INDEXED(pinNames, 0, NUM_DIGITAL_PINS, "", GPIO);
#endif

        FormUI::Container::List list;
        to = ((to >= NUM_DIGITAL_PINS) ? NUM_DIGITAL_PINS : to + 1);
        to = to ? to : NUM_DIGITAL_PINS;
        // __LDBG_printf("from=%u to=%u", from, to);
        for(uint8_t i = from; i < to; i++) {
            if (!isFlashInterfacePin(i)) {
                // __LDBG_printf("i=%u has_name=%u name=%s", i, i < kPinNamesNum, __S(pinNames[i]));
                if ((i < kpinNamesSize) && pgm_read_byte(pinNames[i])) {
                    list.emplace_back(i, FPSTR(pinNames[i]));
                }
                else {
                    list.emplace_back(i, PrintString(F("GPIO%u"), i));
                }
            }
        }
        return list;
    }

    // "${zeroconf:" service "." proto "," variable "|" default_value "}"
    String createZeroConf(const __FlashStringHelper *service, const __FlashStringHelper *proto, const __FlashStringHelper *varName, const __FlashStringHelper *defaultValue)
    {
        auto str = PrintString(F("%s%s.%s,%s"), SPGM(_var_zeroconf, "${zeroconf:"), service, proto, varName);
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
        setMD5(ESP.getSketchMD5());
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
