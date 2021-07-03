/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonConfigReader.h"
#include <Buffer.h>
#include <JsonTools.h>
#include <coredecls.h>
#include "misc.h"
#include "DumpBinary.h"

#include "ConfigurationHelper.h"
#include "Configuration.h"

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include "DebugHandle.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

Configuration::Configuration(uint16_t size) :
    _eeprom(false, 4096),
    _readAccess(0),
    _size(size)
{
}

Configuration::~Configuration()
{
    clear();
}

void Configuration::clear()
{
    __LDBG_printf("params=%u", _params.size());
    _params.clear();
    _eeprom.discard();
}

void Configuration::discard()
{
    __LDBG_printf("discard params=%u", _params.size());
    for(auto &parameter: _params) {
        ConfigurationHelper::deallocate(parameter);
    }
    _eeprom.discard();
    _readAccess = 0;
}

void Configuration::release()
{
    __LDBG_printf("params=%u last_read=%d dirty=%d", _params.size(), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
    for(auto &parameter: _params) {
        if (!parameter.isWriteable()) {
            ConfigurationHelper::deallocate(parameter);
        }
        else if (parameter.hasDataChanged(*this) == false) {
            ConfigurationHelper::deallocate(parameter);
        }
    }
    _eeprom.discard();
    _readAccess = 0;
}

bool Configuration::read()
{
    __LDBG_printf("params=%u", _params.size());
    clear();
#if DEBUG_CONFIGURATION_GETHANDLE
    ConfigurationHelper::readHandles();
#endif
    if (!_readParams()) {
        __LDBG_printf("readParams()=false");
        clear();
        return false;
    }
    return true;
}

bool Configuration::write()
{
    __LDBG_printf("params=%u", _params.size());

    Header header;
    if (!_eeprom.read(reinterpret_cast<uint8_t *>(&header), kOffset, sizeof(header)) || (header.magic != CONFIG_MAGIC_DWORD)) {
        header.version = 1;
    }

    bool dirty = false;
    for (auto &parameter : _params) {
        if (parameter.hasDataChanged(*this)) {
            dirty = true;
            break;
        }
    }
    if (dirty == false) {
        __LDBG_printf("configuration did not change");
        return read();
    }

    Buffer buffer; // storage for parameter information and data
    uint16_t dataOffset = kParamsOffset;

    for (auto &parameter : _params) {
        // create copy
        auto param = parameter._getParam();
        if (parameter._getParam().isWriteable()) {
            // update length and remove writeable flag
            __LDBG_printf("writable: len=%u next_offset=%u", param._writeable->length(),  parameter._getParam().next_offset());
            param._length = param._writeable->length();
            param._is_writeable = false;
        }
        // write parameter headers
        __LDBG_printf("write_header: %s ofs=%d", parameter.toString().c_str(), buffer.length() + kParamsOffset);
        buffer.push_back(param._header);
        dataOffset += parameter._getParam().next_offset_unaligned() ? sizeof(ParameterHeaderType) : 0;
    }

#if DEBUG_CONFIGURATION
    if ((buffer.length() & 3)) {
        __DBG_panic("buffer=%u not aligned", buffer.length());
    }
#endif

    // new data offset is base offset + size of header + size of stored parameter information
    uint16_t _dataOffset = buffer.length() + kParamsOffset;
    __LDBG_assert_printf(_dataOffset == getDataOffset(_params.size()), "_dataOffset=%u mismatch=%u", _dataOffset, getDataOffset(_params.size()));

    _eeprom.begin(header.length + kParamsOffset);

    // write data
    for (auto &parameter : _params) {
        const auto &param = parameter._getParam();
        __LDBG_printf("write_data: %s ofs=%d %s", parameter.toString().c_str(), buffer.length() + kParamsOffset, __debugDumper(parameter, parameter._getParam().data(), parameter._param.length()).c_str());
        if (param.isWriteable()) {
            // write new data
            auto len = param._writeable->length();
            buffer.write(param._writeable->begin(), len);
            // align
            while((len & 3) != 0) {
                buffer.write(0);
                len++;
            }
        }
        else {
            // data did not change, read from eeprom into buffer
            buffer.write(_eeprom.getConstDataPtr() + dataOffset, param.next_offset());

            // auto len = param.next_offset();
            // if (!buffer.reserve(buffer.length() + len)) {
            //     __DBG_panic("out of memory: buffer=%u size=%u", buffer.length(), _size);
            // }
            // auto ptr = buffer.get();
            // buffer.setLength(buffer.length() + len);
            // if (!_eeprom.read(ptr, dataOffset, len)) {
            //     __DBG_printf("failed to read eeprom offset=%u len=%u", dataOffset, len);
            // }
        }
        dataOffset += param.next_offset();
        ConfigurationHelper::deallocate(parameter);
        parameter._getParam() = {};
    }

    if (buffer.length() > _size) {
        __DBG_panic("size exceeded: %u > %u", buffer.length(), _size);
    }

    auto len = buffer.length();
    _eeprom.begin(len + kParamsOffset); // update size

    auto headerPtr = _eeprom.getDataPtr() + kOffset;
    // // update header
    ::new(reinterpret_cast<void *>(headerPtr)) Header(header.version + 1, crc32(buffer.get(), len), len, _params.size());

    // update parameter info and data
    std::copy_n(buffer.get(), len, headerPtr + sizeof(Header));

    __LDBG_printf("CRC %08x, length %d", reinterpret_cast<Header *>(headerPtr)->crc, len);
    _eeprom.commit();

#if DEBUG_CONFIGURATION_GETHANDLE
    ConfigurationHelper::writeHandles();
#endif

    // _eeprom.dump(Serial, false, _offset, 160);

    _params.clear();

    return _readParams();
}

void Configuration::makeWriteable(ConfigurationParameter &param, size_type length)
{
    param._makeWriteable(*this, length);
}

const char *Configuration::getString(HandleType handle)
{
    __LDBG_printf("handle=%04x", handle);
    uint16_t offset;
    auto param = _findParam(ParameterType::STRING, handle, offset);
    if (param == _params.end()) {
        return emptyString.c_str();
    }
    return param->getString(*this, offset);
}

char *Configuration::getWriteableString(HandleType handle, size_type maxLength)
{
    __LDBG_printf("handle=%04x max_len=%u", handle, maxLength);
    auto &param = getWritableParameter<char *>(handle, maxLength);
    return param._getParam().string();
}

const uint8_t *Configuration::getBinary(HandleType handle, size_type &length)
{
    __LDBG_printf("handle=%04x", handle);
    uint16_t offset;
    auto param = _findParam(ParameterType::BINARY, handle, offset);
    if (param == _params.end()) {
        length = 0;
        return nullptr;
    }
    return param->getBinary(*this, length, offset);
}

void *Configuration::getWriteableBinary(HandleType handle, size_type length)
{
    __LDBG_printf("handle=%04x len=%u", handle, length);
    auto &param = getWritableParameter<void *>(handle, length);
    return param._getParam().data();
}

void Configuration::_setString(HandleType handle, const char *string, size_type length, size_type maxLength)
{
    __DBG_printf("handle=%04x length=%u max_len=%u", handle, length, maxLength);
    if (maxLength != (size_type)~0U) {
        if (length >= maxLength - 1) {
            length = maxLength - 1;
        }
    }
    _setString(handle, string, length);
}

void Configuration::_setString(HandleType handle, const char *string, size_type length)
{
    __DBG_printf("handle=%04x length=%u", handle, length);
    uint16_t offset;
    auto &param = _getOrCreateParam(ParameterType::STRING, handle, offset);
    param.setData(*this, (const uint8_t *)string, length);
}

void Configuration::setBinary(HandleType handle, const void *data, size_type length)
{
    __LDBG_printf("handle=%04x data=%p length=%u", handle, data, length);
    uint16_t offset;
    auto &param = _getOrCreateParam(ParameterType::BINARY, handle, offset);
    param.setData(*this, (const uint8_t *)data, length);
}

void Configuration::dump(Print &output, bool dirty, const String &name)
{
    Header header;
    if (!_eeprom.read(reinterpret_cast<uint8_t *>(&header), kOffset, sizeof(header)) || header.magic != CONFIG_MAGIC_DWORD) {
        header.version = 1;
    }

    uint16_t dataOffset = getDataOffset(_params.size());
    output.printf_P(PSTR("Configuration:\noffset=%d data_ofs=%u eeprom_size=%d params=%d len=%d version=%u\n"),
        kOffset,
        dataOffset,
        _eeprom.getSize(),
        _params.size(),
        _eeprom.getSize() - kOffset,
        header.version
    );
    output.printf_P(PSTR("min_mem_usage=%d header_size=%d Param_t::size=%d, ConfigurationParameter::size=%d, Configuration::size=%d\n"),
        sizeof(Configuration) + _params.size() * sizeof(ConfigurationParameter),
        sizeof(Configuration::Header), sizeof(ConfigurationParameter::Param_t),
        sizeof(ConfigurationParameter),
        sizeof(Configuration)
   );

    for (auto &parameter : _params) {
        DEBUG_HELPER_SILENT();
        auto display = true;
        auto &param = parameter._param;
        if (name.length()) {
            if (!name.equals(ConfigurationHelper::getHandleName(param.getHandle()))) {
                display = false;
            }
        }
        else if (dirty) {
            if (!parameter.isWriteable()) {
                display = false;
            }
        }
        //auto length =
        parameter.read(*this, dataOffset);
        DEBUG_HELPER_INIT();
        if (display) {
#if DEBUG_CONFIGURATION_GETHANDLE
            output.printf_P(PSTR("%s[%04x]: "), ConfigurationHelper::getHandleName(param.getHandle()), param.getHandle());
#else
            output.printf_P(PSTR("%04x: "), param.getHandle());
#endif
            output.printf_P(PSTR("type=%s ofs=%d[+%u] len=%d dirty=%u value: "), (const char *)parameter.getTypeString(parameter.getType()), dataOffset, param.next_offset(), /*calculateOffset(param.handle),*/ parameter.getLength(), parameter.isWriteable());
            parameter.dump(output);
        }
        dataOffset += param.next_offset();
    }
}

bool Configuration::isDirty() const
{
    for(auto &param: _params) {
        if (param.isWriteable()) {
            return true;
        }
    }
    return false;
}

void Configuration::exportAsJson(Print &output, const String &version)
{
    output.printf_P(PSTR(
        "{\n"
        "\t\"magic\": \"%#08x\",\n"
        "\t\"version\": \"%s\",\n"
        "\t\"config\": {\n"
    ), CONFIG_MAGIC_DWORD, version.c_str());

    uint16_t dataOffset = kParamsOffset;
    for (auto &parameter : _params) {
        auto &param = parameter._param;

        if (dataOffset != kParamsOffset) {
            output.print(F(",\n"));
        }

        output.printf_P(PSTR("\t\t\"%#04x\": {\n"), param.getHandle());
#if DEBUG_CONFIGURATION_GETHANDLE
        output.print(F("\t\t\t\"name\": \""));
        auto name = ConfigurationHelper::getHandleName(param.getHandle());
        JsonTools::printToEscaped(output, name, strlen(name), false);
        output.print(F("\",\n"));
#endif

        auto length = parameter.read(*this, dataOffset);
        output.printf_P(PSTR(
            "\t\t\t\"type\": %d,\n"
            "\t\t\t\"type_name\": \"%s\",\n"
            "\t\t\t\"length\": %d,\n"
            "\t\t\t\"data\": "
        ), parameter.getType(), parameter.getTypeString(parameter.getType()), length);
        parameter.exportAsJson(output);
        output.print('\n');
        dataOffset += param.next_offset();

        output.print(F("\t\t}"));
    }

    output.print(F("\n\t}\n}\n"));
}

bool Configuration::importJson(Stream &stream, HandleType *handles)
{
    JsonConfigReader reader(&stream, *this, handles);
    reader.initParser();
    return reader.parseStream();
}

Configuration::ParameterList::iterator Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset)
{
    offset = kParamsOffset;
    for (auto it = _params.begin(); it != _params.end(); ++it) {
        if (*it == handle && ((type == ParameterType::_ANY) || (it->_param.type() == type))) {
            //__LDBG_printf("%s FOUND", it->toString().c_str());
            return it;
        }
        offset += it->_param.next_offset();
    }
    __LDBG_printf("handle=%s[%04x] type=%s = NOT FOUND", ConfigurationHelper::getHandleName(handle), handle, (const char *)ConfigurationParameter::getTypeString(type));
    return _params.end();
}

