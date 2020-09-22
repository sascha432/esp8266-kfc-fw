/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include <crc16.h>
#include <list>
#include <vector>
#include <Buffer.h>
#include <DumpBinary.h>
#include <type_traits>
#include <stl_ext/chunked_list.h>
#include <stl_ext/is_trivially_copyable.h>

#include "ConfigurationHelper.h"
#include "ConfigurationParameter.h"
#include "ConfigurationParameterT.h"
#include "EEPROMAccess.h"

#if defined(ESP8266)
#ifdef NO_GLOBAL_EEPROM
#include <EEPROM.h>
extern EEPROMClass EEPROM;
#endif
#endif

// #if defined(ESP32)
// #include <esp_partition.h>
// #endif

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

class Configuration {
public:
    using Handle_t = ConfigurationHelper::HandleType;
    using HandleType = ConfigurationHelper::HandleType;
    using TypeEnum_t = ConfigurationHelper::ParameterType;
    using ParameterType = ConfigurationHelper::ParameterType;
    using Param_t = ConfigurationHelper::ParameterInfo;
    using ParameterInfo = ConfigurationHelper::ParameterInfo;
    using ParameterHeaderType = ConfigurationHelper::ParameterHeaderType;
    using size_type = ConfigurationHelper::size_type;

#include <push_pack.h>

    typedef struct __attribute__((packed)) Header_t {
        uint32_t magic;             // 32
        uint16_t crc;               // 48
        uint32_t length : 12;       // 60
        uint32_t params : 10;       // 70
        uint16_t getParamsLength() const {
            return sizeof(ParameterHeaderType) * params;
        };
    } Header_t;

#include <pop_pack.h>

    typedef union {
        uint8_t headerBuffer[(sizeof(Header_t) + 4) & ~3]; // align size to dwords
        Header_t header;
    } HeaderAligned_t;

    // typedef std::list<ConfigurationParameter> ParameterList;
    typedef std::chunked_list<ConfigurationParameter, 6> ParameterList;

public:
    Configuration(uint16_t offset, uint16_t size);
    ~Configuration();

    // clear data and parameters
    void clear();

    // free data and discard modifications
    void discard();

    // free data and keep modifications
    void release();

    // read data from EEPROM
    bool read();

    // write data to EEPROM
    bool write();

    template <typename _Ta>
    ConfigurationParameter &getWritableParameter(HandleType handle, size_type maxLength = sizeof(_Ta)) {
        __LDBG_printf("handle=%04x max_len=%u", handle, maxLength);
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        if (param._param.isString()) {
            param.getString(*this, offset);
        }
        else {
            size_type length;
            auto ptr = param.getBinary(*this, length, offset);
            if (ptr) {
                __DBG_assert_printf(length == maxLength, "%04x: resizing binary blob=%u to %u maxLength=%u type=%u", handle, length, sizeof(_Ta), maxLength, ConfigurationParameter::getType<_Ta>());
            }
        }
        makeWriteable(param, maxLength);
        return param;
    }

    template <typename _Ta>
    ConfigurationParameter *getParameter(HandleType handle) {
        __LDBG_printf("handle=%04x", handle);
        size_type offset;
        auto iterator = _findParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        if (iterator == _params.end()) {
            return nullptr;
        }
        return &(*iterator);
    }

    template <typename _Ta>
    ConfigurationParameterT<_Ta> &getParameterT(HandleType handle) {
        __LDBG_printf("handle=%04x", handle);
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        return static_cast<ConfigurationParameterT<_Ta> &>(param);
    }

    void makeWriteable(ConfigurationParameter &param, size_type length);

    const char *getString(HandleType handle);
    char *getWriteableString(HandleType handle, size_type maxLength);

    // uint8_t for compatibility
    const uint8_t *getBinary(HandleType handle, size_type &length);
    const void *getBinaryV(HandleType handle, size_type &length) {
        return reinterpret_cast<const void *>(getBinary(handle, length));
    }
    void *getWriteableBinary(HandleType handle, size_type length);

    // PROGMEM safe
    inline void setString(HandleType handle, const char *str, size_type length, size_type maxLength) {
        _setString(handle, str, length, maxLength);
    }

    inline void setString(HandleType handle, const char *str, size_type maxLength) {
        if (!str) {
            _setString(handle, emptyString.c_str(), 0);
            return;
        }
        _setString(handle, str, (size_type)strlen_P(str), maxLength);
    }

    inline void setString(HandleType handle, const char *str) {
        if (!str) {
            _setString(handle, emptyString.c_str(), 0);
            return;
        }
        _setString(handle, str, (size_type)strlen_P(str));
    }

