/**
* Author: sascha_lammers@gmx.de
*/

#include "ConfigurationParameter.h"
#include "Configuration.h"
#include <Buffer.h>
#include <DumpBinary.h>
#include <JsonTools.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

ConfigurationParameter::ConfigurationParameter(const Param_t &param) : _info(), _param(param)
{
    _info.size = getDefaultSize(_param.getType());
}

ConfigurationParameter::ConfigurationParameter(Handle_t handle, TypeEnum_t type) : _info(), _param({handle, type, 0})
{
    _info.size = getDefaultSize(type);
}

uint8_t *ConfigurationParameter::_allocate(Configuration *conf)
{
    if (_info.dirty) {
        __debugbreak_and_panic_printf_P(PSTR("%s allocate called on dirty parameter\n"), toString().c_str());
    }
    if (_info.data && _info.size == _param.getSize()) { // can we reuse the pointer?
        _debug_printf_P(PSTR("%s pointer reused\n"), toString().c_str());
        memset(_info.data, 0, _info.size);
        return _info.data;
    }
    conf->_release(_info.data);
    _info.data = conf->_allocate(_param.getSize());
    _info.size = _param.getSize();
    return _info.data;
}

void ConfigurationParameter::_release(Configuration *conf)
{
    if (_info.dirty) {
        __debugbreak_and_panic_printf_P(PSTR("%s release called on dirty parameter\n"), toString().c_str());
    }
    conf->_release(_info.data);
    _info = Info_t();
}

void ConfigurationParameter::_free()
{
    if (!_info.data || !_info.dirty) {
        __debugbreak_and_panic_printf_P(PSTR("%s free called on parameter without heap allocation\n"), toString().c_str());
    }
    free(_info.data);
    _info = Info_t();
}

String ConfigurationParameter::toString() const
{
    //auto dataPtr = (uint8_t *)((((uint32_t)&_info) + 3) & ~3);
    //auto dataOfs = (intptr_t)dataPtr - (intptr_t)&_info;
    //ptrdiff_t dataLen = sizeof(_info) - dataOfs;
#if DEBUG_GETHANDLE
    //return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u (%p:%p %u %u)"), getHandleName(_param.handle), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty, dataPtr, _info.data, dataOfs, dataLen);
    return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u"), getHandleName(_param.handle), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty);
#else
    return PrintString(F("handle=%04x size=%u len=%u data=%p type=%s dirty=%u"), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty);
#endif
}

//void ConfigurationParameter::setType(ConfigurationParameter::TypeEnum_t type)
//{
//    _param.type = (uint16_t)type;
//}

