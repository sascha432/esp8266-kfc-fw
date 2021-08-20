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

void ConfigurationParameter::setData(Configuration &conf, const uint8_t *data, size_type length)
{
    __LDBG_printf("%s set_size=%u %s", toString().c_str(), length, Configuration::__debugDumper(*this, data, length).c_str());
    if (_compareData(data, length)) {
        __LDBG_printf("compareData=true");
        return;
    }
    if (_param.isString() && length == 0) {
        // we have an empty string, set length to 2 and fill with NUL bytes
        // otherwise the string will not be stored and created in write mode each time being accessed, using 24 bytes instead of 8
        length = 1;
        _makeWriteable(conf, length);
        __LDBG_printf("empty string fill=%u sizeof=%u len=%u", _param._length, _param.sizeOf(length), length);
        std::fill_n(_param.data(), length + 1, 0);
    }
    else {
        _makeWriteable(conf, length);
        memmove_P(_param.data(), data, length); // for strings, we do not copy trailing NUL byte if length != 0
    }
    // __LDBG_assert_printf(_param.isString() == false || _param.string()[length] == 0, "%s NUL byte missing", toString().c_str());
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

void ConfigurationParameter::dump(Print &output)
{
    if (!hasData()) {
        output.println(F("nullptr"));
    }
    else {
        switch (_param.type()) {
        case ParameterType::STRING:
            output.printf_P(PSTR("'%s'\n"), _param.string());
            break;
        case ParameterType::BINARY: {
                DumpBinary dumper(output, DumpBinary::kGroupBytesDefault, 32);
                dumper.setNewLine(F("\n      "));
                if (_param.length()) {
                    dumper.dump(_param.data(), _param.length());
                    output.print('\r');
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
                output.println(value);
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
            auto ptr = _param.string();
            if (strchr(ptr, 0xff)) {
                auto lastSep = strrchr(ptr, 0xff);
                if (lastSep) {
                    *lastSep = 0;
                }
                output.print('[');
                // KFCConfigurationClasses::ConfigStringArray
                char sep[2] = { 0xff, 0 };
                split::split(ptr, sep, [&output](const char *str, size_t size, int flags) {
                    JsonTools::Utf8Buffer buffer;
                    output.print('"');
                    auto endPtr = strchr(str, 0xff);
                    JsonTools::printToEscaped(output, str, endPtr ? endPtr - str : strlen(str), &buffer);
                    if ((flags & split::SplitFlagsType::LAST)) {
                        output.print('"');
                    }
                    else {
                        output.print(F("\", "));
                    }
                }, split::SplitFlagsType::EMPTY);
                output.print(']');
                if (lastSep) {
                    *lastSep = 0xff;
                }
            } else {
                JsonTools::Utf8Buffer buffer;
                output.print('"');
                JsonTools::printToEscaped(output, ptr, strlen(ptr), &buffer);
                output.print('"');
            }
        } break;
        case ParameterType::BINARY: {
            auto ptr = _param.data();
            auto size = _param.length();
            output.print('"');
            while (size--) {
                output.printf_P(PSTR("%02x"), *ptr);
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
            output.print(value);
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

    if (_param.length() != _param.next_offset_unaligned()) {
        __DBG_printf("%s length changed", toString().c_str());
        return true;
    }

    size_t requiredSize = _param.length();
    if (requiredSize < 128) {
        uint8_t buffer[128];
        return _readDataTo(conf, offset, buffer) && (memcmp(buffer, _param.data(), _param.length()) != 0);
    }

    // allocate memory
    auto tmp = std::unique_ptr<uint8_t[]>(::new uint8_t[requiredSize]);
    if (!tmp) {
        __LDBG_assert_panic(tmp.get(), "allocate returned nullptr");
        return true;
    }
    return _readDataTo(conf, offset, tmp.get()) && (memcmp(tmp.get(), _param.data(), _param.length()) != 0);
}

bool ConfigurationParameter::_readDataTo(Configuration &conf, uint16_t offset, uint8_t *ptr) const
{
    // nothing to read
    if (_param.length() == 0) {
        return true;
    }
    return conf.flashRead(ConfigurationHelper::getFlashAddress(offset), ptr, _param.length());
}

bool ConfigurationParameter::_readData(Configuration &conf, uint16_t offset)
{
    if (hasData()) { // return success if we have data
        return true;
    }

    // allocated memory will be increased to read data dword aligned if the size exceeds the temporary buffer
    ConfigurationHelper::allocate(_param.size(), *this);
    conf.setLastReadAccess();

    if (!_readDataTo(conf, offset, _param.data())) {
        return false;
    }
    __LDBG_assert_printf(_param.isString() == false || _param.string()[_param.length()] == 0, "%s NUL byte missing", toString().c_str());
    return true;
}