    inline void setString(HandleType handle, const __FlashStringHelper *fstr, size_type maxLength) {
        setString(handle, RFPSTR(fstr), maxLength);
    }

    inline void setString(HandleType handle, const __FlashStringHelper *fstr) {
        setString(handle, RFPSTR(fstr));
    }

    inline void setString(HandleType handle, const String &str, size_type maxLength) {
        _setString(handle, str.c_str(), (size_type)str.length(), maxLength);
    }

    inline void setString(HandleType handle, const String &str) {
        _setString(handle, str.c_str(), (size_type)str.length());
    }

    // PROGMEM safe
    void setBinary(HandleType handle, const void *data, size_type length);

    bool getBool(HandleType handle) {
        return get<uint8_t>(handle) ? true : false;
    }

    void setBool(HandleType handle, const bool flag) {
        set<bool>(handle, flag ? true : false);
    }

    template <typename _Ta>
    bool exists(HandleType handle) {
        uint16_t offset;
        return _findParam(ConfigurationParameter::getType<_Ta>(), handle, offset) != _params.end();
    }

    template <typename _Ta>
    // static_assert(std::is_<>);
    const _Ta get(HandleType handle) {
        __LDBG_printf("handle=%04x", handle);
        uint16_t offset;
        auto param = _findParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        if (param == _params.end()) {
            return _Ta();
        }
        size_type length;
        auto ptr = reinterpret_cast<const _Ta *>(param->getBinary(*this, length, offset));
        if (!ptr || length != sizeof(_Ta)) {
#if DEBUG_CONFIGURATION
            if (ptr && length != sizeof(_Ta)) {
                __LDBG_printf("size does not match, type=%s handle %04x (%s)",
                    (const char *)ConfigurationParameter::getTypeString(param->getType()),
                    handle,
                    ConfigurationHelper::getHandleName(handle)
                );
            }
#endif
            return _Ta();
        }
        return *ptr;
    }

    template <typename _Ta>
    _Ta &getWriteable(HandleType handle) {
        __LDBG_printf("handle=%04x", handle);
        auto &param = getWritableParameter<_Ta>(handle);
        return *reinterpret_cast<_Ta *>(param._getParam().data());
    }

    template <typename _Ta>
    const _Ta &set(HandleType handle, const _Ta &data) {
        __LDBG_printf("handle=%04x data=%p len=%u", handle, std::addressof(data), sizeof(_Ta));
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<_Ta>(), handle, offset);
        param.setData(*this, (const uint8_t *)&data, (size_type)sizeof(_Ta));
        return data;
    }

    void dump(Print &output, bool dirty = false, const String &name = String());
    bool isDirty() const;

    void exportAsJson(Print& output, const String &version);
    bool importJson(Stream& stream, HandleType *handles = nullptr);

// ------------------------------------------------------------------------
// Memory management

private:
    friend ConfigurationParameter;

private:
    void _setString(HandleType handle, const char *str, size_type length);
    void _setString(HandleType handle, const char *str, size_type length, size_type maxLength);

    // find a parameter, type can be _ANY or a specific type
    ParameterList::iterator _findParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset);
    ConfigurationParameter &_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset);

    // read parameter headers
    bool _readParams();
    uint16_t _readHeader(uint16_t offset, HeaderAligned_t &header);

private:
    ConfigurationHelper::EEPROMAccess _eeprom;
    ParameterList _params;
protected:
    uint32_t _readAccess;
private:
    uint16_t _offset;
    uint16_t _dataOffset;
    uint16_t _size;

// ------------------------------------------------------------------------
// EEPROM

public:
    void dumpEEPROM(Print &output, bool asByteArray = true, uint16_t offset = 0, uint16_t length = 0) {
        _eeprom.dump(output, asByteArray, offset, length);
    }

// ------------------------------------------------------------------------
// last access

public:
    // return time of last readData call that allocated memory
    unsigned long getLastReadAccess() const {
        return _readAccess;
    }
    void setLastReadAccess() {
        _readAccess = millis();
    }

// ------------------------------------------------------------------------
// DEBUG

#if DEBUG_CONFIGURATION

    static String __debugDumper(ConfigurationParameter &param, const __FlashStringHelper *data, size_t len) {
        return __debugDumper(param, (uint8_t *)data, len, true);
    }
    static String __debugDumper(ConfigurationParameter &param, const uint8_t *data, size_t len, bool progmem = false);

#endif

};

#if _MSC_VER
#pragma warning(pop)
#endif

#include <debug_helper_disable.h>
