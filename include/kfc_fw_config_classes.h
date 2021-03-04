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
#include <boost/preprocessor/tuple/rem.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/variadic/elem.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/comparison/equal.hpp>
#include <boost/preprocessor/punctuation/remove_parens.hpp>
#include <push_pack.h>

#include <kfc_fw_config_types.h>

#if IOT_DIMMER_MODULE || DEBUG_4CH_DIMMER
#include "../src/plugins/dimmer_module/firmware_protocol.h"
#endif

#include <Form/Types.h>

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
    static inline size_t get##name##Size() { return k##name##MaxSize; } \
    static inline const uint8_t *get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return loadBinaryConfig(k##name##ConfigHandle, k##name##MaxSize); } \
    static inline void set##name(const uint8_t *data) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeBinaryConfig(k##name##ConfigHandle, data, k##name##MaxSize); }

#define CREATE_STRING_GETTER_SETTER(class_name, name, len) \
    static constexpr size_t k##name##MaxSize = len; \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static inline const char *get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return loadStringConfig(k##name##ConfigHandle); } \
    static inline char *getWriteable##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return loadWriteableStringConfig(k##name##ConfigHandle, k##name##MaxSize); } \
    static inline void set##name(const char *str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeStringConfig(k##name##ConfigHandle, str); } \
    static inline void set##name(const __FlashStringHelper *str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeStringConfig(k##name##ConfigHandle, str); } \
    static inline void set##name(const String &str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeStringConfig(k##name##ConfigHandle, str); }

#define CREATE_STRING_GETTER_SETTER_MIN_MAX(class_name, name, mins, maxs) \
    static inline FormUI::Validator::Length &add##name##LengthValidator(FormUI::Form::BaseForm &form, bool allowEmpty = false) { \
        return form.addValidator(FormUI::Validator::Length(k##name##MinSize, k##name##MaxSize, allowEmpty)); \
    } \
    static inline FormUI::Validator::Length &add##name##LengthValidator(const String &message, FormUI::Form::BaseForm &form, bool allowEmpty = false) { \
        return form.addValidator(FormUI::Validator::Length(message, k##name##MinSize, k##name##MaxSize, allowEmpty)); \
    } \
    static constexpr size_t k##name##MinSize = mins; \
    CREATE_STRING_GETTER_SETTER(class_name, name, maxs)

//name, size, type, min_value, max_value, default_value[, step_size]
#define CREATE_BITFIELD_TYPE_MIN_MAX(name, size, type, min_value, max_value, ...) \
    static inline FormUI::Validator::Range &addRangeValidatorFor_##name(FormUI::Form::BaseForm &form, bool allowZero = false) { \
        form.getLastField().getFormUI()->addItems(FormUI::PlaceHolder(kDefaultValueFor_##name), FormUI::MinMax((int32_t)kMinValueFor_##name, (int32_t)kMaxValueFor_##name), \
            BOOST_PP_REMOVE_PARENS(BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),1),(FormUI::Type::NUMBER_RANGE),(FormUI::Attribute(F("step"),BOOST_PP_VARIADIC_ELEM(1,##__VA_ARGS__)),FormUI::Type::NUMBER_RANGE)))); \
        return form.addValidator(FormUI::Validator::Range((long)kMinValueFor_##name, (long)kMaxValueFor_##name, allowZero)); \
    } \
    static inline FormUI::Validator::Range &addRangeValidatorFor_##name(const String &message, FormUI::Form::BaseForm &form, bool allowZero = false) { \
        form.getLastField().getFormUI()->addItems(FormUI::PlaceHolder(kDefaultValueFor_##name), FormUI::MinMax((int32_t)kMinValueFor_##name, (int32_t)kMaxValueFor_##name), \
            BOOST_PP_REMOVE_PARENS(BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),1),(FormUI::Type::NUMBER_RANGE),(FormUI::Attribute(F("step"),BOOST_PP_VARIADIC_ELEM(1,##__VA_ARGS__)),FormUI::Type::NUMBER_RANGE)))); \
        return form.addValidator(FormUI::Validator::Range(message, (long)kMinValueFor_##name, (long)kMaxValueFor_##name, allowZero)); \
    } \
    static constexpr type kMinValueFor_##name = min_value; \
    static constexpr type kMaxValueFor_##name = max_value; \
    static constexpr type kDefaultValueFor_##name = BOOST_PP_VARIADIC_ELEM(0,##__VA_ARGS__); \
    static constexpr type kTypeMinValueFor_##name = std::is_signed<type>::value ? -(1U << (size - 1)) + 1 : 0; \
    static constexpr type kTypeMaxValueFor_##name = std::is_signed<type>::value ? (1U << (size - 1)) - 1 : (1U << size) - 1; \
    static_assert(min_value >= kTypeMinValueFor_##name && min_value <= kTypeMaxValueFor_##name, "min_value value out of range (" _STRINGIFY(size) " bits)"); \
    static_assert(max_value >= kTypeMinValueFor_##name && max_value <= kTypeMaxValueFor_##name, "max_value value out of range (" _STRINGIFY(size) " bits)"); \
    static_assert(kDefaultValueFor_##name == 0 || (kDefaultValueFor_##name >= kTypeMinValueFor_##name && kDefaultValueFor_##name <= kTypeMaxValueFor_##name), "default_value value out of range"); \
    static_assert(min_value >= std::numeric_limits<long>::min() && min_value <= std::numeric_limits<long>::max(), "min_value value out of range (type long)"); \
    static_assert(max_value >= std::numeric_limits<long>::min() && max_value <= std::numeric_limits<long>::max(), "max_value value out of range (type long)"); \
    CREATE_BITFIELD_TYPE(name, size, type, bits)

#define CREATE_BOOL_BITFIELD_MIN_MAX(name, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, 1, bool, min_value, max_value, ##__VA_ARGS__)

#define CREATE_UINT8_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, uint8_t, min_value, max_value, ##__VA_ARGS__)

#define CREATE_INT16_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, int16_t, min_value, max_value, ##__VA_ARGS__)

#define CREATE_UINT16_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, uint16_t, min_value, max_value, ##__VA_ARGS__)

#define CREATE_INT32_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, int32_t, min_value, max_value, ##__VA_ARGS__)

#define CREATE_UINT32_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, uint32_t, min_value, max_value, ##__VA_ARGS__)

#define CREATE_UINT64_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, uint64_t, min_value, max_value, ##__VA_ARGS__)

#define CREATE_GETTER_SETTER_IP(class_name, name) \
    static constexpr HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const IPAddress get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return IPAddress(*(uint32_t *)loadBinaryConfig(k##name##ConfigHandle, sizeof(uint32_t))); } \
    static void set##name(const IPAddress &address) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeBinaryConfig(k##name##ConfigHandle, &static_cast<uint32_t>(address), sizeof(uint32_t)); }

#define CREATE_IPV4_ADDRESS(name) \
    uint32_t name; \
    static inline void set_ipv4_##name(Type &obj, const IPAddress &addr) { \
        obj.name = static_cast<uint32_t>(addr); \
    } \
    static inline IPAddress get_ipv4_##name(const Type &obj) { \
        return obj.name; \
    }

#define AUTO_DEFAULT_PORT_CONST(default_port) \
    static constexpr uint16_t kPortDefault = default_port; \

#define AUTO_DEFAULT_PORT_CONST_SECURE(unsecure, secure) \
    static constexpr uint16_t kPortDefault = unsecure; \
    static constexpr uint16_t kPortDefaultSecure = secure;

#define AUTO_DEFAULT_PORT_GETTER_SETTER(port_name, unsecure_port) \
    static constexpr uint16_t kDefaultValueFor_##port_name = 0; \
    uint16_t port_name; \
    uint16_t getPort() const { \
        return unsecure_port; \
    } \
    String getPortAsString() const { \
        return port_name == unsecure_port || port_name == 0 ? String() : String(port_name); \
    } \
    void setPort(int port) { \
        port_name = static_cast<uint16_t>(port); \
        if (port_name == 0) { \
            port_name = unsecure_port; \
        } \
    }

#define AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(port_name, unsecure_port, secure_port, is_secure) \
    static constexpr uint16_t kDefaultValueFor_##port_name = 0; \
    uint16_t port_name; \
    bool isSecure() const { \
        return is_secure; \
    } \
    bool isPortAuto() const { \
        return (port_name == 0 || port_name == secure_port || port_name == unsecure_port); \
    } \
    uint16_t getPort() const { \
        return isPortAuto() ? (isSecure() ? secure_port : unsecure_port) : port_name; \
    } \
    String getPortAsString() const { \
        return isPortAuto() ? String() : String(port_name); \
    } \
    void setPort(int port) { \
        port_name = static_cast<uint16_t>(port); \
        if (isPortAuto()) { \
            port_name =  0; \
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
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameDimmerConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameClockConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNamePingConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameWeatherStationConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameRemoteConfig_t);


#define CIF_DEBUG(...) __VA_ARGS__

#else

#define CIF_DEBUG(...)

#endif

namespace KFCConfigurationClasses {

    using HandleType = uint16_t;

    FormUI::Container::List createFormPinList(uint8_t from = 0, uint8_t to = 0xff);
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
                // invalid data, just initialize default object
                return ConfigType();
            }
            // return copy
            return *reinterpret_cast<const ConfigType *>(ptr);
        }

        static void setConfig(const ConfigType &params)
        {
            __CDBG_printf("setConfig=%04x size=%u name=%s", kConfigStructHandle, sizeof(ConfigType), *handleName);
            REGISTER_HANDLE_NAME(*handleName, __DBG__TYPE_SET);
            storeBinaryConfig(kConfigStructHandle, &params, sizeof(ConfigType));
        }

        static ConfigType & getWriteableConfig()
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

#if 0
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
                    CONFIG_DUMP_STRUCT_VAR(__reserved);
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
                    CONFIG_DUMP_STRUCT_VAR(is_netbios_enabled);
                    CONFIG_DUMP_STRUCT_VAR(is_log_login_failures_enabled);
                }
#endif

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

// #if HOME_ASSISTANT_INTEGRATION

//         // --------------------------------------------------------------------
//         // Home Assistant

//         class HomeAssistant {
//         public:
//             typedef enum : uint8_t {
//                 NONE = 0,
//                 TURN_ON,
//                 TURN_OFF,
//                 TOGGLE,
//                 TRIGGER,
//                 SET_BRIGHTNESS,     // values: brightness(0)
//                 CHANGE_BRIGHTNESS,  // values: step(0), min brightness(1), max brightness(2), turn off if less than min. brightness(3)
//                 SET_KELVIN,         // values: kelvin(0)
//                 SET_MIREDS,         // values: mireds(0)
//                 SET_RGB_COLOR,      // values: red(0), green(1), blue(2)
//                 MEDIA_PREVIOUS_TRACK,
//                 MEDIA_NEXT_TRACK,
//                 MEDIA_STOP,
//                 MEDIA_PLAY,
//                 MEDIA_PAUSE,
//                 MEDIA_PLAY_PAUSE,
//                 VOLUME_UP,
//                 VOLUME_DOWN,
//                 VOLUME_SET,         // values: volume(0) in %
//                 VOLUME_MUTE,
//                 VOLUME_UNMUTE,
//                 MQTT_SET,
//                 MQTT_TOGGLE,
//                 MQTT_INCR,
//                 MQTT_DECR,
//                 __END,
//             } ActionEnum_t;
//             typedef struct __attribute__packed__ {
//                 uint16_t id: 16;
//                 ActionEnum_t action;
//                 uint8_t valuesLen;
//                 uint8_t entityLen;
//                 uint8_t apiId;
//             } ActionHeader_t;

//             class Action {
//             public:
//                 typedef std::vector<int32_t> ValuesVector;

//                 Action() = default;
//                 Action(uint16_t id, uint8_t apiId, ActionEnum_t action, const ValuesVector &values, const String &entityId) : _id(id), _apiId(apiId), _action(action), _values(values), _entityId(entityId) {
//                 }
//                 bool operator==(ActionEnum_t id) const {
//                     return _id == id;
//                 }
//                 bool operator==(uint16_t id) const {
//                     return _id == id;
//                 }
//                 ActionEnum_t getAction() const {
//                     return _action;
//                 }
//                 const __FlashStringHelper *getActionFStr() const {
//                     return HomeAssistant::getActionStr(_action);
//                 }
//                 void setAction(ActionEnum_t action) {
//                     _action = action;
//                 }
//                 uint16_t getId() const {
//                     return _id;
//                 }
//                 void setId(uint16_t id) {
//                     _id = id;
//                 }
//                 uint8_t getApiId() const {
//                     return _apiId;
//                 }
//                 void setApiId(uint8_t apiId) {
//                     _apiId = apiId;
//                 }
//                 int32_t getValue(uint8_t num) const {
//                     if (num < _values.size()) {
//                         return _values.at(num);
//                     }
//                     return 0;
//                 }
//                 void setValues(const ValuesVector &values) {
//                     _values = values;
//                 }
//                 ValuesVector &getValues() {
//                     return _values;
//                 }
//                 String getValuesStr() const {
//                     return implode(',', _values);
//                 }
//                 uint8_t getNumValues() const {
//                     return _values.size();
//                 }
//                 const String &getEntityId() const {
//                     return _entityId;
//                 }
//                 void setEntityId(const String &entityId) {
//                     _entityId = entityId;
//                 }
//             private:
//                 uint16_t _id;
//                 uint8_t _apiId;
//                 ActionEnum_t _action;
//                 ValuesVector _values;
//                 String _entityId;
//             };

//             typedef std::vector<Action> ActionVector;

//             HomeAssistant() {
//             }

//             static void defaults();
//             static void setApiEndpoint(const String &endpoint, uint8_t apiId = 0);
//             static void setApiToken(const String &token, uint8_t apiId = 0);
//             static const char *getApiEndpoint(uint8_t apiId = 0);
//             static const char *getApiToken(uint8_t apiId = 0);
//             static void getActions(ActionVector &actions);
//             static void setActions(ActionVector &actions);
//             static Action getAction(uint16_t id);
//             static const __FlashStringHelper *getActionStr(ActionEnum_t action);

//             char api_endpoint[128];
//             char token[250];
//             char api_endpoint1[128];
//             char token1[250];
//             char api_endpoint2[128];
//             char token2[250];
//             char api_endpoint3[128];
//             char token3[250];
//             uint8_t *actions;
//         };

// #endif

        // --------------------------------------------------------------------
        // Remote

        #ifndef IOT_REMOTE_CONTROL_BUTTON_COUNT
        #define IOT_REMOTE_CONTROL_BUTTON_COUNT 4
        #endif

        class RemoteControlConfig {
        public:

            enum class ActionProtocolType : uint8_t {
                NONE = 0,
                MQTT,
                REST,
                TCP,
                UDP,
                MAX
            };

            using ActionIdType = uint16_t;

            #define MAX_ACTION_BITS 4
            #define MAX_ACTION_VAL ((1 << MAX_ACTION_BITS) - 1)

            enum class EventNameType : uint16_t {
                BUTTON_DOWN = 0,
                BUTTON_UP,
                BUTTON_PRESS,
                BUTTON_LONG_PRESS,
                BUTTON_SINGLE_CLICK,
                BUTTON_DOUBLE_CLICK,
                BUTTON_MULTI_CLICK,
                BUTTON_HOLD_REPEAT,
                BUTTON_HOLD_RELEASE,
                MAX,
                BV_NONE = 0,
                BV_BUTTON_DOWN = _BV(BUTTON_DOWN),
                BV_BUTTON_UP = _BV(BUTTON_UP),
                BV_BUTTON_PRESS = _BV(BUTTON_PRESS),
                BV_BUTTON_LONG_PRESS = _BV(BUTTON_LONG_PRESS),
                BV_BUTTON_SINGLE_CLICK = _BV(BUTTON_SINGLE_CLICK),
                BV_BUTTON_DOUBLE_CLICK = _BV(BUTTON_DOUBLE_CLICK),
                BV_BUTTON_MULTI_CLICK = _BV(BUTTON_MULTI_CLICK),
                BV_BUTTON_HOLD_REPEAT = _BV(BUTTON_HOLD_REPEAT),
                BV_BUTTON_HOLD_RELEASE = _BV(BUTTON_HOLD_RELEASE),
                _BV_DEFAULT = BV_BUTTON_PRESS|BV_BUTTON_LONG_PRESS|BV_BUTTON_HOLD_REPEAT,
                ANY = 0xffff,
            };

            struct __attribute__packed__ Events {
                using Type = Events;

                static constexpr size_t kBitCount = 9;

                union __attribute__packed__ {
                    uint16_t event_bits: kBitCount;
                    struct __attribute__packed__ {
                        uint16_t event_down: 1;
                        uint16_t event_up: 1;
                        uint16_t event_press: 1;
                        uint16_t event_long_press: 1;
                        uint16_t event_single_click: 1;
                        uint16_t event_double_click: 1;
                        uint16_t event_multi_click: 1;
                        uint16_t event_hold: 1;
                        uint16_t event_hold_released: 1;
                    };
                };

                Events(EventNameType bits = EventNameType::_BV_DEFAULT) : event_bits(static_cast<uint16_t>(bits)) {}
                Events(uint16_t bits) : event_bits(bits) {}

                bool operator[](int index) const {
                    return event_bits & _BV(index);
                }

                bool operator[](EventNameType index) const {
                    return event_bits & _BV(static_cast<uint8_t>(index));
                }

                CREATE_GETTER_SETTER_TYPE(event_down, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_up, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_press, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_single_click, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_double_click, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_multi_click, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_long_press, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_hold, 1, uint16_t, bits);
                CREATE_GETTER_SETTER_TYPE(event_hold_released, 1, uint16_t, bits);
            };

            typedef struct __attribute__packed__ ComboAction_t {
                using Type = ComboAction_t;
                CREATE_UINT16_BITFIELD_MIN_MAX(time, 13, 0, 8000, 1000, 500);
                CREATE_UINT16_BITFIELD_MIN_MAX(btn0, 2, 0, 3, 0, 1);
                CREATE_UINT16_BITFIELD_MIN_MAX(btn1, 2, 0, 3, 0, 1);
            } ComboAction_t;

            typedef struct __attribute__packed__ MultiClick_t {
                using Type = MultiClick_t;
                CREATE_UINT16_BITFIELD_MIN_MAX(action, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(clicks, 4, 3, 15, 0);
            } MultiClick_t;

            typedef struct __attribute__packed__ Action_t {
                using Type = Action_t;
                CREATE_UINT16_BITFIELD_MIN_MAX(down, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(up, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(press, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(long_press, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(single_click, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(double_click, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(multi_click, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(hold, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);
                CREATE_UINT16_BITFIELD_MIN_MAX(hold_released, MAX_ACTION_BITS, 0, MAX_ACTION_VAL, 0);

                Events udp;
                Events mqtt;
                // MultiClick_t multi_click[4];

                bool hasAction() const {
                    return down || up || press || single_click || double_click || multi_click || long_press || hold || hold_released;
                }

                bool hasUdpAction() const {
                    return udp.event_bits != 0;
                }

                bool hasMQTTAction() const {
                    return mqtt.event_bits != 0;
                }

                #undef MAX_ACTION_BITS
                #undef MAX_ACTION_VAL

            } Action_t;

            // struct __attribute__packed__ Button_t {
            //     using Type = Button_t;

            //     CREATE_UINT32_BITFIELD_MIN_MAX(multi_click_enabled, 1, 0, 1, 0, 1);
            //     CREATE_UINT32_BITFIELD_MIN_MAX(udp_enabled, 1, 0, 1, 0, 1);
            //     CREATE_UINT32_BITFIELD_MIN_MAX(press_time, 11, 0, 2000, 350, 50);           // millis
            //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_time, 13, 0, 8000, 1000, 50);           // millis
            //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_repeat_time, 12, 0, 4000, 200, 50);     // millis
            //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_max_value, 16, 0, 65535, 255, 1);
            //     CREATE_UINT32_BITFIELD_MIN_MAX(hold_step, 16, 0, 65535, 1, 1);
            //     CREATE_UINT32_BITFIELD_MIN_MAX(action_id, 16, 0, 65535, 0, 1);

            //     Button_t() :
            //         multi_click_enabled(kDefaultValueFor_multi_click_enabled),
            //         udp_enabled(kDefaultValueFor_udp_enabled),
            //         press_time(kDefaultValueFor_press_time),
            //         hold_time(kDefaultValueFor_hold_time),
            //         hold_repeat_time(kDefaultValueFor_hold_repeat_time),
            //         hold_max_value(kDefaultValueFor_hold_max_value),
            //         hold_step(kDefaultValueFor_hold_step),
            //         action_id(kDefaultValueFor_action_id)
            //     {}

            // };

            // struct __attribute__packed__ Event_t {
            //     using Type = Event_t;

            //     CREATE_UINT8_BITFIELD_MIN_MAX(enabled, 1, 0, 1, 0, 1);

            //     Event_t() = default;
            //     Event_t(bool aEnabled = false) : enabled(aEnabled) {}
            // };

            typedef struct __attribute__packed__ Config_t {
                using Type = Config_t;
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_sleep_time, 16, 0, 3600 * 16, 2, 1);            // seconds / max. 16 hours
                CREATE_UINT32_BITFIELD_MIN_MAX(deep_sleep_time, 16, 0, 44640, 0, 1);                // in minutes / max. 31 days, 0 = indefinitely
                CREATE_UINT32_BITFIELD_MIN_MAX(click_time, 11, 0, 2000, 350, 1);                    // millis
                CREATE_UINT32_BITFIELD_MIN_MAX(hold_time, 13, 0, 8000, 750, 1);                     // millis
                CREATE_UINT32_BITFIELD_MIN_MAX(hold_repeat_time, 12, 0, 4000, 500, 1);              // millis
                CREATE_UINT32_BITFIELD_MIN_MAX(udp_port, 16, 0, 65535, 7881, 1);                    // port
                CREATE_UINT32_BITFIELD_MIN_MAX(udp_enable, 1, 0, 1, 1, 1);                          // flag
                CREATE_UINT32_BITFIELD_MIN_MAX(mqtt_enable, 1, 0, 1, 1, 1);                         // flag

                Action_t actions[IOT_REMOTE_CONTROL_BUTTON_COUNT];
                ComboAction_t combo[4];

                // Button_t buttons[IOT_REMOTE_CONTROL_BUTTON_COUNT];
                Events enabled;
                // Event_t events[9];

                Config_t();

            } Config_t;

        };

        class RemoteControl : public RemoteControlConfig, public ConfigGetterSetter<RemoteControlConfig::Config_t, _H(MainConfig().plugins.remote.cfg) CIF_DEBUG(, &handleNameRemoteConfig_t)> {
        public:
            static void defaults();
            RemoteControl() {}

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.udp_host, UdpHost, 0, 64);

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name1, Name1, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name2, Name2, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name3, Name3, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name4, Name4, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name5, Name5, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name6, Name6, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name7, Name7, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name8, Name8, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name9, Name9, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.name10, Name10, 1, 64);

            static void setName(uint8_t num, const char *);
            static const char *getName(uint8_t num);
            static constexpr uint8_t kButtonCount = IOT_REMOTE_CONTROL_BUTTON_COUNT;

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event1, Event1, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event2, Event2, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event3, Event3, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event4, Event4, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event5, Event5, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event6, Event6, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event7, Event7, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event8, Event8, 1, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.event9, Event9, 1, 64);

            static void setEventName(uint8_t num, const char *);
            static const char *getEventName(uint8_t num);
            static constexpr auto kEventCount = static_cast<uint8_t>(EventNameType::MAX);

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
                    buffer.push_back(configs[i]);
                    buffer.write(names[i]);
                }
                setConfig(buffer.begin(), buffer.length());
            }
        };

        // --------------------------------------------------------------------
        // Weather Station
        class WeatherStationConfig {
        public:
            typedef struct __attribute__packed__ Config_t {
                using Type = Config_t;

                uint8_t weather_poll_interval;                      // minutes
                uint16_t api_timeout;                               // seconds
                uint8_t backlight_level;                            // PWM level 0-1023
                uint8_t touch_threshold;
                uint8_t released_threshold;
                CREATE_UINT8_BITFIELD(is_metric, 1);
                CREATE_UINT8_BITFIELD(time_format_24h, 1);
                CREATE_UINT8_BITFIELD(show_webui, 1);
                float temp_offset;
                float humidity_offset;
                float pressure_offset;
                uint8_t screenTimer[8];                             // seconds

                Config_t();
            } Config_t;
        };

        class WeatherStation : public WeatherStationConfig, public ConfigGetterSetter<WeatherStationConfig::Config_t, _H(MainConfig().plugins.weatherstation.cfg) CIF_DEBUG(, &handleNameWeatherStationConfig_t)> {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiKey, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.weatherstation, ApiQuery, 0, 64);

        };

        // --------------------------------------------------------------------
        // Alarm

        #ifndef IOT_ALARM_PLUGIN_MAX_ALERTS
        #define IOT_ALARM_PLUGIN_MAX_ALERTS                         10
        #endif
        #ifndef IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION
        #define IOT_ALARM_PLUGIN_DEFAULT_MAX_DURATION               900
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

            typedef struct __attribute__packed__ AlarmTime_t {
                using Type = AlarmTime_t;
                TimeType timestamp;
                CREATE_UINT8_BITFIELD(hour, 8);
                CREATE_UINT8_BITFIELD(minute, 8);
                WeekDay_t week_day;

                static void set_bits_weekdays(Type &obj, uint8_t value) { \
                    obj.week_day.week_days = value;
                } \
                static uint8_t get_bits_weekdays(const Type &obj) { \
                    return obj.week_day.week_days; \
                }
            } AlarmTime_t;

            typedef struct __attribute__packed__ SingleAlarm_t {
                using Type = SingleAlarm_t;
                AlarmTime_t time;
                CREATE_UINT16_BITFIELD(max_duration, 16); // limit in seconds, 0 = unlimited
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
                AT_MOST_ONCE = 0,
                AT_LEAST_ONCE = 1,
                EXACTLY_ONCE = 2,
                MAX,
                PERSISTENT_STORAGE = AT_LEAST_ONCE,
                AUTO_DISCOVERY = AT_LEAST_ONCE,
                DEFAULT = 0xff,
            };

            static_assert(QosType::AUTO_DISCOVERY != QosType::AT_MOST_ONCE, "QoS 1 or 2 required");

            AUTO_DEFAULT_PORT_CONST_SECURE(1883, 8883);

            typedef struct __attribute__packed__ MqttConfig_t {
                using Type = MqttConfig_t;
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery, 1, 0, 1, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(enable_shared_topic, 1, 0, 1, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(keepalive, 10, 0, 900, 15, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery_rebroadcast_interval, 16, 15, 43200, 24 * 60, 3600); // minutes
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_min, 16, 250, 60000, 5000, 500);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_max, 16, 5000, 60000, 60000, 1000);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_reconnect_incr, 7, 0, 100, 10);
                CREATE_UINT32_BITFIELD_MIN_MAX(auto_discovery_delay, 10, 10, 900, 30, 1); // seconds
                uint32_t _free2 : 7; // avoid warning
                CREATE_ENUM_BITFIELD(mode, ModeType);
                CREATE_ENUM_BITFIELD(qos, QosType);
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, kPortDefault, kPortDefaultSecure, static_cast<ModeType>(mode) == ModeType::SECURE);

                // minutes
                uint32_t getAutoDiscoveryRebroadcastInterval() const {
                    return auto_discovery && auto_discovery_delay ? auto_discovery_rebroadcast_interval : 0;
                }

                MqttConfig_t() :
                    auto_discovery(kDefaultValueFor_auto_discovery),
                    enable_shared_topic(kDefaultValueFor_enable_shared_topic),
                    keepalive(kDefaultValueFor_keepalive),
                    auto_discovery_rebroadcast_interval(kDefaultValueFor_auto_discovery_rebroadcast_interval),
                    auto_reconnect_min(kDefaultValueFor_auto_reconnect_min),
                    auto_reconnect_max(kDefaultValueFor_auto_reconnect_max),
                    auto_reconnect_incr(kDefaultValueFor_auto_reconnect_incr),
                    auto_discovery_delay(kDefaultValueFor_auto_discovery_delay),
                    mode(cast_int_mode(ModeType::UNSECURE)),
                    qos(cast_int_qos(QosType::EXACTLY_ONCE)),
                    __port(kDefaultValueFor___port)
                {}

            } MqttConfig_t;

            static constexpr size_t MqttConfig_tSize = sizeof(MqttConfig_t);
        };

        class MQTTClient : public MQTTClientConfig, public ConfigGetterSetter<MQTTClientConfig::MqttConfig_t, _H(MainConfig().plugins.mqtt.cfg) CIF_DEBUG(, &handleNameMqttConfig_t)> {
        public:
            static void defaults();
            static bool isEnabled();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Hostname, 1, 128);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Username, 0, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, Password, 6, 32);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, BaseTopic, 4, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, GroupTopic, 4, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, AutoDiscoveryPrefix, 1, 32);
            // CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.mqtt, SharedTopic, 4, 128);

            static const uint8_t *getFingerPrint(uint16_t &size);
            static void setFingerPrint(const uint8_t *fingerprint, uint16_t size);
            static constexpr size_t kFingerprintMaxSize = 20;
        };

        // --------------------------------------------------------------------
        // Syslog

        class SyslogClientConfig {
        public:
            using SyslogProtocolType = ::SyslogProtocolType;

            AUTO_DEFAULT_PORT_CONST_SECURE(514, 6514);

            typedef struct __attribute__packed__ SyslogConfig_t {
                union __attribute__packed__ {
                    SyslogProtocolType protocol_enum;
                    uint8_t protocol;
                };
                AUTO_DEFAULT_PORT_GETTER_SETTER_SECURE(__port, kPortDefault, kPortDefaultSecure, protocol_enum == SyslogProtocolType::TCP_TLS);

#if 0
                template<class T>
                void dump() {
                    CONFIG_DUMP_STRUCT_INFO(T);
                    CONFIG_DUMP_STRUCT_VAR(protocol);
                    CONFIG_DUMP_STRUCT_VAR(__port);
                }
#endif

                SyslogConfig_t() : protocol_enum(SyslogProtocolType::TCP), __port(kDefaultValueFor___port) {}

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
                using Type = NtpClientConfig_t;

                CREATE_UINT16_BITFIELD_MIN_MAX(refreshInterval, 16, 5, 720 * 60, 15, 5);
                uint32_t getRefreshIntervalMillis() const {
                    return refreshInterval * 60U * 1000U;
                }

                NtpClientConfig_t() : refreshInterval(kDefaultValueFor_refreshInterval) {}

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

// send recorded data over UDP
#ifndef IOT_SENSOR_HAVE_BATTERY_RECORDER
#define IOT_SENSOR_HAVE_BATTERY_RECORDER                        0
#endif

        class SensorConfig {
        public:

#if IOT_SENSOR_HAVE_BATTERY
#ifndef __IOT_SENSOR_COMFIG_T
#define __IOT_SENSOR_COMFIG_T 1
#endif
            enum class SensorRecordType : uint8_t {
                MIN = 0,
                NONE = MIN,
                ADC = 0x01,
                SENSOR = 0x02,
                BOTH = 0x03,
                MAX
            };

            typedef struct BatteryConfig_t {
                using Type = BatteryConfig_t;

                float calibration;
                float offset;
                CREATE_UINT8_BITFIELD_MIN_MAX(precision, 4, 0, 7, 2, 1);

#if IOT_SENSOR_HAVE_BATTERY_RECORDER
                CREATE_ENUM_BITFIELD(record, SensorRecordType);
                CREATE_UINT16_BITFIELD_MIN_MAX(port, 16, 0, 0xffff, 2, 1);
                CREATE_IPV4_ADDRESS(address);
#endif

                BatteryConfig_t();

            } BatteryConfig_t;

            using SensorConfig_t = BatteryConfig_t;
#endif

#if (IOT_SENSOR_HAVE_HLW8012 || IOT_SENSOR_HAVE_HLW8032)
#ifndef __IOT_SENSOR_COMFIG_T
#define __IOT_SENSOR_COMFIG_T 1
#endif

            typedef struct HLW80xxConfig_t {

                using Type = HLW80xxConfig_t;
                float calibrationU;
                float calibrationI;
                float calibrationP;
                uint64_t energyCounter;
                CREATE_UINT8_BITFIELD_MIN_MAX(extraDigits, 4, 0, 7, 0, 1);
                HLW80xxConfig_t();

            } HLW80xxConfig_t;

            using SensorConfig_t = HLW80xxConfig_t;
#endif

#ifndef __IOT_SENSOR_COMFIG_T
#define __IOT_SENSOR_COMFIG_T 1

            typedef struct SensorConfig_t {
                using Type = SensorConfig_t;
            } SensorConfig_t;

#endif

        };

        class Sensor : public SensorConfig, public ConfigGetterSetter<SensorConfig::SensorConfig_t, _H(MainConfig().plugins.sensor.cfg) CIF_DEBUG(, &handleNameSensorConfig_t)> {
        public:
            static void defaults();

#if IOT_SENSOR_HAVE_BATTERY_RECORDER
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.sensor, RecordSensorData, 0, 128);

            static void setRecordHostAndPort(String host, uint16_t port) {
                if (port) {
                    host += ':';
                    host + String(port);
                }
                setRecordSensorData(host);
            }
            static String getRecordHost() {
                String str = getRecordSensorData();
                auto pos = str.lastIndexOf(':');
                if (pos != -1) {
                    str.remove(pos);
                }
                return str;
            }
            static uint16_t getRecordPort() {
                auto str = getRecordSensorData();
                auto pos = strrchr(str, ':');
                if (!pos) {
                    return 0;
                }
                return atoi(pos + 1);
            }
#endif
        };

        // --------------------------------------------------------------------
        // BlindsController

        #ifndef BLINDS_CONFIG_MAX_OPERATIONS
        #define BLINDS_CONFIG_MAX_OPERATIONS                        6
        #endif

        class BlindsConfig {
        public:
            static constexpr size_t kChannel0_OpenPinArrayIndex = 0;
            static constexpr size_t kChannel0_ClosePinArrayIndex = 1;
            static constexpr size_t kChannel1_OpenPinArrayIndex = 2;
            static constexpr size_t kChannel1_ClosePinArrayIndex = 3;
            static constexpr size_t kMultiplexerPinArrayIndex = 4;
            static constexpr size_t kDACPinArrayIndex = 5;

            static constexpr size_t kMaxOperations = BLINDS_CONFIG_MAX_OPERATIONS;

            enum class OperationType : uint32_t {
                NONE = 0,
                OPEN_CHANNEL0,                    // _FOR_CHANNEL0_AND_ALL
                OPEN_CHANNEL0_FOR_CHANNEL1,       // _ONLY
                OPEN_CHANNEL1,
                OPEN_CHANNEL1_FOR_CHANNEL0,
                CLOSE_CHANNEL0,
                CLOSE_CHANNEL0_FOR_CHANNEL1,
                CLOSE_CHANNEL1,
                CLOSE_CHANNEL1_FOR_CHANNEL0,
                DO_NOTHING,
                DO_NOTHING_CHANNEL0,
                DO_NOTHING_CHANNEL1,
                MAX
            };

            enum class PlayToneType : uint32_t {
                NONE = 0,
                INTERVAL,
                IMPERIAL_MARCH,
                MAX
            };

            typedef struct __attribute__packed__ BlindsConfigOperation_t {
                using Type = BlindsConfigOperation_t;

                CREATE_UINT32_BITFIELD_MIN_MAX(delay, 20, 0, 900000, 0, 500);
                CREATE_UINT32_BITFIELD_MIN_MAX(relative_delay, 1, 0, 1, 0, 1);
                CREATE_ENUM_BITFIELD(play_tone, PlayToneType);
                CREATE_ENUM_BITFIELD(action, OperationType);

                // uint16_t delay;                                     // delay before execution in seconds
                // OperationType type;                                 // action

                BlindsConfigOperation_t() :
                    delay(kDefaultValueFor_delay),
                    relative_delay(0),
                    play_tone(BlindsConfigOperation_t::cast_int_play_tone(PlayToneType::NONE)),
                    action(BlindsConfigOperation_t::cast_int_action(OperationType::NONE))
                {}

                template<typename Archive>
                void serialize(Archive & ar, kfc::serialization::version version){
                    ar & KFC_SERIALIZATION_NVP(action);
                    ar & KFC_SERIALIZATION_NVP(delay);
                }

            } BlindsConfigOperation_t;

            typedef struct __attribute__packed__ BlindsConfigChannel_t {
                using Type = BlindsConfigChannel_t;
                CREATE_UINT32_BITFIELD_MIN_MAX(current_limit_mA, 12, 1, 4095, 100, 10);                     // bits 00:11 ofs:len 000:12 0-0x0fff (4095)
                CREATE_UINT32_BITFIELD_MIN_MAX(dac_pwm_value, 10, 0, 1023, 512);                            // bits 12:21 ofs:len 012:10 0-0x03ff (1023)
                CREATE_UINT32_BITFIELD_MIN_MAX(pwm_value, 10, 0, 1023, 256);                                // bits 22:31 ofs:len 022:10 0-0x03ff (1023)
                CREATE_UINT32_BITFIELD_MIN_MAX(current_avg_period_us, 16, 100, 50000, 2500, 100);           // bits 00:15 ofs:len 032:16 0-0xffff (65535)
                CREATE_UINT32_BITFIELD_MIN_MAX(open_time_ms, 16, 0, 60000, 5000, 250);                      // bits 16:31 ofs:len 048:16 0-0xffff (65535)
                CREATE_UINT16_BITFIELD_MIN_MAX(close_time_ms, 16, 0, 60000, 5000, 250);                     // bits 00:15 ofs:len 064:16 0-0xffff (65535)
                BlindsConfigChannel_t();

                template<typename Archive>
                void serialize(Archive & ar, kfc::serialization::version version) {
                    ar & KFC_SERIALIZATION_NVP(current_limit_mA);
                    ar & KFC_SERIALIZATION_NVP(dac_pwm_value);
                    ar & KFC_SERIALIZATION_NVP(pwm_value);
                    ar & KFC_SERIALIZATION_NVP(current_avg_period_us);
                    ar & KFC_SERIALIZATION_NVP(open_time_ms);
                    ar & KFC_SERIALIZATION_NVP(close_time_ms);
                }
            } BlindsConfigChannel_t;

            typedef struct __attribute__packed__ BlindsConfig_t {
                using Type = BlindsConfig_t;

                BlindsConfigChannel_t channels[2];
                BlindsConfigOperation_t open[kMaxOperations];
                BlindsConfigOperation_t close[kMaxOperations];
                uint8_t pins[6];
                CREATE_UINT16_BITFIELD_MIN_MAX(pwm_frequency, 16, 1000, 40000, 30000, 1000);                // bits 00:15 ofs:len 000:16 0-0xffff (65535)
                CREATE_UINT16_BITFIELD_MIN_MAX(adc_recovery_time, 16, 1000, 65000, 12500, 500);             // bits 00:15 ofs:len 016:16 0-0xffff (65535)
                CREATE_UINT16_BITFIELD_MIN_MAX(adc_read_interval, 12, 250, 4000, 750, 100);                 // bits 00:11 ofs:len 032:12 0-0xfff (4095)
                CREATE_UINT16_BITFIELD_MIN_MAX(adc_recoveries_per_second, 3, 1, 7, 4);                      // bits 12:14 ofs:len 044:03 0-0x07 (7)
                CREATE_UINT16_BITFIELD_MIN_MAX(adc_multiplexer, 1, 0, 1, 0);                                // bits 15:15 ofs:len 047:01 0-0x01 (1)
                CREATE_INT32_BITFIELD_MIN_MAX(adc_offset, 11, -1000, 1000, 0);                              // bits 00:10 ofs:len 048:11 0-0x07ff (-1023 - 1023)
                CREATE_INT32_BITFIELD_MIN_MAX(pwm_softstart_time, 12, 0, 1000, 300, 10);                    // bits 11:23 ofs:len 059:23
                CREATE_INT32_BITFIELD_MIN_MAX(play_tone_channel, 3, 0, 2, 0, 0);                            // bits 24:27
                CREATE_UINT32_BITFIELD_MIN_MAX(tone_frequency, 11, 150, 2000, 800, 50);
                CREATE_UINT32_BITFIELD_MIN_MAX(tone_pwm_value, 10, 0, 1023, 150, 1);


                template<typename Archive>
                void serialize(Archive & ar, kfc::serialization::version version) {
                    ar & KFC_SERIALIZATION_NVP(pwm_frequency);
                    ar & KFC_SERIALIZATION_NVP(adc_recovery_time);
                    ar & KFC_SERIALIZATION_NVP(adc_read_interval);
                    ar & KFC_SERIALIZATION_NVP(adc_recoveries_per_second);
                    ar & KFC_SERIALIZATION_NVP(adc_multiplexer);
                    ar & KFC_SERIALIZATION_NVP(adc_offset);
                    ar & KFC_SERIALIZATION_NVP(pwm_softstart_time);
                }

                BlindsConfig_t();

            } SensorConfig_t;

            static constexpr size_t BlindsConfig_t_Size = sizeof(BlindsConfig_t);
            static constexpr size_t BlindsConfig_t_open_Size = sizeof(BlindsConfig_t().open);
            static constexpr size_t BlindsConfigChannel_t_Size = sizeof(BlindsConfigChannel_t);
            static constexpr size_t BlindsConfigOperation_t_Size = sizeof(BlindsConfigOperation_t);
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
        // Dimmer

#if !IOT_DIMMER_MODULE && !IOT_ATOMIC_SUN_V2
        typedef struct {
        } register_mem_cfg_t;
        typedef struct  {
        } dimmer_version_t;
#endif

        class DimmerConfig {
        public:
            typedef struct __attribute__packed__ DimmerConfig_t {
                using Type = DimmerConfig_t;
                dimmer_version_t version;
                register_mem_cfg_t cfg;
            #if IOT_ATOMIC_SUN_V2
                int8_t channel_mapping[4];
            #endif
                float on_fadetime;
                float off_fadetime;
                float lp_fadetime;
            #if IOT_DIMMER_MODULE_HAS_BUTTONS
                /*CREATE_UINT32_BITFIELD_MIN_MAX(config_valid, 1, 0, 1, false);*/                           // bits 00:00 ofs:len 000:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(off_delay, 9, 0, 480, 0);                                    // bits 01:09 ofs:len 001:09 0-0x01ff (511)
                CREATE_UINT32_BITFIELD_MIN_MAX(off_delay_signal, 1, 0, 1, false);                           // bits 10:10 ofs:len 010:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_down_inverted, 1, 0, 1, false);                      // bits 11:11 ofs:len 011:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_up, 5, 0, 16, 4);                                    // bits 12:16 ofs:len 012:05 0-0x001f (31)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_up_inverted, 1, 0, 1, false);                        // bits 17:17 ofs:len 017:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(single_click_time, 14, 250, 16000, 750);                     // bits 18:31 ofs:len 018:14 0-0x3fff (16383)
                CREATE_UINT32_BITFIELD_MIN_MAX(longpress_time, 12, 0, 4000, 1000);                        // bits 00:11 ofs:len 032:12 0-0x0fff (4095)
                CREATE_UINT32_BITFIELD_MIN_MAX(min_brightness, 7, 0, 95, 15);                               // bits 12:18 ofs:len 044:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(pin_ch0_down, 5, 0, 16, 13);                                 // bits 19:23 ofs:len 051:05 0-0x001f (31)
                CREATE_UINT32_BITFIELD_MIN_MAX(shortpress_steps, 7, 4, 100, 15);                             // bits 24:30 ofs:len 056:07 0-0x007f (127)
                uint32_t __free2: 1;                                                                        // bits 31:31 ofs:len 063:01 0-0x0001 (1)
                CREATE_UINT32_BITFIELD_MIN_MAX(longpress_max_brightness, 7, 0, 100, 80);                   // bits 00:06 ofs:len 064:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(longpress_min_brightness, 7, 0, 99, 33);                     // bits 07:13 ofs:len 071:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(max_brightness, 7, 5, 100, 100);                            // bits 14:20 ofs:len 078:07 0-0x007f (127)
                CREATE_UINT32_BITFIELD_MIN_MAX(shortpress_time, 10, 100, 1000, 275);                        // bits 21:30 ofs:len 085:10 0-0x03ff (1023)
                uint32_t __free3: 1;                                                                        // bits 31:31 ofs:len 095:01 0-0x0001 (1)

                uint8_t pin(uint8_t idx) const {
                    return pin(idx / 2, idx % 2);
                }
                uint8_t pin(uint8_t channel, uint8_t button) const {
                    switch(channel<<4|button) {
                        case 0x00: return pin_ch0_up;
                        case 0x01: return pin_ch0_down;
                    }
                    return 0xff;
                }
                uint8_t pin_inverted(uint8_t idx) const {
                    return pin_inverted(idx / 2, idx % 2);
                }
                bool pin_inverted(uint8_t channel, uint8_t button) const {
                    switch(channel<<4|button) {
                        case 0x00: return pin_ch0_up_inverted;
                        case 0x01: return pin_ch0_down_inverted;
                    }
                    return 0xff;
                }
            #else
                CREATE_BOOL_BITFIELD(config_valid);
            #endif

                DimmerConfig_t();

            } DimmerConfig_t;
        };

        class Dimmer : public DimmerConfig, public ConfigGetterSetter<DimmerConfig::DimmerConfig_t, _H(MainConfig().plugins.dimmer.cfg) CIF_DEBUG(, &handleNameDimmerConfig_t)>
        {
        public:
            static void defaults();
        };

        // --------------------------------------------------------------------
        // Clock

#if IOT_CLOCK
        #include "../src/plugins/clock/clock_def.h"
#endif

        class ClockConfig {
        public:
            typedef union __attribute__packed__ ClockColor_t {
                uint32_t value: 24;
                uint8_t bgr[3];
                struct __attribute__packed__ {
                    uint8_t blue;
                    uint8_t green;
                    uint8_t red;
                };
                ClockColor_t(uint32_t val = 0) : value(val) {}
                operator uint32_t() const {
                    return value;
                }
            } ClockColor_t;

            typedef struct __attribute__packed__ RainbowMultiplier_t {
                float value;
                float min;
                float max;
                float incr;
                RainbowMultiplier_t();
                RainbowMultiplier_t(float value, float min, float max, float incr);
            } RainbowMultiplier_t;

            typedef struct __attribute__packed__ RainbowColor_t {
                ClockColor_t min;
                ClockColor_t factor;
                float red_incr;
                float green_incr;
                float blue_incr;
                RainbowColor_t();
            } RainbowColor_t;

            typedef struct __attribute__packed__ FireAnimation_t {

                enum class Orientation {
                    MIN = 0,
                    VERTICAL = MIN,
                    HORIZONTAL,
                    MAX
                };

                using Type = FireAnimation_t;
                uint8_t cooling;
                uint8_t sparking;
                uint8_t speed;
                CREATE_ENUM_BITFIELD(orientation, Orientation);
                CREATE_BOOL_BITFIELD(invert_direction);
                ClockColor_t factor;

                Orientation getOrientation() const {
                    return get_enum_orientation(*this);
                }

                FireAnimation_t();

            } FireAnimation_t;

            typedef struct __attribute__packed__ ClockConfig_t {
                using Type = ClockConfig_t;

                enum class AnimationType : uint8_t {
                    MIN = 0,
                    SOLID = 0,
                    RAINBOW,
                    FLASHING,
                    FADING,
                    FIRE,
                    INTERLEAVED,
#if !IOT_LED_MATRIX
                    COLON_SOLID,
                    COLON_BLINK_FAST,
                    COLON_BLINK_SLOWLY,
#endif
                    MAX,
                };

                static const __FlashStringHelper *getAnimationNames();
                static const __FlashStringHelper *getAnimationNamesJsonArray();
                static const __FlashStringHelper *getAnimationName(AnimationType type);

                enum class InitialStateType : uint8_t  {
                    MIN = 0,
                    OFF = 0,
                    ON,
                    RESTORE,
                    MAX
                };

                ClockColor_t solid_color;
                CREATE_ENUM_BITFIELD(animation, AnimationType);
                CREATE_ENUM_BITFIELD(initial_state, InitialStateType);
                CREATE_UINT8_BITFIELD(time_format_24h, 1);
                CREATE_UINT8_BITFIELD(dithering, 1);
                CREATE_UINT8_BITFIELD(standby_led, 1);
                CREATE_UINT8_BITFIELD(enabled, 1);
                CREATE_UINT32_BITFIELD_MIN_MAX(fading_time, 6, 0, 60, 10, 1)
#if IOT_CLOCK_HAVE_POWER_LIMIT
                CREATE_UINT32_BITFIELD_MIN_MAX(power_limit, 8, 0, 255, 0, 1)
#endif
                CREATE_UINT32_BITFIELD_MIN_MAX(brightness, 8, 0, 255, 255 / 4, 1)
                CREATE_INT32_BITFIELD_MIN_MAX(auto_brightness, 11, -1, 1023, -1, 1)
#if !IOT_LED_MATRIX
                CREATE_UINT32_BITFIELD_MIN_MAX(blink_colon_speed, 13, 50, 8000, 1000, 100)
#endif
                CREATE_UINT32_BITFIELD_MIN_MAX(flashing_speed, 13, 50, 8000, 150, 100)

                static const uint16_t kPowerNumLeds = 256;

#if IOT_CLOCK_HAVE_POWER_LIMIT
                struct __attribute__packed__ {
                    uint16_t red;
                    uint16_t green;
                    uint16_t blue;
                    uint16_t idle;
                } power;
#endif

                struct __attribute__packed__ {
                    struct __attribute__packed__ {
                        uint8_t min;
                        uint8_t max;
                    } temperature_reduce_range;
                    uint8_t max_temperature;
                    int8_t regulator_margin;
                } protection;

                struct __attribute__packed__ {
                    RainbowMultiplier_t multiplier;
                    RainbowColor_t color;
                    uint16_t speed;
                } rainbow;

                struct __attribute__packed__ {
                    ClockColor_t color;
                    uint16_t speed;
                } alarm;

                typedef struct __attribute__packed__ fading_t {
                    using Type = fading_t;
                    CREATE_UINT32_BITFIELD_MIN_MAX(speed, 17, 100, 100000, 1000, 100)
                    CREATE_UINT32_BITFIELD_MIN_MAX(delay, 12, 0, 3600, 3, 1)
                    ClockColor_t factor;
                } fading_t;
                fading_t fading;

                FireAnimation_t fire;

                struct __attribute__packed__ {
                    uint8_t rows;
                    uint8_t cols;
                    uint32_t time;
                } interleaved;

                ClockConfig_t();

                uint16_t getBrightness() const {
                    return brightness;
                }
                void setBrightness(uint8_t pBrightness) {
                    brightness = pBrightness;
                }

                uint32_t getFadingTimeMillis() const {
                    return fading_time * 1000;
                }
                void setFadingTimeMillis(uint32_t time) {
                    fading_time = time / 1000;
                }

                AnimationType getAnimation() const {
                    return get_enum_animation(*this);
                }
                void setAnimation(AnimationType animation) {
                    set_enum_animation(*this, animation);
                }

                InitialStateType getInitialState() const {
                    return get_enum_initial_state(*this);
                }
                void setInitialState(InitialStateType state) {
                    set_enum_initial_state(*this, state);
                }

            } ClockConfig_t;
        };

        class Clock : public ClockConfig, public ConfigGetterSetter<ClockConfig::ClockConfig_t, _H(MainConfig().plugins.clock.cfg) CIF_DEBUG(, &handleNameClockConfig_t)>
        {
        public:
            static void defaults();
        };

        // --------------------------------------------------------------------
        // Ping

        class PingConfig {
        public:
            typedef struct __attribute__packed__ PingConfig_t {
                using Type = PingConfig_t;

                uint16_t interval;                      // minutes
                uint16_t timeout;                       // ms
                CREATE_UINT8_BITFIELD(count, 6);        // number of pings
                CREATE_UINT8_BITFIELD(console, 1);      // ping console @ utilities
                CREATE_UINT8_BITFIELD(service, 1);      // ping monitor background service

                static constexpr uint8_t kRepeatCountMin = 1;
                static constexpr uint8_t kRepeatCountMax = (1 << kBitCountFor_count) - 1;
                static constexpr uint8_t kRepeatCountDefault = 4;
                static constexpr uint16_t kIntervalMin = 1;
                static constexpr uint16_t kIntervalMax = (24 * 60) * 30;
                static constexpr uint16_t kIntervalDefault = 5;
                static constexpr uint16_t kTimeoutMin = 100;
                static constexpr uint16_t kTimeoutMax = 60000;
                static constexpr uint16_t kTimeoutDefault = 5000;

                PingConfig_t();

            } PingConfig_t;
        };

        class Ping : public PingConfig, public ConfigGetterSetter<PingConfig::PingConfig_t, _H(MainConfig().plugins.ping.cfg) CIF_DEBUG(, &handleNamePingConfig_t)>
        {
        public:
            static void defaults();

            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host1, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host2, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host3, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host4, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host5, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host6, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host7, 0, 64);
            CREATE_STRING_GETTER_SETTER_MIN_MAX(MainConfig().plugins.ping, Host8, 0, 64);

            static void setHost(uint8_t num, const char *);
            static const char *getHost(uint8_t num);
            static void removeEmptyHosts(); // trim hostnames and move empty hostnames to the end of the list
            static uint8_t getHostCount();
            static constexpr uint8_t kHostsMax = 8;

        };

        // --------------------------------------------------------------------
        // MDNS

        class MDNS
        {
        public:
        };


        // --------------------------------------------------------------------
        // Plugin Structure

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
        Dimmer dimmer;
        Clock clock;
        Ping ping;
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