uint8_t ConfigurationParameter::getDefaultSize(TypeEnum_t type)
{
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

bool ConfigurationParameter::_compareData(const uint8_t *data, uint16_t size) const
{
    return _info.data && _info.size >= size && _param.length == size && memcmp(_info.data, data, size) == 0;
}

bool ConfigurationParameter::_compareData(const __FlashStringHelper *data, uint16_t size) const
{
    return _info.data && _info.size >= size && _param.length == size && memcmp_P(_info.data, data, size) == 0;
}

void ConfigurationParameter::setData(Configuration *conf, const uint8_t * data, uint16_t size)
{
    _debug_printf_P(PSTR("%s set_size=%u %s\n"), toString().c_str(), size, Configuration::__debugDumper(*this, data, size).c_str());
    if (_compareData(data, size)) {
        _debug_printf_P(PSTR("compareData=true\n"));
        return;
    }
    _makeWriteable(conf, size);
    memcpy(_info.data, data, size);
    if (_param.isString()) {
        _info.data[size] = 0;
    }
}

void ConfigurationParameter::setData(Configuration *conf, const __FlashStringHelper * data, uint16_t size)
{
    _debug_printf_P(PSTR("%s set_size=%u %s\n"), toString().c_str(), size, Configuration::__debugDumper(*this, data, size).c_str());
    if (_compareData(data, size)) {
        _debug_printf_P(PSTR("compareData=true\n"));
        return;
    }
    _makeWriteable(conf, size);
    memcpy_P(_info.data, (data), size);
    if (_param.isString()) {
        _info.data[size] = 0;
    }
}

void ConfigurationParameter::updateStringLength()
{
    if (_info.data) {
        _param.length = strlen(reinterpret_cast<const char *>(_info.data));
    }
}

const char *ConfigurationParameter::getString(Configuration* conf, uint16_t offset)
{
    if (!_info.data && !_readData(conf, offset)) {
        return nullptr;
    }
    //_debug_printf_P(PSTR("%s %s\n"), toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    return reinterpret_cast<const char *>(_info.data);
}

const uint8_t *ConfigurationParameter::getBinary(Configuration *conf, uint16_t &length, uint16_t offset)
{
    if (!_info.data && !_readData(conf, offset)) {
        return nullptr;
    }
    length = _info.size;
    //_debug_printf_P(PSTR("%s %s\n"), toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    return _info.data;
}

uint16_t ConfigurationParameter::read(Configuration *conf, uint16_t offset)
{
    if (!_info.data && !_readData(conf, offset)) {
        return 0;
    }
    return _param.length;
}

void ConfigurationParameter::_makeWriteable(Configuration *conf, uint16_t size)
{
    if (isDirty()) {
        if (size != _param.length) {
            auto oldSize = _param.getSize();
            _param.length = size;
            _info.size = _param.getSize();
            _info.data = reinterpret_cast<uint8_t *>(realloc(_info.data, _info.size));
            if (_param.getSize() > oldSize) {
                memset(_info.data + oldSize, 0, _param.getSize() - oldSize);
            }
        }
    }
    else {
        if (!_info.data) {
            conf->_writeAllocate(*this, size);
        }
        else {
            auto ptr = _info.data;
            auto oldSize = _param.getSize();
            conf->_writeAllocate(*this, size);                              // allocate new memory
            memcpy(_info.data, ptr, std::min(oldSize, size));               // copy data
            conf->_release(ptr);                                            // release pool ptr
        }
        _info.dirty = 1;
    }
}

const __FlashStringHelper *ConfigurationParameter::getTypeString(TypeEnum_t type)
{
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

void ConfigurationParameter::dump(Print &output)
{
    if (!hasData()) {
        output.println(F("nullptr"));
    } else {
        switch (_param.type) {
        case STRING:
            output.printf_P(PSTR("'%s'\n"), reinterpret_cast<const char *>(_info.data));
            break;
        case BINARY: {
                DumpBinary dumper(output);
                if (_info.size) {
                    dumper.dump(_info.data, _info.size);
                }
                else {
                    output.println();
                }
            } break;
        case BYTE: {
                auto value = *(uint8_t *)_info.data;
                output.printf_P(PSTR("%u (%d, %02X)\n"), value, value, value);
            } break;
        case WORD: {
                auto value = *(uint16_t *)_info.data;
                output.printf_P(PSTR("%u (%d, %04X)\n"), value, value, value);
            } break;
        case DWORD: {
            auto value = *(uint32_t *)_info.data;
            output.printf_P(PSTR("%u (%d, %08X)\n"), value, value, value);
        } break;
        case QWORD: {
            auto value = *(uint64_t *)_info.data;
#if defined(ESP32)
            output.printf_P(PSTR("%llu (%lld, %08llX)\n"), value, value, value);
#else
            output.printf_P(PSTR("%lu (%ld, %08lX)\n"), value, value, value);
#endif
        } break;
        case FLOAT: {
                auto value = *(float *)_info.data;
                output.printf_P(PSTR("%f\n"), value);
            } break;
        case DOUBLE: {
                auto value = *(double *)_info.data;
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
            JsonTools::printToEscaped(output, reinterpret_cast<const char *>(_info.data), _param.length, false);
            output.print('"');
            break;
        case BINARY: {
            auto ptr = _info.data;
            auto size = _param.length;
            output.print('"');
            while (size--) {
                output.printf_P(PSTR("%02x"), *ptr & 0xff);
                ptr++;
            }
            output.print('"');
        } break;
        case BYTE: {
            auto value = *(uint8_t *)_info.data;
            output.printf_P(PSTR("%u"), value);
        } break;
        case WORD: {
            auto value = *(uint16_t *)_info.data;
            output.printf_P(PSTR("%u"), value);
        } break;
        case DWORD: {
            auto value = *(uint32_t *)_info.data;
            output.printf_P(PSTR("%u"), value);
        } break;
        case QWORD: {
            auto value = *(uint64_t *)_info.data;
#if defined(ESP32)
            output.printf_P(PSTR("%llu"), value);
#else
            output.printf_P(PSTR("%lu"), value);
#endif
        } break;
        case FLOAT: {
            auto value = *(float *)_info.data;
            output.printf_P(PSTR("%f"), value);
        } break;
        case DOUBLE: {
            auto value = *(double *)_info.data;
            output.printf_P(PSTR("%f"), value);
        } break;
        default:
            output.println(F("null"));
            break;
        }
    }
}

bool ConfigurationParameter::_readData(Configuration *conf, uint16_t offset)
{
    uint8_t *ptr = _allocate(conf);
    if (conf->_storage.size() > GARBAGE_COLLECTOR_MIN_POOL) {
        conf->setLastReadAccess();
    }

    if (_param.length) {
        conf->_eeprom.read(ptr, offset, _param.length, ConfigurationHelper::Pool::align(_info.size));
    }
#if DEBUG_CONFIGURATION
    if (_param.isString()) {
        if (ptr[_param.length] != 0) {
            __debugbreak_and_panic_printf_P(PSTR("%s last byte not NUL\n"), toString().c_str());
        }
    }
    //if (_param.isString()) {
    //    ptr[_param.length] = 0;
    //}
#endif

#if DEBUG_CONFIGURATION
    if (_info.size != getSize()) {
        _debug_printf_P(PSTR("ERROR: %s size mismatch\n"), toString().c_str());
        //__debugbreak_and_panic_printf_P(PSTR("%s size mismatch\n"), toString().c_str());
    }
#endif

    _info.dirty = 0;
    return true;
}
