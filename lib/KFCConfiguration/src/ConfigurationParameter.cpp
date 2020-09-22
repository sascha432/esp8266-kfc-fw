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

String ConfigurationParameter::toString() const
{
    //auto dataPtr = (uint8_t *)((((uintptr_t)&_info) + 3) & ~3);
    //auto dataOfs = (intptr_t)dataPtr - (intptr_t)&_info;
    //ptrdiff_t dataLen = sizeof(_info) - dataOfs;
#if DEBUG_CONFIGURATION_GETHANDLE
    //return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u (%p:%p %u %u)"), ConfigurationHelper::getHandleName(_param.handle), _param.handle, _info.size, _param.length, _info.data, getTypeString(_param.getType()), _info.dirty, dataPtr, _info.data, dataOfs, dataLen);
    return PrintString(F("handle=%s[%04x] size=%u len=%u data=%p type=%s dirty=%u"), ConfigurationHelper::getHandleName(_param.getHandle()), _param.getHandle(), _param.size(), _param.length(), _param.data(), getTypeString(_param.type()), _param.isWriteable());
#else
    return PrintString(F("handle=%04x size=%u len=%u data=%p type=%s writable=%u"), _param.getHandle(), _param.size(), _param.length(), _param.data(), getTypeString(_param.type()), _param.isWriteable());
#endif
}

//void ConfigurationParameter::setType(ConfigurationParameter::TypeEnum_t type)
//{
//    _param.type = (uint16_t)type;
//}


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
        // TODO workaround for broken 0 length strings
        length = 1;
        _makeWriteable(conf, length);
        _param.data()[0] = 0;
        _param.data()[1] = 0;
    }
    else {
        _makeWriteable(conf, length);
        memcpy_P(_param.data(), data, length);
        __LDBG_assert_printf(_param.isString() == false || _param.string()[length] == 0, "%s NUL byte missing", toString().c_str());
    }
}

// void ConfigurationParameter::updateStringLength()
// {
//     __LDBG_assert_printf(_param.isWriteable(), "%s not writable", toString().c_str());
//     if (_param.isWriteable()) {
//         _param.length = strlen(reinterpret_cast<const char *>(_info.data));
//     }
// }

const char *ConfigurationParameter::getString(Configuration &conf, uint16_t offset)
{
    if (!_readData(conf, offset)) {
        return nullptr;
    }
    //__LDBG_printf("%s %s", toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    return _param.string();
}

const uint8_t *ConfigurationParameter::getBinary(Configuration &conf, size_type &length, uint16_t offset)
{
    if (!_readData(conf, offset)) {
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
        __DBG_printf("%s not found", toString().c_str());
        return true;
    }
    __LDBG_assert_panic(static_cast<const void *>(&(*iterator)) == static_cast<const void *>(this), "*iterator=%p this=%p", &(*iterator), this);
    size_t maxSize;
    auto tmp = ConfigurationHelper::_allocator.allocate(conf._eeprom.getReadSize(offset, std::max(_param.length(), _param.next_offset())), &maxSize);
    if (!_readDataTo(conf, offset, tmp, (size_type)maxSize)) {
        return false;
    }
    auto result = (memcmp(tmp, _param.data(), _param.length()) != 0);
#if DEBUG_CONFIGURATION && 0
    if (result) {
        __DBG_printf("%s result=%u", toString().c_str(), result);
        // DumpBinary dump(DEBUG_OUTPUT);
        // dump.setPerLine(0xff);
        // dump.dump(tmp, _param.length());
        // dump.dump(_param.data(), _param.length());
    }
#endif
    ConfigurationHelper::_allocator.deallocate(tmp);
    return result;
}

bool ConfigurationParameter::_readDataTo(Configuration &conf, uint16_t offset, uint8_t *ptr, size_type maxSize) const
{
    // nothing to read
    if (_param.length() == 0) {
        return true;
    }
#if DEBUG_CONFIGURATION_GETHANDLE
    auto readSize =
#endif
    conf._eeprom.read(ptr, offset, _param.length(), maxSize);
    __DBG__addFlashReadSize(_param.getHandle(), readSize);
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

ConfigurationHelper::WriteableData::WriteableData(size_type length, ConfigurationParameter &parameter, Configuration &conf) :
    _buffer{},
    _length(getParameterLength(parameter.getType(), length)),
    _is_string(parameter.getType() == ParameterType::STRING)
{
    if (size() > sizeof(_buffer)) {
        _data = ConfigurationHelper::_allocator.allocate(size() + 4);
    }
    if (parameter.hasData()) {
        auto &param = parameter._getParam();
        memcpy(data(), param._readable, std::min(_length, param.length()));
        ConfigurationHelper::_allocator.deallocate(param._readable);
        param._readable = nullptr;
    }
}

ConfigurationHelper::WriteableData::~WriteableData()
{
    if (size() > sizeof(_buffer) && _data) {
        ConfigurationHelper::_allocator.deallocate(_data);
    }
}

void ConfigurationHelper::WriteableData::resize(size_type newLength, ConfigurationParameter &parameter, Configuration &conf)
{
    if (newLength == length()) {
        return;
    }

    auto &param = parameter._getParam();
    __LDBG_printf("new_length=%u length=%u data=%p", newLength, length(), data());

    auto ptr = ConfigurationHelper::_allocator.allocate(param.sizeOf(newLength) + 4);
    memcpy(ptr, param._writeable->data(), std::min(length(), newLength));
    param._writeable->setData(ptr, newLength);
}

void ConfigurationHelper::WriteableData::setData(uint8_t *ptr, size_type length)
{
    if (size() > sizeof(_buffer) && _data) {
        __LDBG_printf("free data=%p size=%u", _data, size());
        ConfigurationHelper::_allocator.deallocate(_data);
    }
    __LDBG_printf("set data=%p length=%u", ptr, _length);
    _data = ptr;
    _length = length;
}

ConfigurationHelper::size_type ConfigurationHelper::getParameterLength(ParameterType type, size_t length)
{
    switch (type) {
    case ParameterType::BYTE:
        return sizeof(uint8_t);
    case ParameterType::WORD:
        return sizeof(uint16_t);
    case ParameterType::DWORD:
        return sizeof(uint32_t);
    case ParameterType::QWORD:
        return sizeof(uint64_t);
    case ParameterType::FLOAT:
        return sizeof(float);
    case ParameterType::DOUBLE:
        return sizeof(double);
    case ParameterType::BINARY:
    case ParameterType::STRING:
        return (size_type)length;
    default:
        break;
    }
    __DBG_panic("invalid type=%u length=%u", type, length);
    return 0;
}

