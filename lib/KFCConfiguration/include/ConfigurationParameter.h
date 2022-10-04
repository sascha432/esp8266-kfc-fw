/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EEPROM.h>
#include <MicrosTimer.h>
#include <PrintString.h>
#include <type_traits>
#include <algorithm>
#include <stl_ext/type_traits.h>

#include "ConfigurationHelper.h"
// #include ".h"
#include "WriteableData.h"

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class Buffer;
class Configuration;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

namespace ConfigurationHelper {

    class ParameterInfo {
    public:
        ParameterInfo() : _header(0), _readable(nullptr) {
        }

        ParameterInfo(HandleType handle, ParameterType type) : _handle(handle), _type(static_cast<uint16_t>(type)), _length(0), _is_writeable(false), _readable(nullptr) {
        }

        ParameterInfo(ParameterHeaderType header) : _header(header), _readable(nullptr) {
        }

        inline uint8_t *data() {
            return _is_writeable ? _writeable->data() : _readable;
        }

        inline const uint8_t *data() const {
            return _is_writeable ? _writeable->data() : _readable;
        }

        inline const char *string() const {
            return reinterpret_cast<const char *>(data());
        }

        inline char *string() {
            return reinterpret_cast<char *>(data());
        }

        inline ParameterType type() const {
            return static_cast<ParameterType>(_type);
        }

        inline bool hasData() const {
            return _readable != nullptr;
        }

        inline bool isWriteable() const {
            __LDBG_assert_printf((_is_writeable == true && _writeable) || (_is_writeable == false), "is_writeable=%u _writeable=%p", _is_writeable, _writeable);
            return _is_writeable;
        }

        void resizeWriteable(size_type length, ConfigurationParameter &param, Configuration &conf);

        void setWriteable(WriteableData *writeable) {
            __DBG_assert_printf(_is_writeable == false, "is_writeable=%u writeable=%p _writeable=%p", _is_writeable, writeable, _writeable);
            _is_writeable = true;
            _writeable = writeable;
        }

        void setReadable(void *ptr) {
            __LDBG_assert_printf(_is_writeable == false, "is_writeable=%u", _is_writeable);
            _readable = (uint8_t *)ptr;
        }

        inline bool isString() const {
            return type() == ParameterType::STRING;
        }

        inline bool isBinary() const {
            return type() == ParameterType::BINARY;
        }

        // size of data(), for STRING it includes the NULL byte = strlen(data()) + 1
        inline size_type size() const {
            return _is_writeable ? sizeOf(_writeable->length()) : sizeOf(_length);
        };

        // size of data(), for STRING it is strlen(data())
        // returns 0 if it has no data
        inline size_type length() const {
            return _is_writeable ? _writeable->length() : _readable ? _length : 0;
        }

        inline size_type old_size() const {
            return sizeOf(_length);
        };

        inline size_type old_length() const {
            return _length;
        }

        // required capacity for length
        inline size_type sizeOf(size_type length) const {
            return isString() ? (length + 1) : length;
        };

        // return length of data stored in eeprom
        // 32 bit aligned
        inline size_type next_offset() const {
            return (size() + 3) & ~3;
        }

        inline size_type old_next_offset() const {
            return (old_size() + 3) & ~3;
        }

        inline size_type next_offset_unaligned() const {
            return size();
        }

        inline void setHandle(HandleType handle) {
            _handle = handle;
        }

        inline void setHandle(int handle) {
            _handle = static_cast<HandleType>(handle);
        }

        inline HandleType getHandle() const {
            return _handle;
        }

    public:
        struct {
            union {
                struct {
                    // crc16 of the config. name
                    HandleType _handle;
                    // type of data
                    uint16_t _type : 4;
                    // stores the length of the data currently stored in the EEPROM
                    uint16_t _length : 11;
                    // indicates that the parameter is writeable using the _writeable object
                    uint16_t _is_writeable: 1;
                };
                ParameterHeaderType _header;
            };
            union {
                // if not null, it points to read only data
                uint8_t *_readable;
                // if not null and _is_writeable is true, it provides writable access to the stored data
                WriteableData *_writeable;
            };
        };
    };

    static constexpr size_t kParameterInfoSize = sizeof(ParameterInfo);
    static_assert((sizeof(ParameterHeaderType) & 3) == 0, "not dword aligned");

}

class ConfigurationParameter {
public:
    using Handle_t = ConfigurationHelper::HandleType;
    using HandleType = ConfigurationHelper::HandleType;
    using TypeEnum_t = ConfigurationHelper::ParameterType;
    using ParameterType = ConfigurationHelper::ParameterType;
    using Param_t = ConfigurationHelper::ParameterInfo;
    using ParameterInfo = ConfigurationHelper::ParameterInfo;
    using WriteableData = ConfigurationHelper::WriteableData;
    using ParameterHeaderType = ConfigurationHelper::ParameterHeaderType;
    using size_type = ConfigurationHelper::size_type;


    ConfigurationParameter(const ConfigurationParameter &) = delete;
    ConfigurationParameter &operator=(const ConfigurationParameter &) = delete;

    ConfigurationParameter(const ParameterInfo &param) : _param(param) {}

    ConfigurationParameter(HandleType handle, ParameterType type) : _param(handle, type) {}

    ConfigurationParameter(ParameterHeaderType header) : _param(header) {}

    ~ConfigurationParameter() {
        if (_param.isWriteable()) {
            free(_param._writeable);
        }
        else if (_param._readable) {
            free(_param._readable);
        }
    }

