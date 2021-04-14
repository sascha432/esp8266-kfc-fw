/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "kfc_fw_config/base.h"

namespace KFCConfigurationClasses {

    struct System {

        // --------------------------------------------------------------------
        // Flags

        class FlagsConfig {
        public:
            typedef struct __attribute__packed__ ConfigFlags_t {
                using Type = ConfigFlags_t;
                CREATE_BOOL_BITFIELD(is_factory_settings);
                CREATE_BOOL_BITFIELD(is_default_password);
                CREATE_BOOL_BITFIELD(is_softap_enabled);
                CREATE_BOOL_BITFIELD(is_softap_ssid_hidden);
                CREATE_BOOL_BITFIELD(is_softap_standby_mode_enabled);
                CREATE_BOOL_BITFIELD(is_softap_dhcpd_enabled);
                CREATE_BOOL_BITFIELD(is_station_mode_enabled);
                CREATE_BOOL_BITFIELD(is_station_mode_dhcp_enabled);

                CREATE_BOOL_BITFIELD(use_static_ip_during_wakeup);
                bool __reserved: 1;
                CREATE_BOOL_BITFIELD(is_at_mode_enabled);
                CREATE_BOOL_BITFIELD(is_mdns_enabled);
                CREATE_BOOL_BITFIELD(is_ntp_client_enabled);
                CREATE_BOOL_BITFIELD(is_syslog_enabled);
                CREATE_BOOL_BITFIELD(is_web_server_enabled);
                CREATE_BOOL_BITFIELD(is_webserver_performance_mode_enabled);

                CREATE_BOOL_BITFIELD(is_mqtt_enabled);
                CREATE_BOOL_BITFIELD(is_rest_api_enabled);
                CREATE_BOOL_BITFIELD(is_serial2tcp_enabled);
                CREATE_BOOL_BITFIELD(is_webui_enabled);
                CREATE_BOOL_BITFIELD(is_webalerts_enabled);
                CREATE_BOOL_BITFIELD(is_ssdp_enabled);
                CREATE_BOOL_BITFIELD(is_netbios_enabled);
                CREATE_BOOL_BITFIELD(is_log_login_failures_enabled);

                uint8_t __reserved2;

                uint8_t getWifiMode() const {
                    return get_wifi_mode(*this);
                }
                void setWifiMode(uint8_t mode) {
                    set_wifi_mode(*this, mode);
                }

                static uint8_t get_wifi_mode(const Type &obj) {
                    return (obj.is_station_mode_enabled && obj.is_softap_enabled) ? WIFI_AP_STA : (obj.is_station_mode_enabled ? WIFI_STA : (obj.is_softap_enabled ? WIFI_AP : 0));
                }
                static void set_wifi_mode(Type &obj, uint8_t mode) {
                    obj.is_station_mode_enabled = (mode & WIFI_STA) == WIFI_STA;
                    obj.is_softap_enabled = (mode & WIFI_AP) == WIFI_AP;
                }
                static WebServerTypes::ModeType get_webserver_mode(const Type &obj);
                static void set_webserver_mode(Type &obj, WebServerTypes::ModeType mode);

                ConfigFlags_t();

            } ConfigFlags_t;
        };

        static_assert(sizeof(FlagsConfig::ConfigFlags_t) == 4, "invalid size");

        class Flags : public FlagsConfig, public ConfigGetterSetter<FlagsConfig::ConfigFlags_t, _H(MainConfig().system.flags) CIF_DEBUG(, &handleNameFlagsConfig_t)> {
        public:
            static void defaults();
        };

        // --------------------------------------------------------------------
        // Device

        class DeviceConfig {
        public:
            enum class StatusLEDModeType : uint8_t {
                OFF_WHEN_CONNECTED = 0,
                SOLID_WHEN_CONNECTED = 1,
                OFF = 2,
                MAX
            };

