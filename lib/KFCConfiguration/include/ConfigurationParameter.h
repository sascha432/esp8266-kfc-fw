/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include <algorithm>

#include <push_pack.h>

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

class ConfigurationParameter {
public:
    typedef uint16_t Handle_t;

    typedef enum : uint8_t {
        STRING = 0,
        BINARY,
        BYTE,
        WORD,
        DWORD,
        QWORD,
        FLOAT,
        DOUBLE,
        _ANY = 0b1110,
        _INVALID = 0b1111
    } TypeEnum_t;

    typedef struct __attribute__packed__ {
        Handle_t handle;
        uint16_t type : 4;                              // type of data
        uint16_t length : 12;                           // stores the length of the data currently stored in the EEPROM
        TypeEnum_t getType() const {
            return static_cast<TypeEnum_t>(type);
        }
        bool isString() const {
            return type == STRING;
        }
        bool isBinary() const {
            return type == BINARY;
        }
        uint16_t getSize() const {
            return getSize(length);
        };
        uint16_t getSize(uint16_t len) const {
            return type == STRING ? len + 1 : len;
        };
    } Param_t;

    typedef struct __attribute__packed__ {
        uint8_t *data;                                  // pointer to data
        uint16_t size : 12;                             // max. size of data
        uint16_t dirty : 1;                             // data has been changed
        uint16_t ___reserved : 3;
    } Info_t;

    ConfigurationParameter(const ConfigurationParameter &) = delete;
    ConfigurationParameter(const Param_t &param);
    ConfigurationParameter(Handle_t handle, TypeEnum_t type);

    bool operator==(const ConfigurationParameter::Handle_t handle) const {
        return _param.handle == handle;
    }

    uint8_t* _allocate(Configuration *conf);
    void __release(Configuration *conf);
    void _free();

    template <typename T>
    static TypeEnum_t constexpr getType() {
        return std::is_same<T, char *>::value ? STRING :
            (std::is_same<T, float>::value ? FLOAT :
                (std::is_same<T, double>::value ? DOUBLE :
                    ((std::is_same<T, char>::value || std::is_same<T, signed char>::value || std::is_same<T, unsigned char>::value) ? BYTE :
                        ((std::is_same<T, int16_t>::value || std::is_same<T, uint16_t>::value) ? WORD :
                            ((std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value) ? DWORD :
                                ((std::is_same<T, int64_t>::value || std::is_same<T, uint64_t>::value) ? QWORD :
                                    BINARY
        ))))));
    }

    //void setType(TypeEnum_t type);
    static uint8_t getDefaultSize(TypeEnum_t type);

    // PROGMEM safe
    void setData(Configuration *conf, const uint8_t *data, uint16_t size);
    void updateStringLength();

    const char *getString(Configuration *conf, uint16_t offset);
    const uint8_t *getBinary(Configuration *conf, uint16_t &length, uint16_t offset);
    uint16_t read(Configuration *conf, uint16_t offset);

    void dump(Print &output);
    void exportAsJson(Print& output);

    String toString() const;

    Handle_t getHandle() const {
        return _param.handle;
    }

    uint16_t getLength() const {
        if (isDirty()) {
            if (_param.isString()) {
                return (uint16_t)strlen(reinterpret_cast<const char *>(_info.data));
            }
            return _info.size;
        }
        return _param.length;
    }

    uint16_t getSize() const {
        return _info.size;
    }

    bool hasData() const {
        return _info.data;
    }

    bool isDirty() const {
        return _info.dirty;
    }

    static const __FlashStringHelper *getTypeString(TypeEnum_t type);

private:
    friend Configuration;

    bool _readData(Configuration *conf, uint16_t offset);
    void _makeWriteable(Configuration *conf, uint16_t size);

    // PROGMEM safe
    bool _compareData(const uint8_t *data, uint16_t size) const;

protected:
    Info_t _info;
    Param_t _param;
};

template<class T>
class ConfigurationParameterT : public ConfigurationParameter
{
public:
    using ConfigurationParameter::ConfigurationParameter;

    operator bool() const {
        return _info.data != nullptr;
    }

    T &get() {
        return *reinterpret_cast<T *>(_info.data);
    }
    void set(const T &value) {
        *reinterpret_cast<T *>(_info.data) = value;
    }
};

template<>
class ConfigurationParameterT <char *> : public ConfigurationParameter
{
public:
    using ConfigurationParameter::ConfigurationParameter;

    operator bool() const {
        return _info.data != nullptr;
    }

    void set(const char *value) {
        strncpy(reinterpret_cast<char *>(_info.data), value, _param.length)[_param.length] = 0;
    }
    void set(const __FlashStringHelper *value) {
        strncpy_P(reinterpret_cast<char *>(_info.data), reinterpret_cast<PGM_P>(value), _param.length)[_param.length] = 0;
    }
    void set(const String &value) {
        strncpy(reinterpret_cast<char *>(_info.data), value.c_str(), _param.length)[_param.length] = 0;
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <pop_pack.h>
#include <debug_helper_disable.h>
