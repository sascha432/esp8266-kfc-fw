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
        __LDBG_printf("%s pointer reused", toString().c_str());
        memset(_info.data, 0, _info.size);
        return _info.data;
    }
    conf->__release(_info.data);
    _info.data = conf->_allocate(_param.getSize());
    _info.size = _param.getSize();
    return _info.data;
}

void ConfigurationParameter::__release(Configuration *conf)
{
    if (_info.dirty) {
        __debugbreak_and_panic_printf_P(PSTR("%s release called on dirty parameter\n"), toString().c_str());
    }
    conf->__release(_info.data);
    _info = Info_t();
}

void ConfigurationParameter::_free()
{
    if (!_info.dirty) {
        __debugbreak_and_panic_printf_P(PSTR("%s free called on parameter without heap allocation\n"), toString().c_str());
    }
    if (_info.data) {
        free(_info.data);
    }
    _info = Info_t();
}

String ConfigurationParameter::toString() const
{
    //auto dataPtr = (uint8_t *)((((uintptr_t)&_info) + 3) & ~3);
    //auto dataOfs = (intptr_t)dataPtr - (intptr_t)&_info;
    //ptrdiff_t dataLen = sizeof(_info) - dataOfs;
#if DEBUG_CONFIGURATION_GETHANDLE
    //return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u (%p:%p %u %u)"), ConfigurationHelper::getHandleName(_param.handle), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty, dataPtr, _info.data, dataOfs, dataLen);
    return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u"), ConfigurationHelper::getHandleName(_param.handle), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty);
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
    return _info.data && _info.size >= size && _param.length == size && memcmp_P(_info.data, data, size) == 0;
}

void ConfigurationParameter::setData(Configuration *conf, const uint8_t *data, uint16_t size)
{
    __LDBG_printf("%s set_size=%u %s", toString().c_str(), size, Configuration::__debugDumper(*this, data, size).c_str());
    if (_compareData(data, size)) {
        __LDBG_printf("compareData=true");
        return;
    }
    _makeWriteable(conf, size);
    memcpy_P(_info.data, data, size);
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
    //__LDBG_printf("%s %s", toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    return reinterpret_cast<const char *>(_info.data);
}

const uint8_t *ConfigurationParameter::getBinary(Configuration *conf, uint16_t &length, uint16_t offset)
{
    if (!_info.data && !_readData(conf, offset)) {
        return nullptr;
    }
    length = _info.size;
    //__LDBG_printf("%s %s", toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    return _info.data;
}

uint16_t ConfigurationParameter::read(Configuration *conf, uint16_t offset)
{
    if (!_info.data && !_readData(conf, offset)) {
        return 0;
    }
    return getLength();
    //if (_info.dirty) {
    //    if (_param.isString()) {
    //        return (uint16_t)strlen(reinterpret_cast<const char *>(_info.data));
    //    }
    //    return _info.size;
    //}
    //return _param.length;
}

