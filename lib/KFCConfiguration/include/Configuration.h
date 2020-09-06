/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_CONFIGURATION
#define DEBUG_CONFIGURATION                 0
#endif

#include <Arduino_compat.h>
#include <PrintString.h>
#include <crc16.h>
#include <list>
#include <chunked_list.h>
#include <vector>
#include <Buffer.h>
#include <EEPROM.h>
#include <DumpBinary.h>
#if !_WIN32
#include <EEPROM.h>
#endif

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

// do not run garbage collector if pool count <= GARBAGE_COLLECTOR_MIN_POOL
#ifndef GARBAGE_COLLECTOR_MIN_POOL
#if DEBUG_CONFIGURATION
#define GARBAGE_COLLECTOR_MIN_POOL                                  0
#else
#define GARBAGE_COLLECTOR_MIN_POOL                                  2
#endif
#endif

// create custom pool if a parameter exceeds this length
#ifndef CONFIG_POOL_MAX_SIZE
#define CONFIG_POOL_MAX_SIZE                                        96
#endif

// 16, 32, 48, 64, 80, 96 byte...
#ifndef CONFIG_POOL_SIZE
#define CONFIG_POOL_SIZE                                            std::min((size_t)CONFIG_POOL_MAX_SIZE, (size_t)(POOL_COUNT * 16))
#endif


// compare direct reads vs. EEPROM class
#ifndef DEBUG_CONFIGURATION_VERIFY_DIRECT_EEPROM_READ
#define DEBUG_CONFIGURATION_VERIFY_DIRECT_EEPROM_READ               DEBUG_CONFIGURATION
#endif

#ifndef CONFIG_MAGIC_DWORD
#define CONFIG_MAGIC_DWORD                                          0xfef31214
#endif

#define CONFIG_GET_HANDLE(name)                                     __get_constexpr_getHandle(_STRINGIFY(name))
#define CONFIG_GET_HANDLE_STR(name)                                 __get_constexpr_getHandle(name)
#define _H(name)                                                    CONFIG_GET_HANDLE(name)
#define _HS(name)                                                   __get_constexpr_getHandle(name)

#if DEBUG_CONFIGURATION_GETHANDLE

#define _H_GET(name)                                                get<decltype(name)>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_GET))
#define _H_W_GET(name)                                              getWriteable<decltype(name)>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_W_GET))
#define _H_SET(name, value)                                         set<decltype(name)>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_SET), value)
#define _H_STR(name)                                                getString(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_GET))
#define _H_W_STR(name)                                              getWriteableString(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_W_GET), max_len)
#define _H_SET_STR(name, value, max_len)                            setString(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_SET), value)
#define _H_GET_IP(name)                                             get<uint32_t>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_GET))
#define _H_W_GET_IP(name)                                           getWriteable<uint32_t>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_W_GET))
#define _H_SET_IP(name, value)                                      set<uint32_t>(__DBG__registerHandleName(PSTR(_STRINGIFY(name)), __DBG__TYPE_SET), (uint32_t)value)

#else

#define _H_GET(name)                                                get<decltype(name)>(_H(name))
#define _H_W_GET(name)                                              getWriteable<decltype(name)>(_H(name))
#define _H_SET(name, value)                                         set<decltype(name)>(_H(name), value)
#define _H_STR(name)                                                getString(_H(name))
#define _H_W_STR(name)                                              getWriteableString(_H(name), max_len)
#define _H_SET_STR(name, value, max_len)                            setString(_H(name), value)
#define _H_GET_IP(name)                                             get<uint32_t>(_H(name))
#define _H_W_GET_IP(name)                                           getWriteable<uint32_t>(_H(name))
#define _H_SET_IP(name, value)                                      set<uint32_t>(_H(name), (uint32_t)value)

#endif

#if DEBUG_CONFIGURATION_GETHANDLE

// log usage to file, 0 = disable
#ifndef DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL
#define DEBUG_CONFIGURATION_GETHANDLE_LOG_INTERVAL                  5   // minutes
#endif

