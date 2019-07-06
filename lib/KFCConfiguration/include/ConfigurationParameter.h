/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class Buffer;

class ConfigurationParameter {
public:
    typedef enum : uint8_t {
        _INVALID = 0xff,
        _EOF = 0,
        STRING,
        LONG_STRING,
        STRUCT,
        LONG_STRUCT,
        BYTE,
        WORD,
        DWORD,
        QWORD,
        FLOAT
    } TypeEnum_t;

    ConfigurationParameter() {
    }
    ConfigurationParameter(TypeEnum_t type, uint16_t size, uint16_t position);

    bool isDirty() const;
    TypeEnum_t getType() const;

    bool write(Buffer &buffer, const uint16_t handle, uint16_t offset, uint16_t size);

    void setData(const uint8_t *data, uint16_t size);
    void setData(const __FlashStringHelper *data, uint16_t size);

    const char *getString(uint16_t offset, uint16_t size);

    void release();

    static uint16_t getSize(TypeEnum_t type, uint16_t &position, uint16_t size);

private:
    bool _readData(uint16_t offset, uint16_t size);

private:
    TypeEnum_t _type;
    uint8_t *_data;
    uint16_t _size;
    uint16_t _position;
};