void ConfigurationParameter::_makeWriteable(Configuration *conf, uint16_t size)
{
    if (isDirty()) {
        // check if the size has changed
        if (_param.getSize(size) != _info.size) {
            uint16_t oldSize = _info.size;
            _info.size = _param.getSize(size);
            _info.data = reinterpret_cast<uint8_t *>(realloc(_info.data, _info.size));
            if (_info.size > oldSize) {
                memset(_info.data + oldSize, 0, _info.size - oldSize);
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
            conf->_writeAllocate(*this, size);                                  // allocate new memory
            memcpy(_info.data, ptr, std::min(oldSize, size));                   // copy data
            conf->__release(ptr);                                               // release pool ptr
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
            print_string(output, value);
            output.println();
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
        output.println(FSPGM(null));
    }
    else {
        switch (_param.type) {
        case STRING: {
            output.print('"');
            auto str = reinterpret_cast<const char *>(_info.data);
            JsonTools::printToEscaped(output, str, _info.dirty ? strlen(str) : _param.length, false);
            output.print('"');
        } break;
        case BINARY: {
            auto ptr = _info.data;
            auto size = _info.size;
            output.print('"');
            while (size--) {
                output.printf_P(SPGM(_02x, "%02x"), *ptr & 0xff);
                ptr++;
            }
            output.print('"');
        } break;
        case BYTE: {
            auto value = *(uint8_t *)_info.data;
            output.print((uint32_t)value);
        } break;
        case WORD: {
            auto value = *(uint16_t *)_info.data;
            output.print((uint32_t)value);
        } break;
        case DWORD: {
            auto value = *(uint32_t *)_info.data;
            output.print(value);
        } break;
        case QWORD: {
            auto value = *(uint64_t *)_info.data;
            print_string(output, value);
        } break;
        case FLOAT: {
            auto value = *(float *)_info.data;
            output.print(value, 6);
        } break;
        case DOUBLE: {
            auto value = *(double *)_info.data;
            output.print(value, 6);
        } break;
        default:
            output.println(FSPGM(null));
            break;
        }
    }
}

bool ConfigurationParameter::hasDataChanged(Configuration *conf) const
{
    if (!isDirty()) {
        return false;
    }
    if (!hasData()) {
        __DBG_printf("%s not data", toString().c_str());
        return false;
    }
    if (_param.length == 0) {
        return true;
    }
    uint16_t offset;
    auto iterator = conf->_findParam(ConfigurationParameter::TypeEnum_t::_ANY, _param.handle, offset);
    if (iterator == conf->_params.end()) { // not found
        __DBG_printf("%s not found", toString().c_str());
        return true;
    }
    if (static_cast<const void *>(&(*iterator)) != static_cast<const void *>(this)) {
        __DBG_printf("%s parameter mismatch", toString().c_str());
        return true;
    }
    auto size = _param.getSize();
    auto alignedSize = Configuration::Pool::align(size);
    // get original size and allocate aligned blocked
    auto tmp = conf->_allocate(_param.getSize());
    // auto tmp = reinterpret_cast<uint8_t *>(calloc(alignedSize, 1));
    if (!tmp) {
        __DBG_printf("%s allocate failed", toString().c_str());
        return true;
    }
    if (!_readDataTo(conf, offset, tmp, alignedSize)) {
        return false;
    }
    auto result = (memcmp(tmp, _info.data, size) != 0);
#if DEBUG_CONFIGURATION || 0
    if (result)
    {
        __DBG_printf("%s result=%u", toString().c_str(), result);
        DumpBinary dump(DEBUG_OUTPUT);
        dump.setPerLine(size);
        dump.dump(tmp, size);
        dump.dump(_info.data, size);
    }
#endif
    // free(tmp);

    conf->__release(tmp);
    return result;
}

bool ConfigurationParameter::_readDataTo(Configuration *conf, uint16_t offset, uint8_t *ptr, uint16_t size) const
{
    if (_param.length) {

#if DEBUG_CONFIGURATION || 0
        auto address = (const uintptr_t)ptr;
        if (address % 4) {
            __DBG_panic("%s ptr=%p address=%u offset=%u not aligned", toString().c_str(), ptr, address, address % 4);
        }
#endif

#if DEBUG_CONFIGURATION_GETHANDLE
        auto readSize =
#endif
        conf->_eeprom.read(ptr, offset, _param.length, ConfigurationHelper::Pool::align(size));
        __DBG__addFlashReadSize(_param.handle, readSize);
    }

#if DEBUG_CONFIGURATION || 1
    if (_param.isString()) {
        if (ptr[_param.length] != 0) {
            __DBG_panic("%s last byte not NUL", toString().c_str()); // TODO change to __DBG_printf
        }
        ptr[_param.length] = 0;
    }
#else
    if (_param.isString()) {
        if (ptr[_param.length] != 0) {
            __DBG_printf("%s last byte not NUL", toString().c_str()); // TODO change to __DBG_printf
        }
       ptr[_param.length] = 0;
    }
#endif
    return true;
}

bool ConfigurationParameter::_readData(Configuration *conf, uint16_t offset)
{
    auto ptr = _allocate(conf);
    if (conf->_storage.size() > GARBAGE_COLLECTOR_MIN_POOL) {
        conf->setLastReadAccess();
    }

    if (!_readDataTo(conf, offset, ptr, ConfigurationHelper::Pool::align(_info.size))) {
        return false;
    }

#if DEBUG_CONFIGURATION
    if (_info.size != getSize()) {
        __LDBG_printf("ERROR: %s size mismatch", toString().c_str());
        //__debugbreak_and_panic_printf_P(PSTR("%s size mismatch\n"), toString().c_str());
    }
#endif

    _info.dirty = 0;
    return true;
}
