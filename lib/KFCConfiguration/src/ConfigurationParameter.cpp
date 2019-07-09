/**
* Author: sascha_lammers@gmx.de
*/

#include "ConfigurationParameter.h"
#include "Configuration.h"
#if !_WIN32
#include <EEPROM.h>
#endif
#include <Buffer.h>
#include <DumpBinary.h>

ConfigurationParameter::ConfigurationParameter(const Param_t &param) {
    _param = param;
    memset(&_info, 0, sizeof(_info));
}

uint8_t *ConfigurationParameter::allocate(uint16_t size) {
    uint8_t *ptr;
    if (1 || size > sizeof(ptr)) {
        _info.alloc = 1;
        _info.data = (uint8_t *)calloc(size, 1);
        return _info.data;
    }
    return (uint8_t *)&_info.data;
}

void ConfigurationParameter::free() {
    if (_info.alloc) {
        ::free(_info.data);
    }
    memset(&_info, 0, sizeof(_info));
}

uint8_t *ConfigurationParameter::getDataPtr() {
    return _info.alloc ? _info.data : (uint8_t *)&_info.data;
}

bool ConfigurationParameter::hasData() const {
    return _info.dirty || _info.copied;
}

bool ConfigurationParameter::isDirty() const {
    return _info.dirty;
}

void ConfigurationParameter::setDirty(bool dirty) {
    _info.dirty = dirty;
    _info.copied = !dirty;
}

bool ConfigurationParameter::isWriteable(uint16_t size) const {
    return (_info.dirty && size <= _info.size);
}

bool ConfigurationParameter::needsAlloc() const {
    uint16_t requiredSize = _param.length;
    if (_param.type == STRING) {
        requiredSize++;
    }
    return requiredSize > sizeof(uint8_t *);
}

// max. size
uint16_t ConfigurationParameter::getSize() const {
    return _info.size;
}

void ConfigurationParameter::setSize(uint16_t size) {
    _info.size = size;
}

ConfigurationParameter::TypeEnum_t ConfigurationParameter::getType() const {
    return (ConfigurationParameter::TypeEnum_t)_param.type;
}

void ConfigurationParameter::setType(ConfigurationParameter::TypeEnum_t type) {
    _param.type = (int32_t)type;
}

uint8_t ConfigurationParameter::getDefaultSize(TypeEnum_t type) {
    switch (type) {
    case ConfigurationParameter::BYTE:
        return sizeof(uint8_t);
    case ConfigurationParameter::WORD:
        return sizeof(uint16_t);
    case ConfigurationParameter::DWORD:
        return sizeof(uint32_t);
    case ConfigurationParameter::FLOAT:
        return sizeof(float);
    case ConfigurationParameter::STRING:
    case ConfigurationParameter::BINARY:
    case ConfigurationParameter::_INVALID:
        break;
    }
    return 0; // dynamic size
}

void ConfigurationParameter::setData(const uint8_t * data, uint16_t size, bool addNulByte) {
    if (hasData()) {
        if (_info.size == size && memcmp(getDataPtr(), data, size) == 0) {
            return;
        }
        free();
    }
    auto ptr = allocate(size + (addNulByte ? 1 : 0));
    memcpy(ptr, data, size);
    if (addNulByte) {
        ptr[size] = 0;
    }
    _info.size = size;
    _param.length = size;
    _info.dirty = 1;
}

void ConfigurationParameter::setData(const __FlashStringHelper * data, uint16_t size, bool addNulByte) {
    if (hasData()) {
        if (_info.size == size && memcmp_P(getDataPtr(), data, size) == 0) {
            return;
        }
        free();
    }
    auto ptr = allocate(size + (addNulByte ? 1 : 0));
    memcpy_P(ptr, reinterpret_cast<PGM_P>(data), size);
    if (addNulByte) {
        ptr[size] = 0;
    }
    _info.size = size;
    _param.length = size;
    _info.dirty = 1;
}

void ConfigurationParameter::updateStringLength() {
    if (hasData()) {
        _param.length = strlen((const char *)getDataPtr());
    }
}

void ConfigurationParameter::setDataPtr(uint8_t *data, uint16_t size) {
    free();
    _info.data = data;
    _info.size = size;
    _info.dirty = 1;
    _info.alloc = 1;
}

