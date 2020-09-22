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

#include "ConfigurationHelper.h"

#include "Allocator.hpp"


String ConfigurationParameter::toString() const
{
    //auto dataPtr = (uint8_t *)((((uintptr_t)&_info) + 3) & ~3);
    //auto dataOfs = (intptr_t)dataPtr - (intptr_t)&_info;
    //ptrdiff_t dataLen = sizeof(_info) - dataOfs;
#if DEBUG_CONFIGURATION_GETHANDLE

    //return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u (%p:%p %u %u)"), ConfigurationHelper::getHandleName(_param.handle), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty, dataPtr, _info.data, dataOfs, dataLen);
    return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u next_ofs=%u"),
        ConfigurationHelper::getHandleName(_param.getHandle()),
        _param.getHandle(),
        _param.size(),
        _param.length(),
        _param.data(),
        getTypeString(_param.type()),
        _param.isWriteable(),
        _param.next_offset()
    );

#else

    return PrintString(F("handle=%04x size=%u len=%u data=%p type=%s writable=%u next_ofs=%u"),
        _param.getHandle(), _param.size(), _param.length(), _param.data(), getTypeString(_param.type()), _param.isWriteable(), _param.next_offset()
    );

#endif
}

bool ConfigurationParameter::_compareData(const uint8_t *data, size_type length) const
{
    return _param.hasData() && _param.length() == length && memcmp_P(_param.data(), data, length) == 0;
}

void ConfigurationParameter::setData(Configuration &conf, const uint8_t *data, size_type length)
{
    __LDBG_printf("%s set_size=%u %s", toString().c_str(), length, Configuration::__debugDumper(*this, data, length).c_str());
    if (_compareData(data, length)) {
        __LDBG_printf("compareData=true");
        return;
    }
    if (_param.isString() && length == 0) {
        // we have an empty string, set length to 1 and fill with 2 NUL bytes
        // otherwise the string will not be stored and created in write mode each time its access, using 24 bytes instead of 8
        length = 1;
        _makeWriteable(conf, length);
        *((uint16_t *)_param.data()) = 0;
    }
    else {
        _makeWriteable(conf, length);
        memcpy_P(_param.data(), data, length);
    }
    __LDBG_assert_printf(_param.isString() == false || _param.string()[length] == 0, "%s NUL byte missing", toString().c_str());
}

const char *ConfigurationParameter::getString(Configuration &conf, uint16_t offset)
{
    if (_param.size() == 0) {
        return nullptr;
    }
    if (!_readData(conf, offset)) {
        return nullptr;
    }
    //__LDBG_printf("%s %s", toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    return _param.string();
}

const uint8_t *ConfigurationParameter::getBinary(Configuration &conf, size_type &length, uint16_t offset)
{
    if (_param.size() == 0) {
        length = 0;
        return nullptr;
    }
    if (!_readData(conf, offset)) {
        length = 0;
        return nullptr;
    }
    //__LDBG_printf("%s %s", toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    length = _param.length();
    return _param.data();
}

uint16_t ConfigurationParameter::read(Configuration &conf, uint16_t offset)
{
    if (!_readData(conf, offset)) {
        return 0;
    }
    return _param.length();
}

void ConfigurationParameter::_makeWriteable(Configuration &conf, size_type length)
{
    __LDBG_printf("%s length=%u is_writable=%u _writeable=%p ", toString().c_str(), length, _param.isWriteable(), _param._writeable);
    if (_param.isWriteable()) {
        _param.resizeWriteable(length, *this, conf);
    }
    else {
        _param.setWriteable(new WriteableData(length, *this, conf));
    }
}

const __FlashStringHelper *ConfigurationParameter::getTypeString(ParameterType type)
{
    switch (type) {
    case ParameterType::STRING:
        return F("STRING");
    case ParameterType::BINARY:
        return F("BINARY");
    case ParameterType::BYTE:
        return F("BYTE");
    case ParameterType::WORD:
        return F("WORD");
    case ParameterType::DWORD:
        return F("DWORD");
    case ParameterType::QWORD:
        return F("QWORD");
    case ParameterType::FLOAT:
        return F("FLOAT");
    case ParameterType::DOUBLE:
        return F("DOUBLE");
    default:
        break;
    }
    return F("INVALID");
}

