/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <session.h>
#include <buffer.h>
#include <crc16.h>
#include <KFCForms.h>
#include <KFCSerialization.h>

#include <push_pack.h>

#include <kfc_fw_config_types.h>

class Form;
class FormLengthValidator;

#ifndef DEBUG_CONFIG_CLASS
#define DEBUG_CONFIG_CLASS                                                  0
#endif

#if DEBUG_CONFIGURATION_GETHANDLE && !DEBUG_CONFIG_CLASS
#error DEBUG_CONFIG_CLASS=1 required for DEBUG_CONFIGURATION_GETHANDLE=1
#endif

#if DEBUG_CONFIG_CLASS && 0
#define __CDBG_printf(...)                                                  __DBG_printf(__VA_ARGS__)
#define __CDBG_dump(class_name, cfg)                                        cfg.dump<class_name>();
#define __CDBG_dumpString(name)                                             DEBUG_OUTPUT.printf_P(PSTR("%s [%04X]=%s\n"), _STRINGIFY(name), k##name##ConfigHandle, get##name());
#define CONFIG_DUMP_STRUCT_INFO(type)                                       DEBUG_OUTPUT.printf_P(PSTR("--- config struct handle=%04x\n"), type::kConfigStructHandle);
#define CONFIG_DUMP_STRUCT_VAR(name)                                        { DEBUG_OUTPUT.print(_STRINGIFY(name)); DEBUG_OUTPUT.print("="); DEBUG_OUTPUT.println(this->name); }
#else
#define __CDBG_printf(...)
#define __CDBG_dump(class_name, cfg)
#define __CDBG_dumpString(name)
#define CONFIG_DUMP_STRUCT_INFO(type)
#define CONFIG_DUMP_STRUCT_VAR(name)
#endif

#undef DEFAULT

#define CREATE_ZERO_CONF(service, proto, ...)                               KFCConfigurationClasses::createZeroConf(service, proto, ##__VA_ARGS__)

#if !DEBUG_CONFIG_CLASS

#define REGISTER_CONFIG_GET_HANDLE(...)
#define REGISTER_CONFIG_GET_HANDLE_STR(...)
#define DECLARE_CONFIG_HANDLE_PROGMEM_STR(...)
#define DEFINE_CONFIG_HANDLE_PROGMEM_STR(...)
#define REGISTER_HANDLE_NAME(...)

#elif DEBUG_CONFIGURATION_GETHANDLE

namespace ConfigurationHelper {
    const uint16_t registerHandleName(const char *name, uint8_t type);
    const uint16_t registerHandleName(const __FlashStringHelper *name, uint8_t type);
}
#define REGISTER_CONFIG_GET_HANDLE(name)                                    const uint16_t __UNIQUE_NAME(__DBGconfigHandle_) = ConfigurationHelper::registerHandleName(_STRINGIFY(name), __DBG__TYPE_CONST)
#define REGISTER_CONFIG_GET_HANDLE_STR(str)                                 const uint16_t __UNIQUE_NAME(__DBGconfigHandle_) = ConfigurationHelper::registerHandleName(str, __DBG__TYPE_CONST)
#define DECLARE_CONFIG_HANDLE_PROGMEM_STR(name)                             extern const char *name PROGMEM;
#define DEFINE_CONFIG_HANDLE_PROGMEM_STR(name, str) \
    const char *name PROGMEM = str; \
    REGISTER_CONFIG_GET_HANDLE_STR(name);

#define __DBG__TYPE_NONE                                                    0
#define __DBG__TYPE_GET                                                     1
#define __DBG__TYPE_SET                                                     2
#define __DBG__TYPE_W_GET                                                   3
#define __DBG__TYPE_CONST                                                   4
#define __DBG__TYPE_DEFINE_PROGMEM                                          5

#define REGISTER_HANDLE_NAME(name, type)                                    ConfigurationHelper::registerHandleName(name, type)

#else

#define REGISTER_CONFIG_GET_HANDLE(...)
#define REGISTER_CONFIG_GET_HANDLE_STR(...)
#define DECLARE_CONFIG_HANDLE_PROGMEM_STR(name)                             extern const char *name PROGMEM;
#define DEFINE_CONFIG_HANDLE_PROGMEM_STR(name, str)                         const char *name PROGMEM = str;
#define REGISTER_HANDLE_NAME(...)

#endif

// #ifndef _H
// #warning _H_DEFINED_KFCCONFIGURATIONCLASSES                                  1
// #define _H_DEFINED_KFCCONFIGURATIONCLASSES                                  1
// #define _H(name)                                                            constexpr_crc16_update(_STRINGIFY(name), constexpr_strlen(_STRINGIFY(name)))
// #define CONFIG_GET_HANDLE_STR(name)                                         constexpr_crc16_update(name, constexpr_strlen(name))
// #endif

