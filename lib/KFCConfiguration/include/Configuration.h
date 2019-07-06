/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <crc16.h>
#include <map>
#include "ConfigurationParameter.h"

#define CONFIG_MAGIC_DWORD              0xffff1111

#define CONFIG_STRINGIFY(s)             _CONFIG_STRINGIFY(s)
#define _CONFIG_STRINGIFY(s)            #s
#define CONFIG_GET_HANDLE(name)         getHandle(CONFIG_STRINGIFY(name))

uint16_t constexpr getHandle(const char *name) {
    return constexpr_crc16_calc((const uint8_t *)name, constexpr_strlen(name));
}

class Configuration {
public:
    typedef std::map<const uint16_t, ConfigurationParameter> ConfigurationMap;

    typedef struct {
        uint32_t magic;
        uint16_t crc;
        uint16_t length;
    } Header_t;

    Configuration(uint16_t offset, uint16_t size);

    // read map only, data is read on demand
    bool read();
    // write data to EEPROM
    bool write();

    const char *getString(uint16_t handle);
    void setString(uint16_t handle, const String &string);
    void setString(uint16_t handle, const __FlashStringHelper *string);

    void release();
    bool isDirty() const;

private:
    ConfigurationParameter *_findParam(ConfigurationParameter::TypeEnum_t type, uint16_t handle);

    ConfigurationParameter &_getParam(ConfigurationParameter::TypeEnum_t type, const uint16_t handle);

    bool _readMap();

private:
    uint16_t _offset;
    uint16_t _size;
    ConfigurationMap _map;
};
