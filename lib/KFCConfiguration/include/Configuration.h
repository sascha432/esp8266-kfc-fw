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

    struct Header {
        uint32_t magic;
        uint32_t version;
        uint32_t crc;
        uint16_t length;
        uint16_t params;
        // uint32_t length : 12;       // 60
        // uint32_t params : 10;       // 70

        uint16_t getParamsLength() const {
            return sizeof(ParameterHeaderType) * params;
        };

        Header(uint32_t _version = 0, uint32_t _crc = ~0U, uint16_t _length = 0, uint16_t _params = 0) : magic(CONFIG_MAGIC_DWORD), version(_version), crc(_crc), length(_length), params(_params) {}
    };

    static_assert((sizeof(Header) & 3) == 0, "not dword aligned");

    // iterators must not change when the list is modified
    // ParameterList = std::list<ConfigurationParameter>;
    using ParameterList = stdex::chunked_list<ConfigurationParameter, 6>;

    static constexpr size_t ParameterListChunkSize = ParameterList::chunk_element_count * 8 + sizeof(uint32_t); ; //ParameterList::chunk_size;

    static constexpr uint16_t kOffset = 0;
    static_assert((kOffset & 3) == 0, "not dword aligned");

    static constexpr uint16_t kParamsOffset = kOffset + sizeof(Header);
    static_assert((kParamsOffset & 3) == 0, "not dword aligned");

    constexpr uint16_t getDataOffset(uint16_t numParams) const {
        return kParamsOffset + (sizeof(ParameterHeaderType) * numParams);
    }


public:
    Configuration(uint16_t size);
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

    // ------------------------------------------------------------------------
    // EEPROM

protected:
    ConfigurationHelper::EEPROMAccess _eeprom;
    ParameterList _params;
    uint32_t _readAccess;
    // uint16_t _dataOffset;
    uint16_t _size;

    // constexpr uint16_t getDataOffset() const {
    //     return _offset + sizeof(Header);
    // }

    // inline void resetDataOffset() {
    //     _dataOffset = getDataOffset();
    // }

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

    static String __debugDumper(ConfigurationParameter &param, const uint8_t *data, size_t len);

    static String __debugDumper(ConfigurationParameter &param, const __FlashStringHelper *data, size_t len) {
        return __debugDumper(param, reinterpret_cast<const uint8_t *>(data), len);
    }

};

#if _MSC_VER
#pragma warning(pop)
#endif

#include <debug_helper_disable.h>
