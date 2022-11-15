/**
 * Author: sascha_lammers@gmx.de
 */

#include <Configuration.hpp>
#include "kfc_fw_config.h"
#include <Utility/ProgMemHelper.h>
#include "save_crash.h"

#if DEBUG_CONFIG_CLASS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameDeviceConfig_t, "MainConfig().system.device.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameWebServerConfig_t, "MainConfig().system.webserver.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleStationsConfig, "MainConfig().network.settings.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameAlarm_t, "MainConfig().plugins.alarm.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSerial2TCPConfig_t, "MainConfig().plugins.serial2tcp.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameMqttConfig_t, "MainConfig().plugins.mqtt.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSyslogConfig_t, "MainConfig().plugins.syslog.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameNtpClientConfig_t, "MainConfig().plugins.ntpclient.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSensorConfig_t, "MainConfig().plugins.sensor.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameFlagsConfig_t, "MainConfig().system.flags.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameSoftAPConfig, "MainConfig().network.softap.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameBlindsConfig_t, "MainConfig().plugins.blinds.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameDimmerConfig_t, "MainConfig().plugins.dimmer.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameClockConfig_t, "MainConfig().plugins.clock.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNamePingConfig_t, "MainConfig().plugins.ping.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameWeatherStationConfig_t, "MainConfig().plugins.weatherstation.cfg");
DEFINE_CONFIG_HANDLE_PROGMEM_STR(handleNameRemoteConfig_t, "MainConfig().plugins.remote.cfg");


// --------------------------------------------------------------------
// exported static functions

namespace KFCConfigurationClasses {

    System::Flags::ConfigFlags_t::ConfigFlags_t() :
        is_factory_settings(true),
        is_default_password(true),
        is_softap_enabled(true),
        is_softap_ssid_hidden(false),
        is_softap_standby_mode_enabled(true),
        is_softap_dhcpd_enabled(true),
        is_station_mode_enabled(false),
        // is_station_mode_dhcp_enabled(true),
        use_static_ip_during_wakeup(true),
        is_at_mode_enabled(true),
        is_mdns_enabled(true),
        is_ntp_client_enabled(true),
        is_syslog_enabled(false),
        is_web_server_enabled(true),
        is_mqtt_enabled(false),
        is_rest_api_enabled(),
        is_serial2tcp_enabled(true),
        is_webui_enabled(true),
        is_ssdp_enabled(true),
        is_netbios_enabled(true),
        is_log_login_failures_enabled(false)
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
        config_magic(CONFIG_MAGIC_DWORD),
        factory_reset_unixtime(0),
        safe_mode_reboot_timeout_minutes(kDefaultValueFor_safe_mode_reboot_timeout_minutes),
        zeroconf_timeout(kDefaultValueFor_zeroconf_timeout),
        webui_cookie_lifetime_days(kDefaultValueFor_webui_cookie_lifetime_days),
        zeroconf_logging(false),
        status_led_mode(cast_int_status_led_mode(StatusLEDModeType::SOLID_WHEN_CONNECTED))
    {
    }

    FormUI::Container::List createFormPinList(uint8_t from, uint8_t to)
    {
    #if ESP8266
        return FormUI::Container::List(
            0, F("GPIO0"),
            1, F("GPIO1/TX"),
            2, F("GPIO2"),
            3, F("GPIO3/RX"),
            4, F("GPIO4"),
            5, F("GPIO5"),
            12, F("GPIO12"),
            13, F("GPIO13"),
            14, F("GPIO14"),
            15, F("GPIO15"),
            16, F("GPIO16/RTC")
        );
        // PROGMEM_DEF_LOCAL_PSTR_ARRAY(pinNames, "GPIO0", "U0TXD", "GPIO2", "U0RXD", "GPIO4", "GPIO5", "", "", "", "", "", "", "GPIO12", "GPIO13", "GPIO14", "GPIO15", "GPIO16/RTC_GPIO0");
    #else
        #ifndef NUM_DIGITAL_PINS
            #define NUM_DIGITAL_PINS 16
        #endif
        #ifndef isFlashInterfacePin
            #define isFlashInterfacePin(i) false
        #endif
        PROGMEM_DEF_LOCAL_PSTR_ARRAY_INDEXED(pinNames, 0, NUM_DIGITAL_PINS, "", GPIO);
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
    #endif
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


    // --------------------------------------------------------------------
    // Flags

    void System::Flags::defaults()
    {
        setConfig(ConfigFlags_t());
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

    void System::Device::defaults(bool factoryReset)
    {
        DeviceConfig_t cfg = {};
        cfg.config_version = SaveCrash::Data::FirmwareVersion().__version;
        if (factoryReset) {
            cfg.config_magic = CONFIG_MAGIC_DWORD;
            cfg.setLastFactoryResetTimestamp();
        }
        setConfig(cfg);
        setTitle(SPGM(KFC_Firmware, "KFC Firmware"));
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

    Network::Settings::StationModeSettings::StationModeSettings() :
        subnet(kDefaultValueFor_subnet),
        gateway(kDefaultValueFor_gateway),
        dns1(kDefaultValueFor_dns1),
        dns2(kDefaultValueFor_dns2),
        priority(0),
        enabled(false),
        dhcp(true)
    {
        uint8_t mac[6];
        ::WiFi.macAddress(mac);
        uint8_t tmp = (mac[5] <= 1 || mac[5] >= 253 ? (mac[4] <= 1 || mac[4] >= 253 ? (mac[3] <= 1 || mac[3] >= 253 ? mac[3] : rand() % 98 + 1) : mac[4]) : mac[5]);
        local_ip = kCreateIPv4Address(192, 168, 4, 0) | (tmp << 24U);
    }

    void Network::Settings::defaults()
    {
        setConfig(StationsConfig());
        // auto flags = System::Flags::getConfig();
        // flags.setWifiMode(WIFI_AP);
        // flags.is_softap_standby_mode_enabled = true;
        // flags.is_station_mode_dhcp_enabled = true;
        // System::Flags::setConfig(flags);
    }

    // --------------------------------------------------------------------
    // SoftAP

    Network::SoftAP::SoftAPConfig::SoftAPSettings::SoftAPSettings() :
        address(kDefaultValueFor_address),
        subnet(kDefaultValueFor_subnet),
        gateway(kDefaultValueFor_gateway),
        dhcp_start(kDefaultValueFor_dhcp_start),
        dhcp_end(kDefaultValueFor_dhcp_end),
        channel(kDefaultValueFor_channel),
        encryption(kDefaultValueFor_encryption)
    {
    }

    void Network::SoftAP::defaults()
    {
        SoftAPSettings cfg = {};
        setConfig(cfg);
    }

}