            typedef struct __attribute__packed__ DeviceConfig_t {
                using Type = DeviceConfig_t;

                uint32_t config_version;
                CREATE_UINT16_BITFIELD_MIN_MAX(safe_mode_reboot_timeout_minutes, 10, 5, 900, 0, 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(zeroconf_timeout, 16, 1000, 60000, 15000, 1000);
                CREATE_UINT16_BITFIELD_MIN_MAX(webui_cookie_lifetime_days, 10, 3, 720, 90, 30);
                CREATE_UINT16_BITFIELD(zeroconf_logging, 1);
                CREATE_ENUM_BITFIELD(status_led_mode, StatusLEDModeType);

                uint16_t getSafeModeRebootTimeout() const {
                    return safe_mode_reboot_timeout_minutes;
                }
                uint16_t getWebUICookieLifetime() const {
                    return webui_cookie_lifetime_days;
                }
                uint32_t getWebUICookieLifetimeInSeconds() const {
                    return webui_cookie_lifetime_days * 86400U;
                }
                StatusLEDModeType getStatusLedMode() const {
                    return get_enum_status_led_mode(*this);
                }

                DeviceConfig_t();

            } DeviceConfig_t;
        };

        class Device : public DeviceConfig, public ConfigGetterSetter<DeviceConfig::DeviceConfig_t, _H(MainConfig().system.device.cfg) CIF_DEBUG(, &handleNameDeviceConfig_t)> {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().system.device, Name, 3, 16);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().system.device, Title, 3, 32);

            // username is device name
            // in case this changes one day, this function will return the username
            static const char *getUsername() {
                return getName();
            }

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().system.device, Password, 6, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().system.device, Token, SESSION_DEVICE_TOKEN_MIN_LENGTH, 255);

            // static constexpr uint16_t kZeroConfMinTimeout = 1000;
            // static constexpr uint16_t kZeroConfMaxTimeout = 60000;

            // static constexpr uint16_t kWebUICookieMinLifetime = 3;
            // static constexpr uint16_t kWebUICookieMaxLifetime = 720;
        };

        // --------------------------------------------------------------------
        // Firmware

        class Firmware {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER(MainConfig().system.firmware, PluginBlacklist, 255);
            CREATE_STRING_GETTER_SETTER(MainConfig().system.firmware, MD5, 32);

            static const char *getFirmwareMD5() {
                auto md5Str = getMD5();
                if (!md5Str || !*md5Str) {
                    setMD5(ESP.getSketchMD5().c_str());
                    md5Str = getMD5();
                }
                return md5Str;
            }
        };

        class WebServerConfig {
        public:
            using ModeType = WebServerTypes::ModeType;

            AUTO_DEFAULT_PORT_CONST_SECURE(80, 443);

            typedef struct __attribute__packed__ WebServerConfig_t {
                using Type = WebServerConfig_t;

                CREATE_BOOL_BITFIELD(is_https);
#if SECURITY_LOGIN_ATTEMPTS
                CREATE_UINT8_BITFIELD_MIN_MAX(login_attempts, 7, 0, 100, 10, 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(login_timeframe, 12, 0, 3600, 300, 1);               // seconds
                CREATE_UINT16_BITFIELD_MIN_MAX(login_rewrite_interval, 7, 1, 120, 5, 1);            // minutes
                CREATE_UINT16_BITFIELD_MIN_MAX(login_storage_timeframe, 5, 1, 30, 1, 1);            // days
#endif
#if WEBSERVER_TLS_SUPPORT
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, kPortDefault, kPortDefaultSecure, is_https);
#else
                AUTO_DEFAULT_PORT_GETTER_SETTER(__port, kPortDefault);
#endif

#if SECURITY_LOGIN_ATTEMPTS
                uint32_t getLoginRewriteInterval() const {
                    return login_rewrite_interval * 60;
                }

                uint32_t getLoginStorageTimeframe() const {
                    return login_storage_timeframe * 86400;
                }
#endif

                WebServerConfig_t();

            } WebServerConfig_t;
        };

        class WebServer : public WebServerConfig, public ConfigGetterSetter<WebServerConfig::WebServerConfig_t, _H(MainConfig().system.webserver.cfg) CIF_DEBUG(, &handleNameWebServerConfig_t)> {
        public:
            static void defaults();

            static ModeType getMode();
            static void setMode(ModeType mode);

        };

        Flags flags;
        Device device;
        Firmware firmware;
        WebServer webserver;

    };

}
