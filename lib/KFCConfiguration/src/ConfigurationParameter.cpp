/**
* Author: sascha_lammers@gmx.de
*/

#include "ConfigurationParameter.h"
#include <Buffer.h>


ConfigurationParameter::ConfigurationParameter(TypeEnum_t type, uint16_t size, uint16_t position) {
    _type = type;
    _data = nullptr;
    _size = size;
    _position = position;
}

bool ConfigurationParameter::isDirty() const {
    return _data && _position == 0;
}

ConfigurationParameter::TypeEnum_t ConfigurationParameter::getType() const {
    return _type;
}

bool ConfigurationParameter::write(Buffer & buffer, const uint16_t handle, uint16_t offset, uint16_t size) {

    buffer.write((const uint8_t *)&_type, sizeof(_type));
    buffer.write((const uint8_t *)&handle, sizeof(handle));

    auto dataPos = (uint16_t)buffer.length();

    if (!_data) {
        if (!_readData(offset, size)) {
            debug_printf_P(PSTR("ConfigurationParameter::write(): Failed to read data\n"));
            return false;
        }
    }

    switch (_type) {
    case STRING:
    case STRUCT:
        buffer.write((char)_size);
        dataPos++;
        buffer.write((const uint8_t *)_data, _size);
        break;
    default:
        debug_printf_P(PSTR("ConfigurationParameter::write(): Invalid type\n"));
        free(_data);
        _data = nullptr;
        return false;
    }

    _position = dataPos;
    free(_data);
    _data = nullptr;

    return true;
}

void ConfigurationParameter::setData(const uint8_t * data, uint16_t size) {
    if (_data) {
        if (memcmp(_data, data, size) == 0) {
            return;
        }
        free(_data);
        _size = 0;
    }
    _position = 0;
    _data = (uint8_t *)malloc(size + 1);
    if (_data) {
        memcpy(_data, data, size);
        _data[size] = 0;
        _size = size;
    }
}

void ConfigurationParameter::setData(const __FlashStringHelper * data, uint16_t size) {
    if (_data) {
        if (memcmp_P(_data, data, size) == 0) {
            return;
        }
        free(_data);
        _size = 0;
    }
    _position = 0;
    _data = (uint8_t *)malloc(size + 1);
    if (_data) {
        memcpy_P(_data, reinterpret_cast<PGM_P>(data), size);
        _data[size] = 0;
        _size = size;
    }
}

const char * ConfigurationParameter::getString(uint16_t offset, uint16_t size) {
    if (!_data) {
        if (!_readData(offset, size)) {
            return nullptr;
        }
    }
    return (const char *)_data;
}

uint16_t ConfigurationParameter::getSize(TypeEnum_t type, uint16_t & position, uint16_t size) {
    uint8_t _byte = -1;
    uint16_t _word = -1;
    switch (type) {
    case _EOF:
        return 0;
    case STRING:
    case STRUCT:
        EEPROM.get(position, _byte);
        position += sizeof(_byte);
        return _byte;
    case LONG_STRUCT:
    case LONG_STRING:
        EEPROM.get(position, _word);
        position += sizeof(_word);
        return _word;
    case BYTE:
        return sizeof(uint8_t);
    case WORD:
        return sizeof(uint16_t);
    case DWORD:
        return sizeof(uint32_t);
    case QWORD:
        return sizeof(uint64_t);
    case FLOAT:
        return sizeof(float);
    }
    debug_printf_P(PSTR("ConfigurationParameter::getSize(): Invalid type\n"));
    return -1;
}

void ConfigurationParameter::release() {
    if (_data) {
        free(_data);
        _data = nullptr;
    }

}

bool ConfigurationParameter::_readData(uint16_t offset, uint16_t size) {
    if (_position == 0 || _position + _size >= size) {
        return false;
    }
    _data = (uint8_t *)malloc(_size + 1);
    memcpy(_data, EEPROM.getDataPtr() + _position + offset, _size);
    _data[_size] = 0;
    return true;
}
