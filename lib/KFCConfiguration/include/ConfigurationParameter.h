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

// memory alignment for _info.data
#ifndef CONFIGURATION_PARAMETER_MEM_ADDR_ALIGNMENT
#define CONFIGURATION_PARAMETER_MEM_ADDR_ALIGNMENT          4
#endif

// use _info.data to store the settings. saves RAM but adds extra code
#ifndef CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE
#define CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE     1
#endif

class Buffer;
class Configuration;

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
        Handle_t handle;            // 16
        uint16_t type : 4;          // 4
        uint16_t length : 12;       // 16
    } Param_t;

    typedef struct __attribute__packed__ {
        uint8_t *data;              // 32           data or pointer to data
        uint16_t size : 12;         // 44           max. size of data
        uint16_t dirty : 1;         // 45           data has been changed and needs to be stored in EEPROM, implies copied=0
        uint16_t copied : 1;        // 46           data was copied from EEPROM, implies dirty=0
        uint16_t alloc : 1;         // 47           data is an allocated pointer
    } Info_t;

    ConfigurationParameter(const Param_t &param);

    // allocate memory and discard existing data
    uint8_t *allocate(uint16_t size);

    // free allocated memory
    void freeData();

#if CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE
    inline uint8_t *getDataPtr() const {
        return _info.alloc ? _info.data : (uint8_t *)&_info.data;
    }

    // return if the data pointer can be used as storage
#if CONFIGURATION_PARAMETER_MEM_ADDR_ALIGNMENT
    inline bool isAligned() const {
        return (((uint32_t)&_info.data) & (CONFIGURATION_PARAMETER_MEM_ADDR_ALIGNMENT - 1)) == 0;
    }
#else
    constexpr bool isAligned() {
        return true;
}
#endif

    // returns if the parameter fits into the _info data structure
    bool needsAlloc() const;

#else
    inline uint8_t *getDataPtr() const {
        return _info.data;
    }
#endif

    // object has data
    inline bool hasData() const {
        return _info.dirty || _info.copied;
    }

    // object has unsaved data
    inline bool isDirty() const {
        return _info.dirty;
    }

    // set dirty flag. also sets copied
    void setDirty(bool dirty);

    // returns if the specified size fits into the memory
    bool isWriteable(uint16_t size) const;

    // compare if the stored data is the same
    bool compareData(const uint8_t *data, uint16_t size) const;
    bool compareData_P(PGM_P data, uint16_t size) const;

    // release memory if not dirty
    void release();

    // update data to a pointer on the heap
    void setDataPtr(uint8_t *data, uint16_t size);

    uint16_t getSize() const;
    void setSize(uint16_t size);

    TypeEnum_t getType() const;

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

    void setType(TypeEnum_t type);
    static uint8_t getDefaultSize(TypeEnum_t type);

    void setData(const uint8_t *data, uint16_t size, bool addNulByte = false);
    void setData(const __FlashStringHelper *data, uint16_t size, bool addNulByte = false);
    void updateStringLength();

    const char *getString(Configuration *conf, uint16_t offset);
    uint8_t *getBinary(Configuration *conf, uint16_t &length, uint16_t offset);

    uint16_t read(Configuration *conf, uint16_t offset);

    void dump(Print &output);

    inline Handle_t getHandle() const {
        return _param.handle;
    }

    inline uint16_t getLength() const {
        return _param.length;
    }

    inline Param_t &getParam() {
        return _param;
    }

    inline const Param_t &getConstParam() const {
        return _param;
    }

    static const String getTypeString(TypeEnum_t type);

private:
    bool _readData(Configuration *conf, uint16_t offset, bool addNulByte = false);

protected:
    Info_t _info;
    Param_t _param;
};

template <typename T>
class ConfigurationParameterT: public ConfigurationParameter {
public:
    using ConfigurationParameter::ConfigurationParameter;

    ConfigurationParameterT<T>(const ConfigurationParameterT<T> &) = delete;

    inline T &get() {
        return *reinterpret_cast<T *>(getDataPtr());
    }
};

#include <pop_pack.h>
#include <debug_helper_enable.h>