ConfigurationParameter &Configuration::_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset)
{
    auto iterator = _findParam(ParameterType::_ANY, handle, offset);
    if (iterator == _params.end()) {
        _params.emplace_back(handle, type);
        __LDBG_printf("new param %s", _params.back().toString().c_str());
        __DBG__checkIfHandleExists("create", handle);
        return _params.back();
    }
    else if (type != iterator->_param.type()) {
        __DBG_panic("%s: new_type=%s type=%s different", ConfigurationHelper::getHandleName(iterator->getHandle()), iterator->toString().c_str(), (const char *)ConfigurationParameter::getTypeString(type));
    }
    __DBG__checkIfHandleExists("find", iterator->getHandle());
    return *iterator;
}

bool Configuration::_readParams()
{
    _eeprom.discard();

    Header header;
    if (!_eeprom.read(reinterpret_cast<uint8_t *>(&header), kOffset, sizeof(header))) {
        __LDBG_printf("read error offset %u", kOffset);
    }

#if DEBUG_CONFIGURATION
    DumpBinary dump(F("Header:"), DEBUG_OUTPUT);
    dump.dump(reinterpret_cast<uint8_t *>(&header), sizeof(header));
#endif

    while(true) {

        if (header.magic != CONFIG_MAGIC_DWORD) {
            __LDBG_printf("invalid magic %08x", header.magic);
#if DEBUG_CONFIGURATION
            __dump_binary_to(DEBUG_OUTPUT, &header, sizeof(header));
#endif
            break;
        }
        else if (header.crc == ~0U) {
            __LDBG_printf("invalid CRC %08x", header.crc);
            break;
        }
        else if (header.length == 0 || header.length > _size - sizeof(header)) {
            __LDBG_printf("invalid length %d", header.length);
            break;
        }

        // now we know the required size, initialize EEPROM
        _eeprom.begin(header.length + kParamsOffset);
        auto crc = crc32(_eeprom.getConstDataPtr() + kParamsOffset, header.length);
        _eeprom.discard();
        if (header.crc != crc) {
            __LDBG_printf("CRC mismatch %08x != %08x", crc, header.crc);
            break;
        }

        uint16_t offset = kParamsOffset;
        for(size_t i = 0; i < header.params; i++) {

            // __LDBG_printf("offset=%u calc=%u pos=%u/%u", offset, getDataOffset(i), i + 1, header.params);

            ParameterInfo param;
            if (!_eeprom.read(&param._header, offset, sizeof(param._header))) {
                __LDBG_assert_printf(false, "read error %u/%u offset=%u data=%u len=%u", i + 1, header.params, offset, getDataOffset(i), sizeof(param._header));
                break;
            }
            if (param.type() == ParameterType::_INVALID) {
                __LDBG_assert_printf(false, "read error %u/%u type=%u offset=%u data=%u", i + 1, header.params, param.type(), offset, getDataOffset(i));
                break;
            }
            __LDBG_assert_panic(param.isWriteable() == false, "writeable");

            offset += sizeof(param._header);
            _params.emplace_back(param._header);

            if (_params.size() == header.params) {
                __LDBG_printf("_dataOffset=%u params=%u", getDataOffset(header.params), _params.size());
                __LDBG_assert_printf(offset == getDataOffset(header.params), "offset=%u mismatch _dataOffset=%u", offset, getDataOffset(header.params));
                _eeprom.discard();
                return true;
            }
        }

        break;
    }

    clear();
    return false;
}

String Configuration::__debugDumper(ConfigurationParameter &param, const uint8_t *data, size_t len)
{
    PrintString str = F("data");
    if (param._param.isString()) {
        str.printf_P(PSTR("[%u]='"), len);
        auto ptr = data;
        while (len--) {
            str += (char)pgm_read_byte(ptr++);
        }
        str += '\'';
    }
    else {
        str += '=';
        DumpBinary dump(str);
        dump.setPerLine((uint8_t)len);
        dump.dump(data, len);
        str.rtrim();
    }
    return str;
}