#define CREATE_STRING_GETTER_SETTER_BINARY(class_name, name, len) \
    static constexpr size_t k##name##MaxSize = len; \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static size_t get##name##Size() { return k##name##MaxSize; } \
    static const uint8_t *get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return loadBinaryConfig(k##name##ConfigHandle, k##name##MaxSize); } \
    static void set##name(const uint8_t *data) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeBinaryConfig(k##name##ConfigHandle, data, k##name##MaxSize); }

#define CREATE_STRING_GETTER_SETTER(class_name, name, len) \
    static constexpr size_t k##name##MaxSize = len; \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const char *get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return loadStringConfig(k##name##ConfigHandle); } \
    static char *getWriteable##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return loadWriteableStringConfig(k##name##ConfigHandle, k##name##MaxSize); } \
    static void set##name(const char *str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeStringConfig(k##name##ConfigHandle, str); } \
    static void set##name##CStr(const char *str) { set##name(str); } \
    static void set##name(const __FlashStringHelper *str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeStringConfig(k##name##ConfigHandle, str); } \
    static void set##name(const String &str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeStringConfig(k##name##ConfigHandle, str); }

#define CREATE_STRING_GETTER_SETTER_MIN_MAX(class_name, name, mins, maxs) \
    static FormLengthValidator &add##name##LengthValidator(Form &form, bool allowEmpty = false) { \
        return form.addValidator(FormLengthValidator(k##name##MinSize, k##name##MaxSize, allowEmpty)); \
    } \
    static FormLengthValidator &add##name##LengthValidator(const String &message, Form &form, bool allowEmpty = false) { \
        return form.addValidator(FormLengthValidator(message, k##name##MinSize, k##name##MaxSize, allowEmpty)); \
    } \
    static constexpr size_t k##name##MinSize = mins; \
    CREATE_STRING_GETTER_SETTER(class_name, name, maxs)

#define CREATE_GETTER_SETTER_IP(class_name, name) \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const IPAddress get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return IPAddress(*(uint32_t *)loadBinaryConfig(k##name##ConfigHandle, sizeof(uint32_t))); } \
    static void set##name(const IPAddress &address) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeBinaryConfig(k##name##ConfigHandle, &static_cast<uint32_t>(address), sizeof(uint32_t)); }

#define CREATE_IPV4_ADDRESS(name) \
    uint32_t name; \
    static void set_ipv4_##name(Type &obj, const IPAddress &addr) { \
        obj.name = static_cast<uint32_t>(addr); \
    } \
    static IPAddress get_ipv4_##name(const Type &obj) { \
        return obj.name; \
    }

#define AUTO_DEFAULT_PORT_CONST(auto, default_port) \
    static constexpr uint16_t kPortAuto = auto; \
    static constexpr uint16_t kPortDefault = default_port; \

#define AUTO_DEFAULT_PORT_CONST_SECURE(auto, unsecure, secure) \
    static constexpr uint16_t kPortAuto = auto; \
    static constexpr uint16_t kPortDefault = unsecure; \
    static constexpr uint16_t kPortDefaultSecure = secure;

#define AUTO_DEFAULT_PORT_GETTER_SETTER(port_name) \
    uint16_t port_name; \
    bool isPortAuto() const { \
        return (port_name == kPortAuto); \
    } \
    uint16_t getPort() const { \
        return isPortAuto() ? kPortDefault : kPortDefault; \
    } \
    String getPortAsString() const { \
        return isPortAuto() ? String() : String(port_name); \
    } \
    void setPort(int port) { \
        auto tmp = static_cast<uint16_t>(port); \
        if (tmp == kPortDefault) { \
            port_name = kPortAuto; \
        } \
        else { \
            port_name =tmp; \
        } \
    }

#define AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(port_name, is_secure) \
    uint16_t port_name; \
    bool isSecure() const { \
        return (is_secure); \
    } \
    bool isPortAuto() const { \
        return (port_name == kPortAuto || (port_name == kPortDefault && !isSecure()) || (port_name == kPortDefaultSecure && isSecure())); \
    } \
    uint16_t getPort() const { \
        return isPortAuto() ? (isSecure() ? kPortDefaultSecure : kPortDefault) : kPortDefault; \
    } \
    String getPortAsString() const { \
        return isPortAuto() ? String() : String(port_name); \
    } \
    void setPort(int port, bool secure) { \
        auto tmp = static_cast<uint16_t>(port); \
        if (tmp == kPortDefault && !secure) { \
            port_name = kPortAuto; \
        } \
        else if (tmp == kPortDefaultSecure && secure) { \
            port_name = kPortAuto; \
        } \
        else { \
            port_name = tmp; \
        } \
    }


#if DEBUG_CONFIG_CLASS

DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameDeviceConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameWebServerConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSettingsConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameAlarm_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSerial2TCPConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameMqttConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSyslogConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameNtpClientConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSensorConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameFlagsConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSoftAPConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameBlindsConfig_t);

#define CIF_DEBUG(...) __VA_ARGS__

#else

#define CIF_DEBUG(...)

#endif

namespace KFCConfigurationClasses {

    using HandleType = uint16_t;

    String createZeroConf(const __FlashStringHelper *service, const __FlashStringHelper *proto, const __FlashStringHelper *varName, const __FlashStringHelper *defaultValue = nullptr);
    const void *loadBinaryConfig(HandleType handle, uint16_t &length);
    void *loadWriteableBinaryConfig(HandleType handle, uint16_t length);
    void storeBinaryConfig(HandleType handle, const void *data, uint16_t length);
    const char *loadStringConfig(HandleType handle);
    char *loadWriteableStringConfig(HandleType handle, uint16_t size);
    void storeStringConfig(HandleType handle, const char *str);
    void storeStringConfig(HandleType handle, const __FlashStringHelper *str);
    void storeStringConfig(HandleType handle, const String &str);

    template<typename ConfigType, HandleType handleArg CIF_DEBUG(, const char **handleName)>
    class ConfigGetterSetter {
    public:
        static constexpr uint16_t kConfigStructHandle = handleArg;
        using ConfigStructType = ConfigType;