void ConfigurationParameter::dump(Print &output)
{
    if (!hasData()) {
        output.println(F("nullptr"));
    } else {
        switch (_param.type()) {
        case ParameterType::STRING:
            output.printf_P(PSTR("'%s'\n"), _param.string());
            break;
        case ParameterType::BINARY: {
                DumpBinary dumper(output);
                if (_param.length()) {
                    dumper.dump(_param.data(), _param.length());
                }
                else {
                    output.println();
                }
            } break;
        case ParameterType::BYTE: {
                auto value = *(uint8_t *)_param.data();
                output.printf_P(PSTR("%u (%d, %02X)\n"), value, value, value);
            } break;
        case ParameterType::WORD: {
                auto value = *(uint16_t *)_param.data();
                output.printf_P(PSTR("%u (%d, %04X)\n"), value, value, value);
            } break;
        case ParameterType::DWORD: {
            auto value = *(uint32_t *)_param.data();
            output.printf_P(PSTR("%u (%d, %08X)\n"), value, value, value);
        } break;
        case ParameterType::QWORD: {
            auto value = *(uint64_t *)_param.data();
            print_string(output, value);
            output.println();
        } break;
        case ParameterType::FLOAT: {
                auto value = *(float *)_param.data();
                output.printf_P(PSTR("%f\n"), value);
            } break;
        case ParameterType::DOUBLE: {
                auto value = *(double *)_param.data();
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
        switch (_param.type()) {
        case ParameterType::STRING: {
            output.print('"');
            JsonTools::printToEscaped(output, _param.string(), _param.length(), false);
            output.print('"');
        } break;
        case ParameterType::BINARY: {
            auto ptr = _param.data();
            auto size = _param.length();
            output.print('"');
            while (size--) {
                output.printf_P(SPGM(_02x, "%02x"), *ptr);
                ptr++;
            }
            output.print('"');
        } break;
        case ParameterType::BYTE: {
            auto value = *(uint8_t *)_param.data();
            output.print((uint32_t)value);
        } break;
        case ParameterType::WORD: {
            auto value = *(uint16_t *)_param.data();
            output.print((uint32_t)value);
        } break;
        case ParameterType::DWORD: {
            auto value = *(uint32_t *)_param.data();
            output.print(value);
        } break;
        case ParameterType::QWORD: {
            auto value = *(uint64_t *)_param.data();
            print_string(output, value);
        } break;
        case ParameterType::FLOAT: {
            auto value = *(float *)_param.data();
            output.print(value, 6);
        } break;
        case ParameterType::DOUBLE: {
            auto value = *(double *)_param.data();
            output.print(value, 6);
        } break;
        default:
            output.println(FSPGM(null));
            break;
        }
    }
}

bool ConfigurationParameter::hasDataChanged(Configuration &conf) const
{
    // cannot be changed if not writable
    if (!_param.isWriteable()) {
        return false;
    }
    uint16_t offset;
    auto iterator = conf._findParam(ConfigurationParameter::TypeEnum_t::_ANY, _param.getHandle(), offset);
    if (iterator == conf._params.end()) { // not found
        __LDBG_printf("%s not found", toString().c_str());
        return true;
    }
    __LDBG_assert_panic(static_cast<const void *>(&(*iterator)) == static_cast<const void *>(this), "*iterator=%p this=%p", &(*iterator), this);

    if (_param.length() != _param.next_offset()) {
        __LDBG_printf("%s length changed", toString().c_str());
        return true;
    }

    size_t requiredSize = conf._eeprom.getReadSize(offset, _param.length());
    static constexpr size_t kStackBufferSize = 64;
    if (requiredSize <= kStackBufferSize - 1) {
        // use stack
        uint8_t buffer[kStackBufferSize] = {};
        _readDataTo(conf, offset, buffer, (size_type)requiredSize);
        return (memcmp(buffer, _param.data(), _param.length()) != 0);
    }

    // allocate memory
    auto tmp = std::unique_ptr<uint8_t[]>(::new uint8_t[requiredSize]{});
    __LDBG_assert_panic(tmp.get(), "allocate returned nullptr");

    _readDataTo(conf, offset, tmp.get(), (size_type)requiredSize);

    return (memcmp(tmp.get(), _param.data(), _param.length()) != 0);
}

bool ConfigurationParameter::_readDataTo(Configuration &conf, uint16_t offset, uint8_t *ptr, size_type maxSize) const
{
    // nothing to read
    if (_param.length() == 0) {
        return true;
    }

#if DEBUG_CONFIGURATION_GETHANDLE && 0

    size_t flashReadSize = 0;
    conf._eeprom.read(ptr, offset, _param.length(), maxSize, flashReadSize);
    ConfigurationHelper::addFlashUsage(_param.getHandle(), flashReadSize, 0);

#else

    conf._eeprom.read(ptr, offset, _param.length(), maxSize);

#endif

    return true;
}

bool ConfigurationParameter::_readData(Configuration &conf, uint16_t offset)
{
    if (hasData()) { // return success if we have data
        return true;
    }

    // allocated memory will be increased to read data dword aligned if the size exceeds the temporary buffer
    ConfigurationHelper::_allocator.allocate(conf._eeprom.getReadSize(offset, _param.size()), *this, 0);
    conf.setLastReadAccess();

    if (!_readDataTo(conf, offset, _param.data(), _param.size())) {
        return false;
    }

    __DBG_assert_printf(_param.isString() == false || _param.string()[_param.length()] == 0, "%s NUL byte missing", toString().c_str());

    return true;
}

