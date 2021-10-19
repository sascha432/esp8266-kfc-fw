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
#include <boost/preprocessor/variadic/to_tuple.hpp>
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
#define DEBUG_CONFIG_CLASS 0
#endif

#if DEBUG_CONFIGURATION_GETHANDLE && !DEBUG_CONFIG_CLASS
#undef DEBUG_CONFIG_CLASS
#define DEBUG_CONFIG_CLASS 1
#endif

#if DEBUG_CONFIG_CLASS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if DEBUG_CONFIG_CLASS && 1
#define __CDBG_printf(...)                                                  __DBG_printf(__VA_ARGS__)
// #define __CDBG_dump(class_name, cfg)                                        cfg.dump<class_name>();
#define __CDBG_dump(class_name, cfg)
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
    static constexpr ConfigurationHelper::HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    inline static size_t get##name##Size() { return k##name##MaxSize; } \
    inline static const uint8_t *get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return KFCConfigurationClasses::loadBinaryConfig(k##name##ConfigHandle, k##name##MaxSize); } \
    inline static void set##name(const uint8_t *data) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); KFCConfigurationClasses::storeBinaryConfig(k##name##ConfigHandle, data, k##name##MaxSize); }

#define CREATE_STRING_GETTER_SETTER(class_name, name, len) \
    static constexpr size_t k##name##MaxSize = len; \
    static constexpr ConfigurationHelper::HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    inline static const char *get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return KFCConfigurationClasses::loadStringConfig(k##name##ConfigHandle); } \
    inline static char *getWriteable##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return KFCConfigurationClasses::loadWriteableStringConfig(k##name##ConfigHandle, k##name##MaxSize); } \
    inline static void set##name(const char *str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); KFCConfigurationClasses::storeStringConfig(k##name##ConfigHandle, str); } \
    inline static void set##name(const __FlashStringHelper *str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); KFCConfigurationClasses::storeStringConfig(k##name##ConfigHandle, str); } \
    inline static void set##name(const String &str) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); KFCConfigurationClasses::storeStringConfig(k##name##ConfigHandle, str); }

#define  CREATE_STRING_GETTER_SETTER_MIN_MAX(class_name, name, mins, maxs) \
    inline static FormUI::Validator::Length &add##name##LengthValidator(FormUI::Form::BaseForm &form, bool allowEmpty = false) { \
        return form.addValidator(FormUI::Validator::Length(k##name##MinSize, k##name##MaxSize, allowEmpty)); \
    } \
    inline static FormUI::Validator::Length &add##name##LengthValidator(const String &message, FormUI::Form::BaseForm &form, bool allowEmpty = false) { \
        return form.addValidator(FormUI::Validator::Length(message, k##name##MinSize, k##name##MaxSize, allowEmpty)); \
    } \
    static constexpr size_t k##name##MinSize = mins; \
    CREATE_STRING_GETTER_SETTER(class_name, name, maxs)

//name, size, type, min_value, max_value, default_value[, step_size]
#define CREATE_BITFIELD_TYPE_MIN_MAX(name, size, type, min_value, max_value, ...) \
    using common_type_t_##name = std::common_type_t<type, long>; \
    inline static FormUI::Validator::Range &addRangeValidatorFor_##name(FormUI::Form::BaseForm &form, bool allowZero = false) { \
        form.getLastField().getFormUI()->addItems(FormUI::PlaceHolder(kDefaultValueFor_##name), FormUI::MinMax(kMinValueFor_##name, kMaxValueFor_##name), \
            BOOST_PP_REMOVE_PARENS(BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),1),(FormUI::Type::NUMBER_RANGE),(FormUI::Attribute(F("step"),BOOST_PP_VARIADIC_ELEM(1,##__VA_ARGS__)),FormUI::Type::NUMBER_RANGE)))); \
        return form.addValidator(FormUI::Validator::Range(static_cast<common_type_t_##name>(kMinValueFor_##name), static_cast<common_type_t_##name>(kMaxValueFor_##name), allowZero)); \
    } \
    inline static FormUI::Validator::Range &addRangeValidatorFor_##name(const String &message, FormUI::Form::BaseForm &form, bool allowZero = false) { \
        form.getLastField().getFormUI()->addItems(FormUI::PlaceHolder(kDefaultValueFor_##name), FormUI::MinMax(kMinValueFor_##name, kMaxValueFor_##name), \
            BOOST_PP_REMOVE_PARENS(BOOST_PP_IF(BOOST_PP_EQUAL(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__),1),(FormUI::Type::NUMBER_RANGE),(FormUI::Attribute(F("step"),BOOST_PP_VARIADIC_ELEM(1,##__VA_ARGS__)),FormUI::Type::NUMBER_RANGE)))); \
        return form.addValidator(FormUI::Validator::Range(message, static_cast<common_type_t_##name>(kMinValueFor_##name), static_cast<common_type_t_##name>(kMaxValueFor_##name), allowZero)); \
    } \
    static constexpr type kMinValueFor_##name = min_value; \
    static constexpr type kMaxValueFor_##name = max_value; \
    static constexpr type kDefaultValueFor_##name = BOOST_PP_VARIADIC_ELEM(0,##__VA_ARGS__); \
    static constexpr type kTypeMinValueFor_##name = std::is_signed<type>::value ? -(1LL << (size - 1)) : 0; \
    static constexpr type kTypeMaxValueFor_##name = std::is_signed<type>::value ? (1LL << (size - 1)) - 1 : (1ULL << size) - 1; \
    static_assert(min_value >= kTypeMinValueFor_##name && min_value <= kTypeMaxValueFor_##name, "min_value value out of range (" _STRINGIFY(size) " bits)"); \
    static_assert(max_value >= kTypeMinValueFor_##name && max_value <= kTypeMaxValueFor_##name, "max_value value out of range (" _STRINGIFY(size) " bits)"); \
    static_assert(kDefaultValueFor_##name == 0 || (kDefaultValueFor_##name >= kTypeMinValueFor_##name && kDefaultValueFor_##name <= kTypeMaxValueFor_##name), "default_value value out of range"); \
    static_assert(min_value >= std::numeric_limits<common_type_t_##name>::min() && min_value <= std::numeric_limits<common_type_t_##name>::max(), "min_value value out of range (common_type_t)"); \
    static_assert(max_value >= std::numeric_limits<common_type_t_##name>::min() && max_value <= std::numeric_limits<common_type_t_##name>::max(), "max_value value out of range (common_type_t)"); \
    CREATE_BITFIELD_TYPE(name, size, type, bits)

#define CREATE_BOOL_BITFIELD_MIN_MAX(name, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, 1, bool, false, true, ##__VA_ARGS__)

#define CREATE_UINT8_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, uint8_t, min_value, max_value, ##__VA_ARGS__)

#define CREATE_INT8_BITFIELD_MIN_MAX(name, size, min_value, max_value, ...) \
    CREATE_BITFIELD_TYPE_MIN_MAX(name, size, int8_t, min_value, max_value, ##__VA_ARGS__)

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
    static constexpr ConfigurationHelper::HandleType k##name##ConfigHandle = CONFIG_GET_HANDLE_STR(_STRINGIFY(class_name) "." _STRINGIFY(name)); \
    static const IPAddress get##name() { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_GET); return IPAddress(*(uint32_t *)loadBinaryConfig(k##name##ConfigHandle, sizeof(uint32_t))); } \
    static void set##name(const IPAddress &address) { REGISTER_HANDLE_NAME(_STRINGIFY(class_name) "." _STRINGIFY(name), __DBG__TYPE_SET); storeBinaryConfig(k##name##ConfigHandle, &static_cast<uint32_t>(address), sizeof(uint32_t)); }

#define CREATE_IPV4_ADDRESS(name, default_value) \
    uint32_t name; \
    static constexpr uint32_t kDefaultValueFor_##name = default_value; \
    inline static void set_ipv4_##name(Type &obj, const IPAddress &addr) { \
        obj.name = static_cast<uint32_t>(addr); \
    } \
    inline static IPAddress get_ipv4_##name(const Type &obj) { \
        return obj.name; \
    } \
    inline static void set_bits_##name(Type &obj, const IPAddress &value) { \
        obj.name = static_cast<uint32_t>(value); \
    } \
    inline static IPAddress get_bits_##name(const Type &obj) { \
        return obj.name; \
    } \
    inline static FormUI::Validator::Hostname &addHostnameValidatorFor_##name(FormUI::Form::BaseForm &form, bool allowEmpty = true) { \
        if __CONSTEXPR17 (kDefaultValueFor_##name != 0) { \
            form.getLastField().getFormUI()->addItems(FormUI::PlaceHolder(kDefaultValueFor_##name)); \
        } \
        return form.addValidator(FormUI::Validator::Hostname(allowEmpty ? FormUI::HostnameValidator::AllowedType::EMPTY_OR_IP : FormUI::HostnameValidator::AllowedType::IPADDRESS)); \
    }

#define CREATE_FLOAT_FIELD(name, min_value, max_value, default_value) \
    float name; \
    static constexpr float kMinValueFor_##name = min_value; \
    static constexpr float kMaxValueFor_##name = max_value; \
    static constexpr float kDefaultValueFor_##name = default_value; \
    inline static void set_bits_##name(Type &obj, float value) { \
        obj.name = value; \
    } \
    inline static float get_bits_##name(const Type &obj) { \
        return obj.name; \
    } \
    inline static FormUI::Validator::Range &addRangeValidatorFor_##name(FormUI::Form::BaseForm &form, bool allowZero = false) { \
        if __CONSTEXPR17 (std::isnormal(kDefaultValueFor_##name)) { \
            form.getLastField().getFormUI()->addItems(FormUI::PlaceHolder(kDefaultValueFor_##name)); \
        } \
        return form.addValidator(FormUI::Validator::Range(static_cast<float>(kMinValueFor_##name), static_cast<float>(kMaxValueFor_##name), allowZero)); \
    } \
    inline static FormUI::Validator::Range &addRangeValidatorFor_##name(const String &message, FormUI::Form::BaseForm &form, bool allowZero = false) { \
        if __CONSTEXPR17 (std::isnormal(kDefaultValueFor_##name)) { \
            form.getLastField().getFormUI()->addItems(FormUI::PlaceHolder(kDefaultValueFor_##name)); \
        } \
        return form.addValidator(FormUI::Validator::Range(message, static_cast<float>(kMinValueFor_##name), static_cast<float>(kMaxValueFor_##name), allowZero)); \
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
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleStationsConfig);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameAlarm_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSerial2TCPConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameMqttConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSyslogConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameNtpClientConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSensorConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameFlagsConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameSoftAPConfig);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameBlindsConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameDimmerConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameClockConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNamePingConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameWeatherStationConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameRemoteConfig_t);
DECLARE_CONFIG_HANDLE_PROGMEM_STR(handleNameDisplayConfig_t);


#define CIF_DEBUG(...) __VA_ARGS__

#else

#define CIF_DEBUG(...)

#endif

namespace KFCConfigurationClasses {

    using HandleType = uint16_t;
    // using HandleType = ConfigurationHelper::HandleType;

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

    template<typename _ConfigType, HandleType handleArg CIF_DEBUG(, const char **handleName)>
    class ConfigGetterSetter {
    public:
        static constexpr uint16_t kConfigStructHandle = handleArg;
        using ConfigStructType = _ConfigType;

    public:;

        static ConfigStructType getConfig()
        {
            __CDBG_printf("getConfig=%04x size=%u name=%s", kConfigStructHandle, sizeof(ConfigStructType), *handleName);
            REGISTER_HANDLE_NAME(*handleName, __DBG__TYPE_GET);
            uint16_t length = sizeof(ConfigStructType);
            auto ptr = loadBinaryConfig(kConfigStructHandle, length);
            if (!ptr || length != sizeof(ConfigStructType)) {
                // invalid data, just initialize default object
                return ConfigStructType();
            }
            // return copy
            return *reinterpret_cast<const ConfigStructType *>(ptr);
        }

        static void setConfig(const ConfigStructType &params)
        {
            __CDBG_printf("setConfig=%04x size=%u name=%s", kConfigStructHandle, sizeof(ConfigStructType), *handleName);
            REGISTER_HANDLE_NAME(*handleName, __DBG__TYPE_SET);
            storeBinaryConfig(kConfigStructHandle, &params, sizeof(ConfigStructType));
        }

        static ConfigStructType & getWriteableConfig()
        {
            __CDBG_printf("getWriteableConfig=%04x name=%s size=%u", kConfigStructHandle, *handleName, sizeof(ConfigStructType));
            REGISTER_HANDLE_NAME(*handleName, __DBG__TYPE_W_GET);
            return *reinterpret_cast<ConfigStructType *>(loadWriteableBinaryConfig(kConfigStructHandle, sizeof(ConfigStructType)));
        }
    };

    template<HandleType _Handle, size_t _Capacity, size_t _MaxLength>
    class ConfigStringArray {
    public:
        static constexpr auto kHandle = _Handle;
        static constexpr auto kMaxLength = _MaxLength;
        static constexpr auto kMaxSize = (_Capacity * (_MaxLength + 1) + 3);
        static constexpr char kSeparator = -1;

        static_assert(kMaxSize < _BV(11), "max. size exceeded");

        struct SizeType {
            uint16_t count;
            uint16_t length;

            SizeType() : count(0), length(0) {}
        };

    public:
        String get(int index) const {
            auto data = _load();
            auto size = _size(data);
            return _get(index, data, size);
        }

        String operator[](int index) const {
            return get(index);
        }

        size_t size() const {
            return _size(_load()).count;
        }

        constexpr size_t capacity() const {
            return _Capacity;
        }

        constexpr size_t max_length() const {
            return _MaxLength;
        }

        void clear() {
            _store(emptyString);
            // auto data = _load();
            // __dump_binary(data, strlen(data), ~0U, PSTR("clear"));
        }

        bool append(const String &str) {
            return append(str.c_str(), str.length());
        }

        bool append(const __FlashStringHelper *str, int length = -1) {
            return append(reinterpret_cast<const char *>(str), length);
        }

        bool append(const char *str, int length = -1) {
            if (length == -1) {
                length = strlen_P(str);
            }
            length = std::min<int>(max_length(), length);
            // __LDBG_printf("append str=%*.*s len=%u", length, length, str, length);
            if (length == 0) {
                // empty strings are not stored
                return false;
            }
            auto data = _load();
            auto size = _size(data);
            if (size.count >= capacity()) {
                // capacity reached
                return false;
            }
            // increase buffer
            auto newData = loadWriteableStringConfig(kHandle, size.length + length + 2);
            // append new string
            memcpy_P(newData + size.length, str, length);
            // truncate string if max_length() was exceeded and add separator
            auto endPtr = newData + size.length + length;
            *endPtr++ = kSeparator;
            *endPtr = 0;
            // __dump_binary(newData, endPtr - newData + 1, ~0U, PSTR("append"));
            return true;
        }

    private:
        String _get(int index, const char *data, SizeType size) const {
            String tmp;
            if (index < size.count) {
                auto iterator = data;
                while(index > 0) {
                    iterator += _strlen(iterator) + 1;
                    index--;
                }
                tmp.concat(iterator, _strlen(iterator));
            }
            return tmp;
        }

        uint16_t _strlen(const char *data) const {
            auto ptr = data;
            while(*ptr != kSeparator && *ptr) {
                ptr++;
            }
            return ptr - data;
        }

        SizeType _size(const char *data) const {
            SizeType size;
            auto ptr = data;
            while(*ptr) {
                if (*ptr++ == kSeparator) {
                    size.count++;
                }
            }
            size.length = ptr - data;
            // __LDBG_printf("count=%u len=%u", size.count, size.length);
            return size;
        }

        void _store(const char *str) {
            KFCConfigurationClasses::storeStringConfig(kHandle, str);
        }

        void _store(const String &str) {
            KFCConfigurationClasses::storeStringConfig(kHandle, str);
        }

        const char *_load() const {
            return KFCConfigurationClasses::loadStringConfig(kHandle);
        }
    };


}

#if DEBUG_CONFIG_CLASS
#include <debug_helper_disable.h>
#endif