        static ConfigType getConfig()
        {
            __CDBG_printf("getConfig=%04x size=%u name=%s", kConfigStructHandle, sizeof(ConfigType), *handleName);
            REGISTER_HANDLE_NAME(*handleName, __DBG__TYPE_GET);
            uint16_t length = sizeof(ConfigType);
            auto ptr = loadBinaryConfig(kConfigStructHandle, length);
            if (!ptr || length != sizeof(ConfigType)) {
                // __CDBG_printf("binary handle=%04x name=%s stored_size=%u mismatch. setting default values size=%u", kConfigStructHandle, *handleName, length, sizeof(ConfigType));
                ConfigType cfg = {};
                void *newPtr = loadWriteableBinaryConfig(kConfigStructHandle, sizeof(ConfigType));
                ptr = memcpy(newPtr, &cfg, sizeof(ConfigType));
            }
            return *reinterpret_cast<const ConfigType *>(ptr);
        }

        static void setConfig(const ConfigType &params)
        {
            __CDBG_printf("setConfig=%04x size=%u name=%s", kConfigStructHandle, sizeof(ConfigType), *handleName);
            REGISTER_HANDLE_NAME(*handleName, __DBG__TYPE_SET);
            storeBinaryConfig(kConfigStructHandle, &params, sizeof(ConfigType));
        }

        static ConfigType &getWriteableConfig()
        {
            __CDBG_printf("getWriteableConfig=%04x name=%s size=%u", kConfigStructHandle, *handleName, sizeof(ConfigType));
            REGISTER_HANDLE_NAME(*handleName, __DBG__TYPE_W_GET);
            return *reinterpret_cast<ConfigType *>(loadWriteableBinaryConfig(kConfigStructHandle, sizeof(ConfigType)));
        }
    };

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
                CREATE_BOOL_BITFIELD(is_led_on_when_connected);
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
                CREATE_UINT8_BITFIELD(__reserved, 2);

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

                template<class T>
                void dump() {
                    CONFIG_DUMP_STRUCT_INFO(T);
                    CONFIG_DUMP_STRUCT_VAR(is_factory_settings);
                    CONFIG_DUMP_STRUCT_VAR(is_default_password);
                    CONFIG_DUMP_STRUCT_VAR(is_softap_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_softap_ssid_hidden);
                    CONFIG_DUMP_STRUCT_VAR(is_softap_standby_mode_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_softap_dhcpd_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_station_mode_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_station_mode_dhcp_enabled);

                    CONFIG_DUMP_STRUCT_VAR(use_static_ip_during_wakeup);
                    CONFIG_DUMP_STRUCT_VAR(is_led_on_when_connected);
                    CONFIG_DUMP_STRUCT_VAR(is_at_mode_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_mdns_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_ntp_client_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_syslog_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_web_server_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_webserver_performance_mode_enabled);

                    CONFIG_DUMP_STRUCT_VAR(is_mqtt_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_rest_api_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_serial2tcp_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_webui_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_webalerts_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_ssdp_enabled);
                }