// store all configuration handle names for debugging. needs a lot RAM
#define __DBG__registerHandleName(name, type)                       ConfigurationHelper::registerHandleName(name, type)
#define __DBG__checkIfHandleExists(type, handle)                    if (!registerHandleExists(handle)) { __DBG_printf("handle=%04x no name registered, type=%s", handle, PSTR(type)); }
#define __DBG__addFlashReadSize(handle, size)                       ConfigurationHelper::addFlashUsage(handle, size, 0)
#define __DBG__addFlashWriteSize(handle, size)                      ConfigurationHelper::addFlashUsage(handle, 0, size)

#else

#define __DBG__registerHandleName(...)
#define __DBG__checkIfHandleExists(...)
#define __DBG__addFlashReadSize(...)
#define __DBG__addFlashWriteSize(...)

#endif
#define __get_constexpr_getHandle(name)                             constexpr_crc16_update(name, constexpr_strlen(name))

#include <push_pack.h>

#if _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

#include "ConfigurationParameter.h"
#include "ConfigurationHelper.h"

class Configuration {
public:
    typedef ConfigurationParameter::Handle_t Handle_t;

    typedef struct __attribute__packed__ {
        uint32_t magic;             // 32
        uint16_t crc;               // 48
        uint32_t length : 12;       // 60
        uint32_t params : 10;       // 70
        uint16_t getParamsLength() const {
            return sizeof(ConfigurationParameter::Param_t) * params;
        };
    } Header_t;

    typedef union __attribute__packed__ {
        uint8_t headerBuffer[(sizeof(Header_t) + 7) & ~7]; // align size to dwords
        Header_t header;
    } HeaderAligned_t;

    // typedef std::list<ConfigurationParameter> ParameterList;
    typedef xtra_containers::chunked_list<ConfigurationParameter, 6> ParameterList;

public:
    Configuration(uint16_t offset, uint16_t size);
    ~Configuration();

    // discard and clear parameter table
    void clear();

    // release parameters and discard any changes
    void discard();

    // release read only parameters
    void release();

    // read parameters without data
    bool read();

    // write dirty parameters to EEPROM
    bool write();

