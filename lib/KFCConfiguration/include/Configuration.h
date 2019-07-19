/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_CONFIGURATION
#define DEBUG_CONFIGURATION 0
#endif

#include <Arduino_compat.h>
#include <crc16.h>
#include <map>
#include <vector>
#include <Buffer.h>
#include <DumpBinary.h>
#include "ConfigurationParameter.h"
#if !_WIN32
#include <EEPROM.h>
#endif

#ifdef NO_GLOBAL_EEPROM
#include <EEPROM.h>
extern EEPROMClass EEPROM;
#endif

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#ifndef CONFIG_MAGIC_DWORD
#define CONFIG_MAGIC_DWORD              0xfef31211
#endif

#define CONFIG_GET_HANDLE(name)         getHandle(_STRINGIFY(name))
#define _H(name)                        CONFIG_GET_HANDLE(name)

#define _H_GET(name)                    get<decltype(name)>(_H(name))
#define _H_W_GET(name)                  getWriteable<decltype(name)>(_H(name))
#define _H_SET(name, value)             set<decltype(name)>(_H(name), value)
#define _H_STR(name)                    getString(_H(name))
#define _H_W_STR(name)                  getWriteableString(_H(name), sizeof name)
#define _H_SET_STR(name, value)         setString(_H(name), value)
#define _H_GET_IP(name)                 get<uint32_t>(_H(name))
#define _H_W_GET_IP(name)               getWriteable<uint32_t>(_H(name))
#define _H_SET_IP(name, value)          set<uint32_t>(_H(name), (uint32_t)value)

#if DEBUG && DEBUG_CONFIGURATION
// store all configuration handle names for debugging. needs a lot RAM
#define DEBUG_GETHANDLE 1
#define CONSTEXPR_CONFIG_HANDLE_T ConfigurationParameter::Handle_t
uint16_t getHandle(const char *name);
const char *getHandleName(ConfigurationParameter::Handle_t crc);
#else
#define DEBUG_GETHANDLE 0
#define CONSTEXPR_CONFIG_HANDLE_T constexpr ConfigurationParameter::Handle_t
uint16_t constexpr getHandle(const char *name) {
    return constexpr_crc16_calc((const uint8_t *)name, constexpr_strlen(name));
}
const char *getHandleName(ConfigurationParameter::Handle_t crc);
#endif

#include <push_pack.h>

class Configuration {
public:
    typedef ConfigurationParameter::Handle_t Handle_t;

    typedef struct __attribute__packed__ {
        uint32_t magic;             // 32
        uint16_t crc;               // 48
        uint32_t length : 12;       // 60
        uint32_t params : 10;       // 70
    } Header_t;

    typedef std::vector<ConfigurationParameter> ParameterVector;
    typedef std::vector<ConfigurationParameter>::iterator ParameterVectorIterator;

    Configuration(uint16_t offset, uint16_t size);
    ~Configuration();

    void clear();

    // read map only, data is read on demand
    bool read();
    // write data to EEPROM
    bool write();