                ConfigFlags_t();

            } ConfigFlags_t;
        };

        class Flags : public FlagsConfig, public ConfigGetterSetter<FlagsConfig::ConfigFlags_t, _H(MainConfig().system.flags.cfg) CIF_DEBUG(, &handleNameFlagsConfig_t)> {
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
                MAX
            };

            typedef struct __attribute__packed__ DeviceConfig_t {
                using Type = DeviceConfig_t;

                uint32_t config_version;
                uint16_t safe_mode_reboot_timeout_minutes;
                uint16_t zeroconf_timeout;
                CREATE_UINT16_BITFIELD(webui_cookie_lifetime_days, 10);
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

                DeviceConfig_t() :
                    config_version(FIRMWARE_VERSION),
                    safe_mode_reboot_timeout_minutes(0),
                    zeroconf_timeout(5000),
                    webui_cookie_lifetime_days(90),
                    zeroconf_logging(false),
                    status_led_mode(cast_int_status_led_mode(StatusLEDModeType::SOLID_WHEN_CONNECTED)) {}

            } DeviceConfig_t;
        };

        class Device : public DeviceConfig, public ConfigGetterSetter<DeviceConfig::DeviceConfig_t, _H(MainConfig().system.device.cfg) CIF_DEBUG(, &handleNameDeviceConfig_t)> {
        public:
            Device() {
            }
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

            static constexpr uint16_t kZeroConfMinTimeout = 1000;
            static constexpr uint16_t kZeroConfMaxTimeout = 60000;

            static constexpr uint16_t kWebUICookieMinLifetime = 3;
            static constexpr uint16_t kWebUICookieMaxLifetime = 720;
        };

        // --------------------------------------------------------------------
        // Firmware

        class Firmware {
        public:
            static void defaults();

            // sha1 of the firmware.elf file
            static const uint8_t *getElfHash(uint16_t &length);     // length getElfHashSize byte
            static uint8_t getElfHashHex(String &str);
            static void setElfHash(const uint8_t *data);            // length getElfHashSize byte
            static void setElfHashHex(const char *data);            // getElfHashHexSize() + nul byte
            static constexpr uint8_t getElfHashSize() {
                return static_cast<uint8_t>(sizeof(elf_sha1));
            }
            static constexpr uint8_t getElfHashHexSize() {
                return static_cast<uint8_t>(sizeof(elf_sha1)) * 2;
            }

            CREATE_STRING_GETTER_SETTER(MainConfig().system.firmware, PluginBlacklist, 255);

            uint8_t elf_sha1[20];
        };

        class WebServerConfig {
        public:
            enum class ModeType : uint8_t {
                MIN = 0,
                DISABLED = 0,
                UNSECURE,
                SECURE,
                MAX
            };

            AUTO_DEFAULT_PORT_CONST_SECURE(0, 80, 443);

            typedef struct __attribute__packed__ WebServerConfig_t {
                uint8_t is_https: 1;
                uint8_t __reserved: 7;
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, is_https);

                WebServerConfig_t() : is_https(false), __port(kPortAuto) {}

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

    // --------------------------------------------------------------------
    // Network

    struct Network {

        // --------------------------------------------------------------------
        // Settings
        class SettingsConfig {
        public:
            typedef struct __attribute__packed__ SettingsConfig_t {
                using Type = SettingsConfig_t;
                CREATE_IPV4_ADDRESS(local_ip);
                CREATE_IPV4_ADDRESS(subnet);
                CREATE_IPV4_ADDRESS(gateway);
                CREATE_IPV4_ADDRESS(dns1);
                CREATE_IPV4_ADDRESS(dns2);

                IPAddress getLocalIp() const {
                    return local_ip;
                }
                IPAddress getSubnet() const {
                    return subnet;
                }
                IPAddress getGateway() const {
                    return gateway;
                }
                IPAddress getDns1() const {
                    return dns1;
                }
                IPAddress getDns2() const {
                    return dns2;
                }
                SettingsConfig_t();
            } SettingsConfig_t;
        };

        class Settings : public SettingsConfig, public ConfigGetterSetter<SettingsConfig::SettingsConfig_t, _H(MainConfig().network.settings.cfg) CIF_DEBUG(, &handleNameSettingsConfig_t)> {
        public:

            static void defaults();

        };

        // --------------------------------------------------------------------
        // SoftAP

        class SoftAPConfig {
        public:
            using EncryptionType = WiFiEncryptionType;

            typedef struct __attribute__packed__ SoftAPConfig_t {
                using Type = SoftAPConfig_t;
                CREATE_IPV4_ADDRESS(address);
                CREATE_IPV4_ADDRESS(subnet);
                CREATE_IPV4_ADDRESS(gateway);
                CREATE_IPV4_ADDRESS(dhcp_start);
                CREATE_IPV4_ADDRESS(dhcp_end);
                uint8_t channel;
                union __attribute__packed__ {
                    EncryptionType encryption_enum;
                    uint8_t encryption;
                };
                SoftAPConfig_t();

                IPAddress getAddress() const {
                    return address;
                }
                IPAddress getSubnet() const {
                    return subnet;
                }
                IPAddress getGateway() const {
                    return gateway;
                }
                IPAddress getDhcpStart() const {
                    return dhcp_start;
                }
                IPAddress getDhcpEnd() const {
                    return dhcp_end;
                }
                uint8_t getChannel() const {
                    return channel;
                }
                EncryptionType getEncryption() const {
                    return encryption_enum;
                }
            } SoftAPConfig_t;
        };

        class SoftAP : public SoftAPConfig, public ConfigGetterSetter<SoftAPConfig::SoftAPConfig_t, _H(MainConfig().network.softap.cfg) CIF_DEBUG(, &handleNameSoftAPConfig_t)> {
        public:

            static void defaults();

        };

        // --------------------------------------------------------------------
        // WiFi

        class WiFi {
        public:
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SSID, 1, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, Password, 8, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SoftApSSID, 1, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().network.wifi, SoftApPassword, 8, 32);
        };

        Settings settings;
        SoftAP softap;
        WiFi wifi;
    };

    // --------------------------------------------------------------------
    // Plugins

    struct Plugins {

        // --------------------------------------------------------------------
        // Home Assistant

        class HomeAssistant {
        public:
            typedef enum : uint8_t {
                NONE = 0,
                TURN_ON,
                TURN_OFF,
                TOGGLE,
                TRIGGER,
                SET_BRIGHTNESS,     // values: brightness(0)
                CHANGE_BRIGHTNESS,  // values: step(0), min brightness(1), max brightness(2), turn off if less than min. brightness(3)
                SET_KELVIN,         // values: kelvin(0)
                SET_MIREDS,         // values: mireds(0)
                SET_RGB_COLOR,      // values: red(0), green(1), blue(2)
                MEDIA_PREVIOUS_TRACK,
                MEDIA_NEXT_TRACK,
                MEDIA_STOP,
                MEDIA_PLAY,
                MEDIA_PAUSE,
                MEDIA_PLAY_PAUSE,
                VOLUME_UP,
                VOLUME_DOWN,
                VOLUME_SET,         // values: volume(0) in %
                VOLUME_MUTE,
                VOLUME_UNMUTE,
                MQTT_SET,
                MQTT_TOGGLE,
                MQTT_INCR,
                MQTT_DECR,
                __END,
            } ActionEnum_t;
            typedef struct __attribute__packed__ {
                uint16_t id: 16;
                ActionEnum_t action;
                uint8_t valuesLen;
                uint8_t entityLen;
                uint8_t apiId;
            } ActionHeader_t;

            class Action {
            public:
                typedef std::vector<int32_t> ValuesVector;

                Action() = default;
                Action(uint16_t id, uint8_t apiId, ActionEnum_t action, const ValuesVector &values, const String &entityId) : _id(id), _apiId(apiId), _action(action), _values(values), _entityId(entityId) {
                }
                bool operator==(ActionEnum_t id) const {
                    return _id == id;
                }
                bool operator==(uint16_t id) const {
                    return _id == id;
                }
                ActionEnum_t getAction() const {
                    return _action;
                }
                const __FlashStringHelper *getActionFStr() const {
                    return HomeAssistant::getActionStr(_action);
                }
                void setAction(ActionEnum_t action) {
                    _action = action;
                }
                uint16_t getId() const {
                    return _id;
                }
                void setId(uint16_t id) {
                    _id = id;
                }
                uint8_t getApiId() const {
                    return _apiId;
                }
                void setApiId(uint8_t apiId) {
                    _apiId = apiId;
                }
                int32_t getValue(uint8_t num) const {
                    if (num < _values.size()) {
                        return _values.at(num);
                    }
                    return 0;
                }
                void setValues(const ValuesVector &values) {
                    _values = values;
                }
                ValuesVector &getValues() {
                    return _values;
                }
                String getValuesStr() const {
                    return implode(',', _values);
                }
                uint8_t getNumValues() const {
                    return _values.size();
                }
                const String &getEntityId() const {
                    return _entityId;
                }
                void setEntityId(const String &entityId) {
                    _entityId = entityId;
                }
            private:
                uint16_t _id;
                uint8_t _apiId;
                ActionEnum_t _action;
                ValuesVector _values;
                String _entityId;
            };

            typedef std::vector<Action> ActionVector;

            HomeAssistant() {
            }

            static void defaults();
            static void setApiEndpoint(const String &endpoint, uint8_t apiId = 0);
            static void setApiToken(const String &token, uint8_t apiId = 0);
            static const char *getApiEndpoint(uint8_t apiId = 0);
            static const char *getApiToken(uint8_t apiId = 0);
            static void getActions(ActionVector &actions);
            static void setActions(ActionVector &actions);
            static Action getAction(uint16_t id);
            static const __FlashStringHelper *getActionStr(ActionEnum_t action);

            char api_endpoint[128];
            char token[250];
            char api_endpoint1[128];
            char token1[250];
            char api_endpoint2[128];
            char token2[250];
            char api_endpoint3[128];
            char token3[250];
            uint8_t *actions;
        };

        // --------------------------------------------------------------------
        // Remote

        #ifndef IOT_REMOTE_CONTROL_BUTTON_COUNT
        #define IOT_REMOTE_CONTROL_BUTTON_COUNT 4
        #endif

        class RemoteControl {
        public:
            typedef struct __attribute__packed__ {
                uint16_t shortpress;
                uint16_t longpress;
                uint16_t repeat;
            } ComboAction_t;

            typedef struct __attribute__packed__ {
                uint16_t shortpress;
                uint16_t longpress;
                uint16_t repeat;
                ComboAction_t combo[IOT_REMOTE_CONTROL_BUTTON_COUNT];
            } Action_t;

            struct __attribute__packed__ {
                uint8_t autoSleepTime: 8;
                uint16_t deepSleepTime: 16;       // ESP8266 max. ~14500 seconds, 0 = indefinitely
                uint16_t longpressTime;
                uint16_t repeatTime;
                Action_t actions[IOT_REMOTE_CONTROL_BUTTON_COUNT];
            };

            RemoteControl() {
                autoSleepTime = 2;
                deepSleepTime = 0;
                longpressTime = 750;
                repeatTime = 500;
                memset(&actions, 0, sizeof(actions));
            }

            void validate() {
                if (!longpressTime) {
                    *this = RemoteControl();
                }
                if (!autoSleepTime) {
                    autoSleepTime = 2;
                }
            }

            static void defaults();
            static RemoteControl get();
        };

        // --------------------------------------------------------------------
        // IOT Switch

        class IOTSwitch {
        public:
            typedef enum : uint8_t {
                OFF =       0x00,
                ON =        0x01,
                RESTORE =   0x02,
            } StateEnum_t;
            typedef enum : uint8_t {
                NONE =      0x00,
                HIDE =      0x01,
                NEW_ROW =   0x02,
            } WebUIEnum_t;
            typedef struct __attribute__packed__ {
                uint8_t length;
                StateEnum_t state;
                WebUIEnum_t webUI;
            } Switch_t;

            static const uint8_t *getConfig();
            static void setConfig(const uint8_t *buf, size_t size);

            // T = std::array<String, N>, R = std::array<Switch_t, N>
            template <class T, class R>
            static void getConfig(T &names, R &configs) {
                names = {};
                configs = {};
                uint16_t length = 0;
                auto ptr = getConfig();
                if (ptr) {
                    size_t i = 0;
                    auto endPtr = ptr + length;
                    while(ptr + sizeof(Switch_t) <= endPtr && i < names.size()) {
                        configs[i] = *reinterpret_cast<Switch_t *>(const_cast<uint8_t *>(ptr));
                        ptr += sizeof(Switch_t);
                        if (ptr + configs[i].length <= endPtr) {
                            names[i] = PrintString(ptr, configs[i].length);
                        }
                        ptr += configs[i++].length;
                    }
                }
            }

            template <class T, class R>
            static void setConfig(const T &names, R &configs) {
                Buffer buffer;
                for(size_t i = 0; i < names.size(); i++) {
                    configs[i].length = names[i].length();
                    buffer.writeObject(configs[i]);
                    buffer.write(names[i]);
                }
                setConfig(buffer.begin(), buffer.length());
            }
        };

        // --------------------------------------------------------------------
        // Weather Station

        class WeatherStation
        {
        public:
            class WeatherStationConfig {
            public:
                WeatherStationConfig();
                void validate();
                uint32_t getPollIntervalMillis();
                struct __attribute__packed__ {
                    uint16_t weather_poll_interval;
                    uint16_t api_timeout;
                    uint8_t backlight_level;
                    uint8_t touch_threshold;
                    uint8_t released_threshold;
                    uint8_t is_metric: 1;
                    uint8_t time_format_24h: 1;
                    uint8_t show_webui: 1;
                    float temp_offset;
                    float humidity_offset;
                    float pressure_offset;
                    uint8_t screenTimer[8];
                };
            };

            WeatherStation();

            static void defaults();
            static const char *getApiKey();
            static const char *getQueryString();
            static WeatherStationConfig &getWriteableConfig();
            static WeatherStationConfig getConfig();
            void setConfig(const WeatherStationConfig &params);

            char openweather_api_key[65];
            char openweather_api_query[65];
            WeatherStationConfig config;
        };


        // --------------------------------------------------------------------
        // Alarm

        #ifndef IOT_ALARM_PLUGIN_MAX_ALERTS
        #define IOT_ALARM_PLUGIN_MAX_ALERTS                         10
        #endif
        #ifndef IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION
        #define IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION               300
        #endif

        class AlarmConfig
        {
        public:
            static constexpr uint8_t MAX_ALARMS = IOT_ALARM_PLUGIN_MAX_ALERTS;
            static constexpr uint16_t DEFAULT_MAX_DURATION = IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION;
            static constexpr uint16_t STOP_ALARM = std::numeric_limits<uint16_t>::max() - 1;

            using TimeType = uint32_t;

            enum class AlarmModeType : uint8_t {
                BOTH,       // can be used if silent or buzzer is not available
                SILENT,
                BUZZER,
                MAX
            };

            enum class WeekDaysType : uint8_t {
                NONE,
                SUNDAY = _BV(0),
                MONDAY = _BV(1),
                TUESDAY = _BV(2),
                WEDNESDAY = _BV(3),
                THURSDAY = _BV(4),
                FRIDAY = _BV(5),
                SATURDAY = _BV(6),
                WEEK_DAYS = _BV(1)|_BV(2)|_BV(3)|_BV(4)|_BV(5),
                WEEK_END = _BV(0)|_BV(6),
            };

            typedef union __attribute__packed__ {
                WeekDaysType week_days_enum;
                uint8_t week_days;
                struct {
                    uint8_t sunday: 1;              // 0
                    uint8_t monday: 1;              // 1
                    uint8_t tuesday: 1;             // 2
                    uint8_t wednesday: 1;           // 3
                    uint8_t thursday: 1;            // 4
                    uint8_t friday: 1;              // 5
                    uint8_t saturday: 1;            // 6
                };
            } WeekDay_t;

            typedef struct __attribute__packed__ {
                TimeType timestamp;
                uint8_t hour;
                uint8_t minute;
                WeekDay_t week_day;
            } AlarmTime_t;

            typedef struct __attribute__packed__ SingleAlarm_t {
                using Type = SingleAlarm_t;
                AlarmTime_t time;
                uint16_t max_duration;       // limit in seconds, 0 = unlimited
                CREATE_ENUM_BITFIELD(mode, AlarmModeType);
                CREATE_UINT8_BITFIELD(is_enabled, 1);
            } SingleAlarm_t;

            typedef struct __attribute__packed__ {
                SingleAlarm_t alarms[MAX_ALARMS];
            } Alarm_t;
        };

        class Alarm : public AlarmConfig, public ConfigGetterSetter<AlarmConfig::Alarm_t, _H(MainConfig().plugins.alarm.cfg) CIF_DEBUG(, &handleNameAlarm_t)> {
        public:

            Alarm();

            // update timestamp for a single alarm
            // set now to the current time plus a safety margin to let the system install the alarm
            // auto now = time(nullptr) + 30;
            // auto tm = localtime(&now);
            // - if the alarm is disabled, timestamp is set to 0
            // - if any weekday is selected, the timestamp is set to 0
            // - if none of the weekdays are selected, the timestamp is set to unixtime at hour:minute of today. if hour:minute has
            // passed already, the alarm is set for tomorrow (+1 day) at hour:minute
            static void updateTimestamp(const struct tm *tm, SingleAlarm_t &alarm);

            // calls updateTimestamp() for each entry
            static void updateTimestamps(const struct tm *tm, Alarm_t &cfg);

            // return time of the next alarm
            static TimeType getTime(const struct tm *tm, SingleAlarm_t &alarm);

            static void getWeekDays(Print &output, uint8_t weekdays, char none = 'x');
            static String getWeekDaysString(uint8_t weekdays, char none = 'x');

            static void defaults();
            static void dump(Print &output, Alarm_t &cfg);

            Alarm_t cfg;
        };

        // --------------------------------------------------------------------
        // Serial2TCP

        class Serial2TCPConfig {
        public:
            enum class ModeType : uint8_t {
                NONE,
                SERVER,
                CLIENT,
                MAX
            };
            enum class SerialPortType : uint8_t {
                SERIAL0,
                SERIAL1,
                SOFTWARE,
                MAX
            };

            typedef struct __attribute__packed__ Serial2Tcp_t {
                using Type = Serial2Tcp_t;
                uint16_t port;
                uint32_t baudrate;
                CREATE_ENUM_BITFIELD(mode, ModeType);
                CREATE_ENUM_BITFIELD(serial_port, SerialPortType);
                CREATE_UINT8_BITFIELD(authentication, 1);
                CREATE_UINT8_BITFIELD(auto_connect, 1);
                uint8_t rx_pin;
                uint8_t tx_pin;
                uint8_t keep_alive;
                uint8_t auto_reconnect;
                uint16_t idle_timeout;

                Serial2Tcp_t() : port(2323), baudrate(KFC_SERIAL_RATE), mode(cast_int_mode(ModeType::SERVER)), serial_port(cast_int_serial_port(SerialPortType::SERIAL0)), authentication(false), auto_connect(false), rx_pin(0), tx_pin(0), keep_alive(30), auto_reconnect(5), idle_timeout(300) {}

            } Serial2Tcp_t;
        };

        class Serial2TCP : public Serial2TCPConfig, ConfigGetterSetter<Serial2TCPConfig::Serial2Tcp_t, _H(MainConfig().plugins.serial2tcp.cfg) CIF_DEBUG(, &handleNameSerial2TCPConfig_t)> {
        public:

            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.serial2tcp, Hostname, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.serial2tcp, Username, 3, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.serial2tcp, Password, 6, 32);
        };

        // --------------------------------------------------------------------
        // MQTTClient

        class MQTTClientConfig {
        public:
            enum class ModeType : uint8_t {
                MIN = 0,
                DISABLED = MIN,
                UNSECURE,
                SECURE,
                MAX
            };

            enum class QosType : uint8_t {
                MIN = 0,
                AT_MODE_ONCE = 0,
                AT_LEAST_ONCE = 1,
                EXACTLY_ONCE = 2,
                MAX,
                DEFAULT = 0xff,
            };

            AUTO_DEFAULT_PORT_CONST_SECURE(0, 1883, 8883);

            typedef struct __attribute__packed__ MqttConfig_t {
                using Type = MqttConfig_t;
                CREATE_ENUM_BITFIELD(mode, ModeType);
                CREATE_ENUM_BITFIELD(qos, QosType);
                CREATE_UINT8_BITFIELD(auto_discovery, 1);
                CREATE_UINT8_BITFIELD(enable_shared_topic, 1);
                uint8_t keepalive;
                uint16_t auto_discovery_rebroadcast_interval; // minutes
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, get_enum_mode(*this) == ModeType::SECURE);

                static constexpr uint8_t kKeepAliveDefault = 15;
                static constexpr uint16_t kAutoDiscoveryRebroadcastDefault = 24 * 60;

                MqttConfig_t() : mode(cast_int_mode(ModeType::UNSECURE)), qos(cast_int_qos(QosType::EXACTLY_ONCE)), auto_discovery(true), enable_shared_topic(true), keepalive(kKeepAliveDefault), auto_discovery_rebroadcast_interval(kAutoDiscoveryRebroadcastDefault), __port(kPortAuto) {}

            } MqttConfig_t;
        };

        class MQTTClient : public MQTTClientConfig, public ConfigGetterSetter<MQTTClientConfig::MqttConfig_t, _H(MainConfig().plugins.mqtt.cfg) CIF_DEBUG(, &handleNameMqttConfig_t)> {
        public:
            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Hostname, 1, 128);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Username, 0, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Password, 6, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Topic, 4, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, AutoDiscoveryPrefix, 1, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, SharedTopic, 4, 128);

            static const uint8_t *getFingerPrint(uint16_t &size);
            static void setFingerPrint(const uint8_t *fingerprint, uint16_t size);
            static constexpr size_t kFingerprintMaxSize = 20;
        };

        // --------------------------------------------------------------------
        // Syslog

        class SyslogClientConfig {
        public:
            using SyslogProtocolType = ::SyslogProtocolType;

            AUTO_DEFAULT_PORT_CONST_SECURE(0, 514, 6514);

            typedef struct __attribute__packed__ SyslogConfig_t {
                union __attribute__packed__ {
                    SyslogProtocolType protocol_enum;
                    uint8_t protocol;
                };
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, protocol_enum == SyslogProtocolType::TCP_TLS);

                template<class T>
                void dump() {
                    CONFIG_DUMP_STRUCT_INFO(T);
                    CONFIG_DUMP_STRUCT_VAR(protocol);
                    CONFIG_DUMP_STRUCT_VAR(__port);
                }

                SyslogConfig_t() : protocol_enum(SyslogProtocolType::TCP), __port(kPortDefault) {}

            } SyslogConfig_t;
        };

        class SyslogClient : public SyslogClientConfig, public ConfigGetterSetter<SyslogClientConfig::SyslogConfig_t, _H(MainConfig().plugins.syslog.cfg) CIF_DEBUG(, &handleNameSyslogConfig_t)>
        {
        public:
            static void defaults();
            static bool isEnabled();
            static bool isEnabled(SyslogProtocolType protocol);

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.syslog, Hostname, 1, 128);
        };

        // --------------------------------------------------------------------
        // NTP CLient

        class NTPClientConfig {
        public:
            typedef struct __attribute__packed__ NtpClientConfig_t {
                // minutes
                uint16_t refreshInterval;
                uint32_t getRefreshIntervalMillis() const {
                    return refreshInterval * 60U * 1000U;
                }
                static constexpr uint16_t kRefreshIntervalMin = 5;
                static constexpr uint16_t kRefreshIntervalMax = 720 * 60;
                static constexpr uint16_t kRefreshIntervalDefault = 15;

                NtpClientConfig_t() : refreshInterval(kRefreshIntervalDefault) {}

            } NtpClientConfig_t;
        };

        class NTPClient : public NTPClientConfig, public ConfigGetterSetter<NTPClientConfig::NtpClientConfig_t, _H(MainConfig().plugins.ntpclient.cfg) CIF_DEBUG(, &handleNameNtpClientConfig_t)>
        {
        public:
            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server1, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server2, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ntpclient, Server3, 0, 64);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.ntpclient, TimezoneName, 64);
            CREATE_STRING_GETTER_SETTER(MainConfig().plugins.ntpclient, PosixTimezone, 64);

            static const char *getServer(uint8_t num);
            static constexpr uint8_t kServersMax = 3;

        };

        // --------------------------------------------------------------------
        // Sensor

        class SensorConfig {
        public:
            typedef struct __attribute__packed__ SensorConfig_t {
#if IOT_SENSOR_HAVE_BATTERY
                typedef struct __attribute__packed__ battery_t {
                    float calibration;
                    uint8_t precision;
                    float offset;

                    battery_t() : calibration(1), precision(1), offset(0) {}

                } battery_t;
                battery_t battery;
#endif
#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
                typedef struct __attribute__packed__ hlw80xx_t {
                    float calibrationU;
                    float calibrationI;
                    float calibrationP;
                    uint64_t energyCounter;
                    uint8_t extraDigits;

                    hlw80xx_t();

                } hlw80xx_t;
                hlw80xx_t hlw80xx;
#endif
                SensorConfig_t() = default;
            } SensorConfig_t;
        };

        class Sensor : public SensorConfig, public ConfigGetterSetter<SensorConfig::SensorConfig_t, _H(MainConfig().plugins.sensor.cfg) CIF_DEBUG(, &handleNameSensorConfig_t)> {
        public:
            static void defaults();
        };

        // --------------------------------------------------------------------
        // BlindsController

        class BlindsConfig {
        public:
            enum class OperationType : uint8_t {
                NONE = 0,
                OPEN_CHANNEL0,                    // _FOR_CHANNEL0_AND_ALL
                OPEN_CHANNEL0_FOR_CHANNEL1,       // _ONLY
                OPEN_CHANNEL1,
                OPEN_CHANNEL1_FOR_CHANNEL0,
                CLOSE_CHANNEL0,
                CLOSE_CHANNEL0_FOR_CHANNEL1,
                CLOSE_CHANNEL1,
                CLOSE_CHANNEL1_FOR_CHANNEL0,
                MAX
            };

            enum class MultiplexerType : uint8_t {
                LOW_FOR_CHANNEL0 = 0,
                HIGH_FOR_CHANNEL0 = 1,
                MAX
            };

            typedef struct __attribute__packed__ BlindsConfigOperation_t {
                using Type = BlindsConfigOperation_t;

                uint16_t delay;                                     // delay before execution in seconds
                CREATE_ENUM_BITFIELD(type, OperationType);          // action

                BlindsConfigOperation_t() : delay(0), type(0) {}

                template<typename Archive>
                void serialize(Archive & ar, kfc::serialization::version version){
                    ar & KFC_SERIALIZATION_NVP(type);
                    ar & KFC_SERIALIZATION_NVP(delay);
                }

            } BlindsConfigOperation_t;

            typedef struct __attribute__packed__ BlindsConfigChannel_t {
                uint16_t pwm_value;                 // 0-1023
                uint16_t current_limit;             // mA
                uint16_t current_limit_time;        // ms
                uint16_t open_time;                 // ms
                uint16_t close_time;                // ms

                BlindsConfigChannel_t() : pwm_value(600), current_limit(1500), current_limit_time(50), open_time(5000), close_time(5500) {}

                template<typename Archive>
                void serialize(Archive & ar, kfc::serialization::version version){
                    ar & KFC_SERIALIZATION_NVP(pwm_value);
                    ar & KFC_SERIALIZATION_NVP(current_limit);
                    ar & KFC_SERIALIZATION_NVP(current_limit_time);
                    ar & KFC_SERIALIZATION_NVP(open_time);
                    ar & KFC_SERIALIZATION_NVP(close_time);
                }

            } BlindsConfigChannel_t;

            typedef struct __attribute__packed__ BlindsConfig_t {
                using Type = BlindsConfig_t;

                BlindsConfigChannel_t channels[2];
                BlindsConfigOperation_t open[4];
                BlindsConfigOperation_t close[4];
                uint8_t pins[5];
                CREATE_ENUM_BITFIELD(multiplexer, MultiplexerType);

                template<typename Archive>
                void serialize(Archive & ar, kfc::serialization::version version) {
                    ar & KFC_SERIALIZATION_NVP(channels);
                    ar & KFC_SERIALIZATION_NVP(open);
                    ar & KFC_SERIALIZATION_NVP(close);
                    ar & KFC_SERIALIZATION_NVP(pins);
                    ar & KFC_SERIALIZATION_NVP(multiplexer);
                }

                BlindsConfig_t();

            } SensorConfig_t;
        };

        class Blinds : public BlindsConfig, public ConfigGetterSetter<BlindsConfig::BlindsConfig_t, _H(MainConfig().plugins.blinds.cfg) CIF_DEBUG(, &handleNameBlindsConfig_t)> {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.blinds, Channel0Name, 2, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.blinds, Channel1Name, 2, 64);

            static const char *getChannelName(size_t num) {
                switch(num) {
                    case 0:
                        return getChannel0Name();
                    case 1:
                    default:
                        return getChannel1Name();
                }
            }

            template<typename Archive>
            void load(Archive & ar, kfc::serialization::version version) const {
                ar & getConfig();
                ar & getChannel0Name();
                ar & getChannel1Name();
            }

            template<typename Archive>
            void save(Archive & ar, kfc::serialization::version version) {
                ar & getWriteableConfig();
                ar & getWriteableChannel0Name();
                ar & getWriteableChannel1Name();
            }
        };

        // --------------------------------------------------------------------
        // MDNS

        class MDNS
        {
        public:

        };


        // --------------------------------------------------------------------
        // Plugin Structure

        HomeAssistant homeassistant;
        RemoteControl remotecontrol;
        IOTSwitch iotswitch;
        WeatherStation weatherstation;
        Alarm alarm;
        Serial2TCP serial2tcp;
        MQTTClient mqtt;
        SyslogClient syslog;
        NTPClient ntpclient;
        Sensor sensor;
        Blinds blinds;
        MDNS mdns;

    };

    // --------------------------------------------------------------------
    // Main Config Structure

    struct MainConfig {

        System system;
        Network network;
        Plugins plugins;
    };

};

static constexpr size_t ConfigFlagsSize = sizeof(KFCConfigurationClasses::System::Flags::ConfigFlags_t);
static_assert(ConfigFlagsSize == sizeof(uint32_t), "size exceeded");

#include <pop_pack.h>

// #ifdef _H_DEFINED_KFCCONFIGURATIONCLASSES
// #undef CONFIG_GET_HANDLE_STR
// #undef _H
// #undef _H_DEFINED_KFCCONFIGURATIONCLASSES
// #endif
