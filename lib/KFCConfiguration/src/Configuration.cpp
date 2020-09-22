/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonConfigReader.h"
#include <Buffer.h>
#include <JsonTools.h>
#include "misc.h"
#include "DumpBinary.h"

#include "ConfigurationHelper.h"
#include "Configuration.h"
#include "Allocator.h"

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#include "DebugHandle.h"

#include "Allocator.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

Configuration::Configuration(uint16_t offset, uint16_t size) :
    _eeprom(false, 4096),
    _readAccess(0),
    _offset(offset),
    _dataOffset(offset + sizeof(Header_t)),
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
    _dataOffset = _offset + sizeof(Header_t);
}

void Configuration::discard()
{
    __LDBG_printf("discard params=%u", _params.size());
    std::for_each(_params.begin(), _params.end(), [](ConfigurationParameter &parameter) {
        ConfigurationHelper::_allocator.deallocate(parameter);
    });
    ConfigurationHelper::_allocator.release();
    _eeprom.discard();
    _readAccess = 0;
}

void Configuration::release()
{
    __LDBG_printf("params=%u last_read=%d dirty=%d", _params.size(), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
    std::for_each(_params.begin(), _params.end(), [this](ConfigurationParameter &parameter) {
        if (!parameter.isWriteable()) {
            ConfigurationHelper::_allocator.deallocate(parameter);
        }
        else if (parameter.hasDataChanged(*this) == false) {
            ConfigurationHelper::_allocator.deallocate(parameter);
        }
    });
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

    _eeprom.begin();

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
    uint16_t dataOffset = _offset + sizeof(Header_t);

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
        __LDBG_printf("write_header: %s ofs=%d", parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)));
        buffer.push_back(param._header);
        dataOffset += parameter._getParam().next_offset() ? sizeof(ParameterHeaderType) : 0;
    }

    __LDBG_assert_printf(dataOffset == _dataOffset, "wrong data_offset=%u _data_offset=%u", dataOffset, _dataOffset);

    // new data offset is base offset + size of header + size of stored parameter information
    _dataOffset = _offset + (uint16_t)(buffer.length() + sizeof(Header_t));
#if DEBUG_CONFIGURATION
    auto calcOfs = _offset + (sizeof(ParameterHeaderType) * _params.size()) + sizeof(Header_t);
    if (_dataOffset != calcOfs) {
        __DBG_panic("real_ofs=%u ofs=%u", _dataOffset, calcOfs);

    }
#endif

    // write data
    for (auto &parameter : _params) {
        const auto &param = parameter._getParam();
        __LDBG_printf("write_data: %s ofs=%d %s", parameter.toString().c_str(), (int)(buffer.length() + _offset + sizeof(Header_t)), __debugDumper(parameter, parameter._getParam().data(), parameter._param.length()).c_str());
        if (param.isWriteable()) {
            // write new data
            buffer.write(param._writeable->begin(), param._writeable->length());
        }
        else {
            // data did not change, read from eeprom into buffer
            buffer.write(_eeprom.getConstDataPtr() + dataOffset, param.next_offset());
        }
        dataOffset += param.next_offset();
        ConfigurationHelper::_allocator.deallocate(parameter);
        parameter._getParam() = {};
    }

    if (buffer.length() > _size) {
        __DBG_panic("size exceeded: %u > %u", buffer.length(), _size);
    }

    auto len = (uint16_t)buffer.length();
    auto eepromSize = _offset + len + sizeof(Header_t);
    _eeprom.begin(eepromSize); // update size

    auto headerPtr = _eeprom.getDataPtr() + _offset;
    auto &header = *reinterpret_cast<Header_t *>(headerPtr);
    // update header
    header = Header_t({ CONFIG_MAGIC_DWORD, crc16_update(buffer.get(), len), len, _params.size() });

    // update parameter info and data
    memcpy(headerPtr + sizeof(header), buffer.get(), len);

    __LDBG_printf("CRC %04x, length %d", header.crc, len);
    _eeprom.commit();

#if DEBUG_CONFIGURATION_GETHANDLE
    ConfigurationHelper::writeHandles();
#endif

    // _eeprom.dump(Serial, false, _offset, 160);

    _params.clear();
    ConfigurationHelper::_allocator.release();
    _dataOffset = _offset + sizeof(Header_t);

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
    __LDBG_printf("handle=%04x length=%u max_len=%u", handle, length, maxLength);
    if (maxLength != (size_type)~0U) {
        if (length >= maxLength - 1) {
            length = maxLength - 1;
        }
    }
    _setString(handle, string, length);
}