    // the reference returned cannot be used to access the parameter safely
    // use ConfigurationParameterFactory::get()
    template <typename T>
    ConfigurationParameterT<T> &getWritableParameter(Handle_t handle, uint16_t maxLength = 0) {
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<T>(), handle, offset);
        if (param.isWriteable(sizeof(T))) {
            return static_cast<ConfigurationParameterT<T>&>(param);
        }
        if (std::is_same<T, char *>::value) {
            char *ptr = (char *)param.getString(this, offset);
            if (ptr) {
                if (maxLength < param.getSize()) {
                    maxLength = param.getSize();
                }
            }
            char *data = (char *)calloc(maxLength + 1, 1);
            if (ptr) {
                strncpy(data, ptr, maxLength + 1);
            }
            param.setDataPtr((uint8_t *)data, maxLength);
        }
        else {
            uint16_t length;
            auto dataPtr = reinterpret_cast<T *>(param.getBinary(this, length, offset));
            if (!dataPtr || (dataPtr && length != sizeof(T))) {
                dataPtr = reinterpret_cast<T *>(param.allocate(sizeof(T))); // discard data if size does not fit
                param.setSize(sizeof(T));
                param.getParam().length = sizeof(T);
            }
        }
        return static_cast<ConfigurationParameterT<T>&>(param);
    }

    template <typename T>
    ConfigurationParameterT<T> *getParameter(Handle_t handle) {
        uint16_t offset;
        auto iterator = _findParam(ConfigurationParameter::getType<T>(), handle, offset);
        if (iterator == _params.end()) {
            return nullptr;
        }
        return static_cast<ConfigurationParameterT<T> *>(&(*iterator));
    }


    const char *getString(Handle_t handle);
    char *getWriteableString(Handle_t handle, uint16_t maxLength);
    const void *getBinary(Handle_t handle, uint16_t &length);

    void setString(Handle_t handle, const char *string);
    void setString(Handle_t handle, const String &string);
    void setString(Handle_t handle, const __FlashStringHelper *string);
    void setBinary(Handle_t handle, const void *data, uint16_t length);

    const bool getBool(Handle_t handle) {
        return get<uint8_t>(handle) ? true : false;
    }

    void setBool(Handle_t handle, const bool flag) {
        set<bool>(handle, flag ? true : false);
    }

    template <typename T>
    bool exists(Handle_t handle) {
        uint16_t offset;
        return _findParam(ConfigurationParameter::getType<T>(), handle, offset) != _params.end();
    }

    template <typename T>
    const T get(Handle_t handle) {
        uint16_t offset;
        auto param = _findParam(ConfigurationParameter::getType<T>(), handle, offset);
        if (param == _params.end()) {
            T empty;
            memset(&empty, 0, sizeof(empty));
            return empty;
        }
        uint16_t length;
        auto dataPtr = reinterpret_cast<const T *>(param->getBinary(this, length, offset));
        if (!dataPtr || length != sizeof(T)) {
#if DEBUG_CONFIGURATION
            if (dataPtr && length != sizeof(T)) {
                _debug_printf("Configuration::get<%s>() size does not match, handle %04x (%s)\n", ConfigurationParameter::getTypeString(param->getType()).c_str(), handle, getHandleName(handle));
            }
#endif
            T empty;
            memset(&empty, 0, sizeof(empty));
            return empty;
        }
        return *dataPtr;
    }

    template <typename T>
    T &getWriteable(Handle_t handle) {
        auto &param = getWritableParameter<T>(handle);
        param.setDirty(true);
        return param.get();
    }

    template <typename T>
    const T set(Handle_t handle, const T data) {
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<T>(), handle, offset);
        param.setData((const uint8_t *)&data, sizeof(T));
        return data;
    }

    ConfigurationParameter &getParameterByPosition(uint16_t position);
    uint16_t getPosition(const ConfigurationParameter *parameter) const;

    void dumpEEPROM(Print &output, bool asByteArray = true, uint16_t offset = 0, uint16_t length = 0);
    void dump(Print &output);
    void release();
    bool isDirty() const;

    inline void beginEEPROM() {
        if (!_eepromInitialized) {
            _eepromInitialized = true;
            _debug_printf("Configuration::beginEEPROM() = %d\n", _eepromSize);
            EEPROM.begin(_eepromSize);
        }
    }

    inline bool isEEPROMInitialized() {
        return _eepromInitialized;
    }

    void endEEPROM();
    void commitEEPROM();
    void getEEPROM(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size);

    // return time of last readData call that allocated memory
    inline unsigned long getLastReadAccess() {
        return _readAccess;
    }
    inline void setLastReadAccess() {
        _readAccess = millis();
    }

    virtual void setLastError(const String &error) {
    }
    virtual const char *getLastError() const {
        return nullptr;
    }


private:
    // find a parameter, type can be _ANY or a specific type
    ParameterVectorIterator _findParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset);

    ConfigurationParameter &_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset);

    // read parameter headers
    bool _readParams();

#if DEBUG_CONFIGURATION
    uint16_t calculateOffset(Handle_t handle);
#endif

private:
    uint16_t _offset;
    uint16_t _dataOffset;
    uint16_t _size;
    ParameterVector _params;
    bool _eepromInitialized;
    uint16_t _eepromSize;

protected:
    unsigned long _readAccess;
};

#include <pop_pack.h>

#include <debug_helper_enable.h>
