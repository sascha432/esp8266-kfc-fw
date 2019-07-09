/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <type_traits>

#if _WIN32 || _WIN64
#define CONFIGURATION_PACKED
#pragma pack(push, 1)
#else
#define CONFIGURATION_PACKED   __attribute__((packed))
#endif

class Buffer;

class ConfigurationParameter {
public:
    typedef enum : uint8_t {
        STRING = 0,
        BINARY,
        BYTE,
        WORD,
        DWORD,
        FLOAT,
        _INVALID = 0b1111
    } TypeEnum_t;

    typedef struct CONFIGURATION_PACKED {
        uint16_t handle;            // 16
        uint16_t type : 4;          // 4
        uint16_t length : 12;       // 16
    } Param_t;

    typedef struct CONFIGURATION_PACKED {
        uint8_t *data;              // 32           data or ptr to data
        uint16_t size : 12;         // 44           max. size of data
        uint16_t dirty : 1;         // 45           data has been changed and needs to be stored in EEPROM
        uint16_t copied : 1;        // 46           data was copied from EEPROM
        uint16_t alloc : 1;         // 47           data is an allocated ptr
    } Info_t;

    ConfigurationParameter(const Param_t &param);

    uint8_t *allocate(uint16_t size);
    void free();
    uint8_t *getDataPtr();

    inline bool hasData() const;
    bool isDirty() const;
    void setDirty(bool dirty);
    bool isWriteable(uint16_t size) const;
    bool needsAlloc() const;

    uint16_t getSize() const;
    void setSize(uint16_t size);

    TypeEnum_t getType() const;
    template<typename T>
    static TypeEnum_t constexpr getType() {
        return std::is_same<T, float>::value ? FLOAT : (sizeof(T) == sizeof(uint8_t) ? BYTE : (sizeof(T) == sizeof(uint16_t) ? WORD : (sizeof(T) == sizeof(uint32_t) ? DWORD : BINARY)));
    }
    void setType(TypeEnum_t type);
    static uint8_t getDefaultSize(TypeEnum_t type);

    void setData(const uint8_t *data, uint16_t size, bool addNulByte = false);
    void setData(const __FlashStringHelper *data, uint16_t size, bool addNulByte = false);
    void updateStringLength();
    void setDataPtr(uint8_t *data, uint16_t size);

    const char *getString(uint16_t offset);
    const uint8_t *getBinary(uint16_t &length, uint16_t offset);

    uint16_t read(uint16_t offset);
    bool write(Buffer &buffer);

    void dump(Print &output);
    void release();

    Param_t &getParam();

    static const String getTypeString(TypeEnum_t type);

private:
    bool _readData(uint16_t offset, bool addNulByte = false);

private:
    Param_t _param;
    Info_t _info;
};

#if _WIN32 || _WIN64
#pragma pack(pop)
#endif