    template <typename T>
    ConfigurationParameter &getWritableParameter(Handle_t handle, uint16_t maxLength = sizeof(T)) {
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<T>(), handle, offset);
        if (param._param.isString()) {
            param.getString(this, offset);
        }
        else {
            uint16_t length;
            auto ptr = param.getBinary(this, length, offset);
            if (ptr) {
                if (length != maxLength) {
                    __DBG_printf("%04x: resizing binary blob=%u to %u maxLength=%u type=%u", handle, length, sizeof(T), maxLength, ConfigurationParameter::getType<T>());
                }
            }
        }
        makeWriteable(param, maxLength);
        return param;
    }

    template <typename T>
    ConfigurationParameter *getParameter(Handle_t handle) {
        uint16_t offset;
        auto iterator = _findParam(ConfigurationParameter::getType<T>(), handle, offset);
        if (iterator == _params.end()) {
            return nullptr;
        }
        return &(*iterator);
    }

    template <typename T>
    ConfigurationParameterT<T> &getParameterT(Handle_t handle) {
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<T>(), handle, offset);
        return static_cast<ConfigurationParameterT<T> &>(param);
    }

    void makeWriteable(ConfigurationParameter &param, uint16_t size = 0);

    const char *getString(Handle_t handle);
    char *getWriteableString(Handle_t handle, uint16_t maxLength);

    // uint8_t for compatibility
    const uint8_t *getBinary(Handle_t handle, uint16_t &length);
    const void *getBinaryV(Handle_t handle, uint16_t &length) {
        return reinterpret_cast<const void *>(getBinary(handle, length));
    }
    void *getWriteableBinary(Handle_t handle, uint16_t length);

    // PROGMEM safe
    void setString(Handle_t handle, const char *str, uint16_t length);
    void setString(Handle_t handle, const char *str) {
        if (!str) {
            __DBG_printf("%04x: data=nullptr", handle);
            setString(handle, emptyString.c_str(), 0);
        }
        else {
            setString(handle, str, (uint16_t)strlen_P(str));
        }
    }
    void setString(Handle_t handle, const __FlashStringHelper *fstr) {
        setString(handle, RFPSTR(fstr));
    }
    void setString(Handle_t handle, const String &str) {
        setString(handle, str.c_str(), (uint16_t)str.length());
    }
    // PROGMEM safe
    void setBinary(Handle_t handle, const void *data, uint16_t length);

    bool getBool(Handle_t handle) {
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
            return T();
        }
        uint16_t length;
        auto ptr = reinterpret_cast<const T *>(param->getBinary(this, length, offset));
        if (!ptr || length != sizeof(T)) {
#if DEBUG_CONFIGURATION
            if (ptr && length != sizeof(T)) {
                __LDBG_printf("size does not match, type=%s handle %04x (%s)",
                    (const char *)ConfigurationParameter::getTypeString(param->_param.getType()),
                    handle,
                    ConfigurationHelper::getHandleName(handle)
                );
            }
#endif
            return T();
        }
        return *ptr;
    }

    template <typename T>
    T &getWriteable(Handle_t handle) {
        auto &param = getWritableParameter<T>(handle);
        return *reinterpret_cast<T *>(param._info.data);
    }

    template <typename T>
    const T &set(Handle_t handle, const T &data) {
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<T>(), handle, offset);
        param.setData(this, (const uint8_t *)&data, sizeof(T));
        return data;
    }

    void dump(Print &output, bool dirty = false, const String &name = String());
    bool isDirty() const;

    void exportAsJson(Print& output, const String &version);
    bool importJson(Stream& stream, uint16_t *handles = nullptr);

// ------------------------------------------------------------------------
// Memory management

private:
    typedef ConfigurationHelper::Pool Pool;
    typedef std::vector<Pool> PoolVector;

#if DEBUG_CONFIGURATION
    void _dumpPool(PoolVector &pool);
#else
    inline void _dumpPool(PoolVector &pool) {}
#endif

    PoolVector _storage;

private:
    friend ConfigurationParameter;

    uint8_t *_allocate(uint16_t size, PoolVector *pool = nullptr);
    void __release(const void *ptr);
    Pool *_getPool(const void *ptr);
    Pool *_findPool(uint16_t length, PoolVector *poolVector) const;
    void _shrinkStorage();
    void _freeAll();

private:
    // find a parameter, type can be _ANY or a specific type
    ParameterList::iterator _findParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset);
    ConfigurationParameter &_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset);

    // read parameter headers
    bool _readParams();
    uint16_t _readHeader(uint16_t offset, HeaderAligned_t &header);

private:
    static constexpr uint16_t kInvalidOffset = ~0;

    uint16_t _offset;
    uint16_t _dataOffset;
    uint16_t _size;
    ParameterList _params;

// ------------------------------------------------------------------------
// EEPROM

public:
    void dumpEEPROM(Print &output, bool asByteArray = true, uint16_t offset = 0, uint16_t length = 0) {
        _eeprom.dump(output, asByteArray, offset, length);
    }

private:
    ConfigurationHelper::EEPROMAccess _eeprom;

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

protected:
    unsigned long _readAccess;


// ------------------------------------------------------------------------
// DEBUG

#if DEBUG_CONFIGURATION

    uint16_t calculateOffset(Handle_t handle) const;

    static String __debugDumper(ConfigurationParameter &param, const __FlashStringHelper *data, size_t len) {
        return __debugDumper(param, (uint8_t *)data, len, true);
    }
    static String __debugDumper(ConfigurationParameter &param, const uint8_t *data, size_t len, bool progmem = false);

#endif
};

#if _MSC_VER
#pragma warning(pop)
#endif

#include <pop_pack.h>
#include <debug_helper_disable.h>
