/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_CONFIGURATION
#define DEBUG_CONFIGURATION                 1
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
#include "ConfigurationParameter.h"
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

// compare direct reads vs. EEPROM class
#ifndef DEBUG_CONFIGURATION_VERIFY_DIRECT_EEPROM_READ
#define DEBUG_CONFIGURATION_VERIFY_DIRECT_EEPROM_READ               DEBUG_CONFIGURATION
#endif

#ifndef CONFIG_MAGIC_DWORD
#define CONFIG_MAGIC_DWORD              0xfef31214
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
ConfigurationParameter::Handle_t getHandle(const char *name);
const char *getHandleName(ConfigurationParameter::Handle_t crc);
#else
#define DEBUG_GETHANDLE 0
#define CONSTEXPR_CONFIG_HANDLE_T constexpr ConfigurationParameter::Handle_t
ConfigurationParameter::Handle_t constexpr getHandle(const char *name) {
    return constexpr_crc16_calc((const uint8_t *)name, constexpr_strlen(name));
}
const char *getHandleName(ConfigurationParameter::Handle_t crc);
#endif

#include <push_pack.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

namespace ConfigurationHelper {

    // memory pool for configuration data
    class Pool {
    public:
        Pool(uint16_t size);
        ~Pool();
        Pool(const Pool &pool) = delete;
        Pool(Pool &&pool) noexcept;
        Pool &operator=(Pool &&pool) noexcept;

        // initialize memory pool
        void init();

        // returns true if enough space for length bytes
        bool space(uint16_t length) const;

        uint16_t available() const;
        uint16_t size() const;

        // number of pointers
        uint8_t count() const;

        uint8_t *allocate(uint16_t length);
        void release(const void *ptr);

        // returns length dword aligned
        static uint16_t align(uint16_t length);

        // returns true if ptr belongs to this pool
        bool hasPtr(const void *ptr) const;

        void *getPtr() const;

    private:
        uint8_t *_ptr;
        uint16_t _length;
        uint16_t _size;
        uint8_t _count;
    };

    // direct access to EEPROM if supported
    class EEPROMAccess {
    public:
        EEPROMAccess(bool isInitialized, uint16_t size) : _isInitialized(isInitialized), _size(size) {
//#if defined(ESP32)
//            _partition = nullptr;
//#endif
        }

        operator bool() const {
            return _isInitialized;
        }

        uint16_t getSize() const {
            return _size;
        }

        void begin(uint16_t size = 0);
        void end();
        void commit();

        void read(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size);
        void dump(Print &output, bool asByteArray = true, uint16_t offset = 0, uint16_t length = 0);

        uint8_t *getDataPtr() {
#if DEBUG_CONFIGURATION
            if (!_isInitialized) {
                __debugbreak_and_panic_printf_P(PSTR("EEPROM is not initialized\n"));
            }
#endif
            return EEPROM.getDataPtr();
        }

        const uint8_t *getConstDataPtr() const {
#if DEBUG_CONFIGURATION
            if (!_isInitialized) {
                __debugbreak_and_panic_printf_P(PSTR("EEPROM is not initialized\n"));
            }
#endif
#if defined(ESP32)
            return EEPROM.getDataPtr();
#else
            return EEPROM.getConstDataPtr();
#endif
        }

        template<typename T>
        inline T &get(int const address, T &t) {
#if DEBUG_CONFIGURATION
            if (!_isInitialized) {
                __debugbreak_and_panic_printf_P(PSTR("EEPROM is not initialized\n"));
            }
            if (address + sizeof(T) > _size) {
                __debugbreak_and_panic_printf_P(PSTR("address=%u size=%u eeprom_size=%u\n"), address, sizeof(T), _size);
            }
#endif
            return EEPROM.get(address, t);
        }

    private:
        bool _isInitialized;
        uint16_t _size;
 //#if defined(ESP32)
 //       const esp_partition_t *_partition;
 //#endif
    };

};

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
    ConfigurationParameter &getWritableParameter(Handle_t handle, uint16_t maxLength = 0) {
        uint16_t offset;
        auto &param = _getOrCreateParam(ConfigurationParameter::getType<T>(), handle, offset);
        if (param._param.isString()) {
            param.getString(this, offset);
        }
        else {
            uint16_t length;
            auto ptr = param.getBinary(this, length, offset);
            if (ptr && length != sizeof(T)) {
                __debugbreak_and_panic_printf_P(PSTR("%s size does not match len=%u size=%u\n"), param.toString().c_str(), length, sizeof(T));
                //_release(ptr);
                //ptr = nullptr;
            }
        }
        makeWriteable(param, maxLength);
        return param;

        //_debug_printf_P(PSTR("type=%s, max_len=%u\n"), ConfigurationParameter::getTypeString(param._param.getType()), maxLength);
        //if (param._param.isString()) {
        //    auto ptr = param.getString(this, offset);
        //    _writeAllocate(param, maxLength);
        //    if (ptr) {
        //        strncpy(reinterpret_cast<char *>(param._info.data), ptr, maxLength)[maxLength] = 0;
        //        _release(ptr);
        //    }
        //}
        //else {
        //    uint16_t length;
        //    auto ptr = param.getBinary(this, length, offset);
        //    if (length != sizeof(T)) {
        //        _release(ptr);
        //        ptr = nullptr;
        //    }
        //    _writeAllocate(param, sizeof(T));
        //    if (ptr) {
        //        memcpy(param._info.data, ptr, sizeof(T));
        //        _release(ptr);
        //    }
        //}
        //param._info.dirty = 1;
        //_debug_printf_P(PSTR("param size=%u len=%u\n"), param._info.size, param.getLength());
        //return param;
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

    const char *getString(Handle_t handle, ConfigurationParameter **paramPtr = nullptr);
    char *getWriteableString(Handle_t handle, uint16_t maxLength);

    const uint8_t *getBinary(Handle_t handle, uint16_t &length, ConfigurationParameter **paramPtr = nullptr);

    void setString(Handle_t handle, const char *string);
    void setString(Handle_t handle, const String &string);
    void setString(Handle_t handle, const __FlashStringHelper *string);
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
                _debug_printf_P(PSTR("size does not match, handle %04x (%s)\n"), ConfigurationParameter::getTypeString(param->_param.getType()), handle, getHandleName(handle));
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
    const T set(Handle_t handle, const T data) {
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

    void _writeAllocate(ConfigurationParameter &param, uint16_t size);
    uint8_t *_allocate(uint16_t size, PoolVector *pool = nullptr);
    void _release(const void *ptr);
    Pool *_getPool(const void *ptr);
    void _shrinkStorage();

private:
    // find a parameter, type can be _ANY or a specific type
    ParameterList::iterator _findParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset);
    ConfigurationParameter &_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset);

    // read parameter headers
    bool _readParams();

private:
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

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <pop_pack.h>

#include <debug_helper_disable.h>