const char * ConfigurationParameter::getString(uint16_t offset) {
    if (!hasData()) {
        if (!_readData(offset, true)) {
            return nullptr;
        }
    }
    return (const char *)getDataPtr();
}

const uint8_t * ConfigurationParameter::getBinary(uint16_t &length, uint16_t offset) {
    if (!_info.data) {
        if (!_readData(offset)) {
            return nullptr;
        }
    }
    length = _info.size;
    return (const uint8_t *)getDataPtr();
}

const String ConfigurationParameter::getTypeString(TypeEnum_t type) {
    switch (type) {
    case ConfigurationParameter::STRING:
        return F("STRING");
    case ConfigurationParameter::BINARY:
        return F("BINARY");
    case ConfigurationParameter::BYTE:
        return F("BYTE");
    case ConfigurationParameter::WORD:
        return F("WORD");
    case ConfigurationParameter::DWORD:
        return F("DWORD");
    case ConfigurationParameter::FLOAT:
        return F("FLOAT");
    case ConfigurationParameter::_INVALID:
        break;
    }
    return F("INVALID");
}

ConfigurationParameter::Param_t &ConfigurationParameter::getParam() {
    return _param;
}

uint16_t ConfigurationParameter::read(uint16_t offset) {
    if (!_readData(offset, _param.type == STRING)) {
        return 0;
    }
    return _param.length;
}

bool ConfigurationParameter::write(Buffer & buffer) {
    buffer.write((const uint8_t *)getDataPtr(), _param.length);
    return true;
}

void ConfigurationParameter::dump(Print &output) {
    output.printf_P(PSTR("dirty=%d copied=%d alloc=%d "), _info.dirty, _info.copied, _info.alloc);
    if (!hasData()) {
        output.println(F("nullptr"));
    } else {
        switch (_param.type) {
        case STRING:
            output.printf_P(PSTR("'%s'\n"), (const char *)getDataPtr());
            break;
        case BINARY: {
                DumpBinary dumper(output);
                dumper.dump(getDataPtr(), _info.size);
            } break;
        case BYTE: {
                auto value = *(uint8_t *)getDataPtr();
                output.printf_P(PSTR("%u (%d, %02X)\n"), value, value, value);
            } break;
        case WORD: {
                auto value = *(uint16_t *)getDataPtr();
                output.printf_P(PSTR("%u (%d, %04X)\n"), value, value, value);
            } break;
        case DWORD: {
                auto value = *(uint32_t *)getDataPtr();
                output.printf_P(PSTR("%u (%d, %08X)\n"), value, value, value);
            } break;
        case FLOAT: {
                auto value = *(float *)getDataPtr();
                output.printf_P(PSTR("%f\n"), value);
            } break;
        default:
             output.println(F("Invalid type"));
             break;
        }
    }
}

void ConfigurationParameter::release() {
    debug_printf_P(PSTR("ConfigurationParameter::release(): %04x (%s) size %d, dirty %d, copied %d, alloc %d\n"), _param.handle, getHandleName(_param.handle), _info.size, _info.dirty, _info.copied, _info.alloc);
    if (!_info.dirty && _info.alloc) { // free allocated data ptr if data has not been changed and force re-read
        free();
    }
}

bool ConfigurationParameter::_readData(uint16_t offset, bool addNulByte) {
    if (hasData()) {
        return true;
    }
    if (!_param.length) {
        return false;
    }

    uint8_t *ptr = allocate(_param.length + (addNulByte ? 1 : 0));

#if 0
    if (!Configuration::isEEPROMInitialized()) {
        Configuration::beginEEPROM(); // do not call end() to avoid performance penalty
    }
    memcpy(ptr, EEPROM.getConstDataPtr() + offset, _param.length);
    if (addNulByte) {
        ptr[_param.length] = 0;
    }
#elif ESP8266

    if (Configuration::isEEPROMInitialized()) { // use RAM if EEPROM was copied
        memcpy(ptr, EEPROM.getConstDataPtr() + offset, _param.length);
        if (addNulByte) {
            ptr[_param.length] = 0;
        }
    } else {
        Configuration::copyEEPROM(ptr, offset, _param.length, _info.alloc ? _info.size : sizeof(uint8_t *));
        if (addNulByte) {
            ptr[_param.length] = 0;
        }
    }
#endif

    _info.copied = 1;
    _info.dirty = 0;
    _info.size = _param.length;
    return true;
}