void Configuration::_setString(HandleType handle, const char *string, size_type length)
{
    __LDBG_printf("handle=%04x length=%u", handle, length);
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
    output.printf_P(PSTR("Configuration:\nofs=%d base_ofs=%d eeprom_size=%d params=%d len=%d\n"), _dataOffset, _offset, _eeprom.getSize(), _params.size(), _eeprom.getSize() - _offset);
    output.printf_P(PSTR("min_mem_usage=%d header_size=%d Param_t::size=%d, ConfigurationParameter::size=%d, Configuration::size=%d\n"), sizeof(Configuration) + _params.size() * sizeof(ConfigurationParameter), sizeof(Configuration::Header_t), sizeof(ConfigurationParameter::Param_t), sizeof(ConfigurationParameter), sizeof(Configuration));

    uint16_t dataOffset = (uint16_t)(_offset + sizeof(Header_t) + (_params.size() * sizeof(ParameterHeaderType)));
    output.printf_P(PSTR("_dataOffset=%u _offset=%u dataOffset=%u\n"), _dataOffset, _offset, dataOffset);
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
    output.printf_P(PSTR("{\n\t\"magic\": \"%#08x\",\n\t\"version\": \"%s\",\n"), CONFIG_MAGIC_DWORD, version.c_str());
    output.print(F("\t\"config\": {\n"));

    uint16_t dataOffset = _dataOffset;
    for (auto &parameter : _params) {
        auto &param = parameter._param;

        if (dataOffset != _dataOffset) {
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
        output.printf_P(PSTR("\t\t\t\"type\": %d,\n"), parameter.getType());
        output.printf_P(PSTR("\t\t\t\"type_name\": \"%s\",\n"), (const char *)parameter.getTypeString(parameter.getType()));
        output.printf_P(PSTR("\t\t\t\"length\": %d,\n"), length);
        output.print(F("\t\t\t\"data\": "));
        parameter.exportAsJson(output);
        output.print(F("\n"));
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
    offset = _dataOffset;
    if (type == ParameterType::_ANY) {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle) {
                //__LDBG_printf("%s FOUND", it->toString().c_str());
                return it;
            }
            offset += it->_param.next_offset();
        }
    }
    else {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (*it == handle && it->_param.type() == type) {
                //__LDBG_printf("%s FOUND", it->toString().c_str());
                return it;
            }
            offset += it->_param.next_offset();
        }
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

uint16_t Configuration::_readHeader(uint16_t offset, HeaderAligned_t &hdr)
{
    _eeprom.read(hdr.headerBuffer, (uint16_t)offset, (uint16_t)sizeof(hdr.header), (uint16_t)sizeof(hdr.headerBuffer));
    return offset + sizeof(hdr.header);
}

bool Configuration::_readParams()
{
    uint16_t offset = _offset;
    HeaderAligned_t hdr;

    _eeprom.discard();
    offset = _readHeader(offset, hdr);
    __LDBG_printf("offset=%u params_ofs=%u", _offset, offset);

#if DEBUG_CONFIGURATION
    DumpBinary dump(F("Header:"), DEBUG_OUTPUT);
    dump.dump((const uint8_t *)&hdr, sizeof(hdr.header));
#endif

    while(true) {

        if (hdr.header.magic != CONFIG_MAGIC_DWORD) {
            __LDBG_printf("invalid magic %08x", hdr.header.magic);
#if DEBUG_CONFIGURATION
            _eeprom.dump(Serial, false, _offset, 160);
#endif
            break;
        }
        else if (hdr.header.crc == -1) {
            __LDBG_printf("invalid CRC %04x", hdr.header.crc);
            break;
        }
        else if (hdr.header.length == 0 || hdr.header.length > _size - sizeof(hdr.header)) {
            __LDBG_printf("invalid length %d", hdr.header.length);
            break;
        }

        // now we know the required size, initialize EEPROM
        _eeprom.begin(_offset + sizeof(hdr.header) + hdr.header.length);

        uint16_t crc = crc16_update(_eeprom.getConstDataPtr() + _offset + sizeof(hdr.header), hdr.header.length);
        if (hdr.header.crc != crc) {
            __LDBG_printf("CRC mismatch %04x != %04x", crc, hdr.header.crc);
            break;
        }

        //offset += (sizeof(param._header) * hdr.header.params);
        _dataOffset =  offset + (sizeof(ParameterHeaderType) * hdr.header.params);

        for(size_t i = 0; i <= hdr.header.params; i++) {

            if (_params.size() == hdr.header.params) {
                __LDBG_printf("_dataOffset=%u params=%u", _dataOffset, _params.size());
                __LDBG_assert_printf(offset == _dataOffset, "offset=%u mismatch _dataOffset=%u", offset, _dataOffset);
                _eeprom.discard();
                return true;
            }

            ParameterInfo param;
            if (
                (_eeprom.read((uint8_t *)&param._header, offset, sizeof(param._header), sizeof(param._header)) != sizeof(param._header)) ||
                (param.type() == ParameterType::_INVALID)
            ) {
                __LDBG_assert_printf(false, "read error type=%u", param.type());
                break;
            }
            __LDBG_assert_panic(param.isWriteable() == false, "writeable");

            offset += sizeof(param._header);
            _params.emplace_back(param._header);
            if (_params.size() != i + 1) {
                __LDBG_assert_printf(false, "emplace_back failed");
                break;
            }
        }

        break;
    }

    clear();
    return false;
}

#if DEBUG_CONFIGURATION

String Configuration::__debugDumper(ConfigurationParameter &param, const uint8_t *data, size_t len, bool progmem)
{
    PrintString str = F("data");
progmem=true;// treat everything as PROGMEM
    if (param._param.isString()) {
        str.printf_P(PSTR("[%u]='"), len);
        if (progmem) {
            auto ptr = data;
            while (len--) {
                str += (char)pgm_read_byte(ptr++);
            }
        }
        else {
            auto ptr = data;
            while (len--) {
                str += (char)*ptr++;
            }
        }
        str += '\'';
    }
    else {
        str += '=';
        DumpBinary dump(str);
        dump.setPerLine((uint8_t)len);
        if (progmem) {
            auto pPtr = malloc(len);
            memcpy_P(pPtr, data, len);
            dump.dump((uint8_t *)pPtr, len);
            free(pPtr);
        }
        else {
            dump.dump(data, len);
        }
        str.rtrim();
    }
    return str;
}

#endif