    bool operator==(const HandleType handle) const {
        return _param.getHandle() == handle;
    }

    template <typename _Ta>
    static ParameterType constexpr getType() {
        return
            (std::is_same<_Ta, char *>::value ? ParameterType::STRING :
                ((std::is_same<_Ta, uint8_t *>::value || std::is_same<_Ta, void *>::value) ? ParameterType::BINARY :
                    (std::is_same<_Ta, float>::value ? ParameterType::FLOAT :
                        (std::is_same<_Ta, double>::value ? ParameterType::DOUBLE :
                            ((std::is_same<_Ta, char>::value || std::is_same<_Ta, signed char>::value || std::is_same<_Ta, unsigned char>::value) ? ParameterType::BYTE :
                                ((std::is_same<_Ta, int16_t>::value || std::is_same<_Ta, uint16_t>::value) ? ParameterType::WORD :
                                    ((std::is_same<_Ta, int32_t>::value || std::is_same<_Ta, uint32_t>::value) ? ParameterType::DWORD :
                                        ((std::is_same<_Ta, int64_t>::value || std::is_same<_Ta, uint64_t>::value) ? ParameterType::QWORD :
                                            ParameterType::BINARY
            ))))))));
    }

    // PROGMEM safe
    void setData(Configuration &conf, const uint8_t *data, size_type length);

    const char *getString(Configuration &conf, uint16_t offset);

    const uint8_t *getBinary(Configuration &conf, size_type &length, uint16_t offset);

    void dump(Print &output);
    void exportAsJson(Print& output);

    String toString() const;

    inline Handle_t getHandle() const {
        return _param.getHandle();
    }

    inline ParameterType getType() const {
        return _param.type();
    }

    inline bool isString() const {
        return _param.type() == ParameterType::STRING;
    }

    inline uint16_t getLength() const {
        return _param.length();
    }

    inline bool hasData() const {
        return _param.hasData();
    }

    inline bool isWriteable() const {
        return _param.isWriteable();
    }

    // compare data with EEPROM content if isWriteable()
    bool hasDataChanged(Configuration &conf) const;

    static const __FlashStringHelper *getTypeString(ParameterType type);

    inline static PGM_P getTypeString_P(ParameterType type) {
        return reinterpret_cast<PGM_P>(getTypeString(type));
    }


private:
    friend Configuration;
    friend WriteableData;

    uint16_t read(Configuration &conf, uint16_t offset);

    bool _readData(Configuration &conf, uint16_t offset);
    #if ESP8266
        bool _readDataTo(Configuration &conf, uint16_t offset, uint8_t *ptr) const;
    #endif
    void _makeWriteable(Configuration &conf, size_type length);

    // PROGMEM safe
    bool _compareData(const uint8_t *data, size_type length) const;

protected:
    ParameterInfo _param;

public:
    inline ParameterInfo &_getParam() {
        return _param;
    }

    inline ParameterInfo _getParam() const {
        return _param;
    }
};

namespace ConfigurationHelper {

    inline void ParameterInfo::resizeWriteable(size_type length, ConfigurationParameter &param, Configuration &conf)
    {
        __DBG_assert_printf(_is_writeable == true, "is_writeable=%u new_length=%u %s", _is_writeable, length, param.toString().c_str());
        _writeable->resize(length, param, conf);
    }

}

inline bool ConfigurationParameter::_compareData(const uint8_t *data, size_type length) const
{
    return _param.hasData() && _param.length() == length && memcmp_P(_param.data(), data, length) == 0;
}

inline const __FlashStringHelper *ConfigurationParameter::getTypeString(ParameterType type)
{
    switch (type) {
    case ParameterType::STRING:
        return F("STRING");
    case ParameterType::BINARY:
        return F("BINARY");
    case ParameterType::BYTE:
        return F("BYTE");
    case ParameterType::WORD:
        return F("WORD");
    case ParameterType::DWORD:
        return F("DWORD");
    case ParameterType::QWORD:
        return F("QWORD");
    case ParameterType::FLOAT:
        return F("FLOAT");
    case ParameterType::DOUBLE:
        return F("DOUBLE");
    case ParameterType::_ANY:
        return F("ANY");
    default:
        break;
    }
    return F("INVALID");
}

inline void ConfigurationParameter::_makeWriteable(Configuration &conf, size_type length)
{
    __LDBG_printf("%s length=%u is_writable=%u _writeable=%p ", toString().c_str(), length, _param.isWriteable(), _param._writeable);
    if (_param.isWriteable()) {
        _param.resizeWriteable(length, *this, conf);
    }
    else {
        _param.setWriteable(new WriteableData(length, *this, conf));
    }
}

inline String ConfigurationParameter::toString() const
{
    #if DEBUG_CONFIGURATION_GETHANDLE

        //return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u (%p:%p %u %u)"), ConfigurationHelper::getHandleName(_param.handle), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty, dataPtr, _info.data, dataOfs, dataLen);
        return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u next_ofs=%u"),
            ConfigurationHelper::getHandleName(_param.getHandle()),
            _param.getHandle(),
            _param.size(),
            _param.length(),
            _param.data(),
            getTypeString(_param.type()),
            _param.isWriteable(),
            _param.next_offset()
        );

    #else

        return PrintString(F("handle=%04x size=%u len=%u data=%p type=%s writable=%u next_ofs=%u"),
            _param.getHandle(), _param.size(), _param.length(), _param.data(), getTypeString(_param.type()), _param.isWriteable(), _param.next_offset()
        );

    #endif
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <debug_helper_disable.h>
