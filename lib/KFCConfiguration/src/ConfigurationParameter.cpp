/**
* Author: sascha_lammers@gmx.de
*/

#include "ConfigurationParameter.h"
#include "Configuration.h"
#include <Buffer.h>
#include <DumpBinary.h>
#include <JsonTools.h>

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

ConfigurationParameter::ConfigurationParameter(const Param_t &param) {
    _param = param;
    memset(&_info, 0, sizeof(_info));
}

uint8_t *ConfigurationParameter::allocate(uint16_t size) {
#if CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE
    if (!_info.alloc && isAligned() && size <= sizeof(_info.data)) {
        return (uint8_t *)&_info.data;
    }
#endif
    _info.copied = 0;
    _info.dirty = 0;
    if (_info.alloc) {
        if (_info.size == size) {
            memset(_info.data, 0, _info.size);
            return _info.data;
        }
        free(_info.data);
    }
    _info.alloc = 1;
    _info.data = (uint8_t *)calloc(size, 1);
    return _info.data;
}

void ConfigurationParameter::freeData() {
    //_debug_printf_P(PSTR("ConfigurationParameter::freeData(): %04x, alloc %d, ptr %p, size %d\n"), _param.handle, _info.alloc, _info.data, _info.size);
    if (_info.alloc) {
        free(_info.data);
    }
    memset(&_info, 0, sizeof(_info));
}

void ConfigurationParameter::setDirty(bool dirty) {
    _info.dirty = dirty;
    _info.copied = !dirty;
}

bool ConfigurationParameter::isWriteable(uint16_t size) const {
    return (_info.dirty && size <= _info.size);
}

#if CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE

bool ConfigurationParameter::needsAlloc() const {
    uint16_t requiredSize = _param.length;
    if (_param.type == STRING) {
        requiredSize++;
    }
    return requiredSize > sizeof(_info.data);
}

#endif

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
    case ConfigurationParameter::QWORD:
        return sizeof(uint64_t);
    case ConfigurationParameter::FLOAT:
        return sizeof(float);
    case ConfigurationParameter::DOUBLE:
        return sizeof(double);
    case ConfigurationParameter::STRING:
    case ConfigurationParameter::BINARY:
    case ConfigurationParameter::_ANY:
    case ConfigurationParameter::_INVALID:
        break;
    }
    return 0; // dynamic size
}

bool ConfigurationParameter::compareData(const uint8_t *data, uint16_t size) const {
    //_debug_printf_P(PSTR("compareData(%p, %d): aligned=%d, size=%d, length=%d, cmpPtr=%p\n"), data, size, isAligned(), _info.size, _param.length, data);
    return _info.size >= size && _param.length == size && memcmp(getDataPtr(), data, size) == 0;
}

bool ConfigurationParameter::compareData_P(PGM_P data, uint16_t size) const {
    return _info.size >= size && _param.length == size && memcmp_P(getDataPtr(), data, size) == 0;
}

void ConfigurationParameter::setData(const uint8_t * data, uint16_t size, bool addNulByte) {
    if (hasData()) {
        if (compareData(data, size)) {
            return;
        }
        freeData();
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
        if (compareData_P(reinterpret_cast<PGM_P>(data), size)) {
            return;
        }
        freeData();
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
    freeData();
    _info.data = data;
    _info.size = size;
    _info.copied = 1;
    _info.alloc = 1;
}

const char * ConfigurationParameter::getString(Configuration *conf, uint16_t offset) {
    if (!hasData()) {
        if (!_readData(conf, offset, true)) {
            return nullptr;
        }
    }
    return (const char *)getDataPtr();
}

uint8_t * ConfigurationParameter::getBinary(Configuration *conf, uint16_t &length, uint16_t offset) {
    if (!hasData()) {
        if (!_readData(conf, offset)) {
            return nullptr;
        }
    }
    length = _info.size;
    return getDataPtr();
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
    case ConfigurationParameter::QWORD:
        return F("QWORD");
    case ConfigurationParameter::FLOAT:
        return F("FLOAT");
    case ConfigurationParameter::DOUBLE:
        return F("DOUBLE");
    case ConfigurationParameter::_INVALID:
    case ConfigurationParameter::_ANY:
        break;
    }
    return F("INVALID");
}

uint16_t ConfigurationParameter::read(Configuration *conf, uint16_t offset) {
    if (!hasData()) {
        if (!_readData(conf, offset, _param.type == STRING)) {
            return 0;
        }
    }
    return _param.length;
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
        case QWORD: {
            auto value = *(uint64_t *)getDataPtr();
#if defined(ESP32)
            output.printf_P(PSTR("%llu (%lld, %08llX)\n"), value, value, value);
#else
            output.printf_P(PSTR("%lu (%ld, %08lX)\n"), value, value, value);
#endif
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

void ConfigurationParameter::exportAsJson(Print& output)
{
    if (!hasData()) {
        output.println(F("null"));
    }
    else {
        switch (_param.type) {
        case STRING:
            output.print('"');
            JsonTools::printToEscaped(output, (const char*)getDataPtr(), _info.size, false);
            output.print('"');
            break;
        case BINARY: {
            auto ptr = getDataPtr();
            auto size = _info.size;
            output.print('"');
            while (size--) {
                output.printf_P(PSTR("%02x"), *ptr & 0xff);
                ptr++;
            }
            output.print('"');
        } break;
        case BYTE: {
            auto value = *(uint8_t *)getDataPtr();
            output.printf_P(PSTR("%u"), value);
        } break;
        case WORD: {
            auto value = *(uint16_t *)getDataPtr();
            output.printf_P(PSTR("%u"), value);
        } break;
        case DWORD: {
            auto value = *(uint32_t *)getDataPtr();
            output.printf_P(PSTR("%u"), value);
        } break;
        case QWORD: {
            auto value = *(uint64_t *)getDataPtr();
#if defined(ESP32)
            output.printf_P(PSTR("%llu"), value);
#else
            output.printf_P(PSTR("%lu"), value);
#endif
        } break;
        case FLOAT: {
            auto value = *(float *)getDataPtr();
            output.printf_P(PSTR("%f"), value);
        } break;
        default:
            output.println(F("null"));
            break;
        }
    }
}

void ConfigurationParameter::release() {
    //_debug_printf_P(PSTR("ConfigurationParameter::release(): %04x (%s) size %d, dirty %d, copied %d, alloc %d\n"), _param.handle, getHandleName(_param.handle), _info.size, _info.dirty, _info.copied, _info.alloc);
    if (!_info.dirty && _info.alloc) { // free allocated data ptr if data has not been changed and force re-read
        //_debug_printf_P(PSTR("ConfigurationParameter::release(): %04x (%s) size %d, dirty %d, copied %d, alloc %d\n"), _param.handle, getHandleName(_param.handle), _info.size, _info.dirty, _info.copied, _info.alloc);
        freeData();
    }
}

bool ConfigurationParameter::_readData(Configuration *conf, uint16_t offset, bool addNulByte) {
    if (!_param.length) {
        return false;
    }
    uint8_t *ptr = allocate(_param.length + (addNulByte ? 1 : 0));
    if (_info.alloc) {
        conf->setLastReadAccess();
    }

#if CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE
    conf->getEEPROM(ptr, offset, _param.length, _info.alloc ? _info.size : sizeof(_info.data));
#else
    conf->getEEPROM(ptr, offset, _param.length, _info.size);
#endif
    if (addNulByte) {
        ptr[_param.length] = 0;
    }

    _info.copied = 1;
    _info.dirty = 0;
    _info.size = _param.length;
    return true;
}
