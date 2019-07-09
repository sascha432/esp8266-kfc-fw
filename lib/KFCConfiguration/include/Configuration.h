/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <crc16.h>
#include <map>
#include <vector>
#include <Buffer.h>
#include "ConfigurationParameter.h"

#ifdef NO_GLOBAL_EEPROM
#include <EEPROM.h>
extern EEPROMClass EEPROM;
#endif

#ifndef CONFIG_MAGIC_DWORD
#define CONFIG_MAGIC_DWORD              0xfef31211
#endif

#define CONFIG_STRINGIFY(s)             _CONFIG_STRINGIFY(s)
#define _CONFIG_STRINGIFY(s)            #s
#define CONFIG_GET_HANDLE(name)         getHandle(CONFIG_STRINGIFY(name))
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

#if DEBUG && 1
// needs a lot RAM
#define DEBUG_GETHANDLE 1
uint16_t getHandle(const char *name);
const char *getHandleName(uint16_t crc);
#else
#define DEBUG_GETHANDLE 0
uint16_t constexpr getHandle(const char *name) {
    return constexpr_crc16_calc((const uint8_t *)name, constexpr_strlen(name));
}
const char *getHandleName(uint16_t crc);
#endif

#if _WIN32 || _WIN64
#define CONFIGURATION_PACKED
#pragma pack(push, 1)
#else
#define CONFIGURATION_PACKED   __attribute__((packed))
#endif

class Configuration {
public:
    typedef struct CONFIGURATION_PACKED {
        uint32_t magic;             // 32
        uint16_t crc;               // 48
        uint32_t length : 12;       // 60
        uint32_t params : 10;       // 70
    } Header_t;

    typedef std::vector<ConfigurationParameter> ParameterVector;

    Configuration(uint16_t offset, uint16_t size);
    ~Configuration();

    void clear();

    // read map only, data is read on demand
    bool read();
    // write data to EEPROM
    bool write();

    const char *getString(uint16_t handle);
    char *getWriteableString(uint16_t handle, uint16_t maxLength);
    const void *getBinary(uint16_t handle, uint16_t &length);

    void setString(uint16_t handle, const char *string);
    void setString(uint16_t handle, const String &string);
    void setString(uint16_t handle, const __FlashStringHelper *string);
    void setBinary(uint16_t handle, const void *data, uint16_t length);

    const bool getBool(uint16_t handle) {
        return get<uint8_t>(handle) ? true : false;
    }
    template<typename T>
    const T &getBinary(uint16_t handle, T &ptr, uint16_t &length) {
        return ptr = (T)getBinary(handle, length);
    }
    template<typename T>
    const T *getStruct(uint16_t handle, T &data) {
        uint16_t length;
        const void *dataPtr = getBinary(handle, length);
        if (!dataPtr || length != sizeof(T)) {
#if DEBUG
            if (dataPtr && length != sizeof(T)) {
                debug_printf("Configuration::getStruct() size does not match, handle %04x (%s)\n", handle, getHandleName(handle));
            }
#endif
            return nullptr;
        }
        memcpy(&data, dataPtr, length);
        return reinterpret_cast<const T *>(dataPtr);
    }
    template<typename T>
    const T get(uint16_t handle) {
        auto param = _findParam(ConfigurationParameter::getType<T>(), handle);
        if (!param) {
            T empty;
            memset(&empty, 0, sizeof(empty));
            return empty;
        }
        uint16_t length = sizeof(T);
        T *dataPtr = (T *)param->getBinary(length, _offset);
        if (!dataPtr || length != sizeof(T)) {
#if DEBUG
            if (dataPtr && length != sizeof(T)) {
                debug_printf("Configuration::get<%s>() size does not match, handle %04x (%s)\n", ConfigurationParameter::getTypeString(param->getType()).c_str(), handle, getHandleName(handle));
            }
#endif
            T empty;
            memset(&empty, 0, sizeof(empty));
            return empty;
        }
        return (const T)*dataPtr;
    }
    template<typename T>
    T &getWriteable(uint16_t handle) {
        auto &param = _getParam(ConfigurationParameter::getType<T>(), handle);
        if (param.isWriteable(sizeof(T))) {
            return (T &)*((T *)param.getDataPtr());
        }
        uint16_t length;
        T *ptr = (T *)param.getBinary(length, _offset);
        if (ptr && length != sizeof(T)) {
            param.free();
            ptr = nullptr;
        }
        T *data = (T *)calloc(sizeof(T), 1);
        if (ptr) {
            memcpy(data, ptr, sizeof(T));
            param.free();
        }
        param.setDataPtr((uint8_t *)data, sizeof(T));
        param.setDirty(true);
        return (T &)*data;
    }

    void setBool(uint16_t handle, const bool flag) {
        set<bool>(handle, flag ? true : false);
    }
    template<typename T>
    const T set(uint16_t handle, const T data) {
        auto &param = _getParam(ConfigurationParameter::getType<T>(), handle);
        param.setData((const uint8_t *)&data, sizeof(T));
        return data;
    }

    void dumpEEPROM(Print &output);
    void dump(Print &output);
    void release();
    bool isDirty() const;

    static void beginEEPROM();
    static void endEEPROM();
    static void commitEEPROM();
    static bool isEEPROMInitialized();
    static uint16_t getEEPROMSize();
#if ESP8266
    static void copyEEPROM(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size);
#endif
    static unsigned long getLastReadAccess();

private:
    ConfigurationParameter *_findParam(ConfigurationParameter::TypeEnum_t type, uint16_t handle);
    ConfigurationParameter &_getParam(ConfigurationParameter::TypeEnum_t type, uint16_t handle);
    bool _readParams();
    uint16_t calculateOffset(uint16_t handle);

private:
    uint16_t _offset;
    uint16_t _dataOffset;
    uint16_t _size;
    ParameterVector _params;
    static bool _eepromInitialized;
    static uint16_t _eepromSize;

protected:
    static unsigned long _readAccess;
};

#if _WIN32 || _WIN64
#pragma pack(pop)
#endif
