/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonConfigReader.h"
#include <Buffer.h>
#include <JsonTools.h>
#include <interrupts.h>
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
}

void Configuration::discard()
{
    __LDBG_printf("discard params=%u", _params.size());
    for(auto &parameter: _params) {
        ConfigurationHelper::deallocate(parameter);
    }
    _readAccess = 0;
}

void Configuration::release()
{
    __LDBG_printf("params=%u last_read=%d dirty=%d", _params.size(), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
    for(auto &parameter: _params) {
        if (!parameter.isWriteable()) {
            ConfigurationHelper::deallocate(parameter);
            // if (_readAccess) {
            //     parameter._getParam()._usage._counter2 += (millis() - _readAccess) / 1000;
            // }
        }
        else if (parameter.hasDataChanged(*this) == false) {
            ConfigurationHelper::deallocate(parameter);
            // if (_readAccess) {
            //     parameter._getParam()._usage._counter2 += (millis() - _readAccess) / 1000;
            // }
        }
    }
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
    auto address = ConfigurationHelper::getFlashAddress(kHeaderOffset);

    // get header
    if (!ESP.flashRead(address, header, sizeof(header))) {
        __DBG_panic("cannot read header offset=%u", ConfigurationHelper::getOffsetFromFlashAddress(address));
    }
    if (!header) {
        __DBG_panic("invalid magic=%08x", header.magic());
    }

    // check dirty data for changes
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

    // create new configuration in memory
    {
        // locked skope
        esp8266::InterruptLock lock;
        Buffer buffer;

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
        }

#if DEBUG_CONFIGURATION
        if ((buffer.length() & 3)) {
            __DBG_panic("buffer=%u not aligned", buffer.length());
        }
#endif

        // get offset of stored data
        uint32_t oldAddress = ConfigurationHelper::getFlashAddress(getDataOffset(header.numParams()));

        // write data
        for (auto &parameter : _params) {
            const auto &param = parameter._getParam();
            __LDBG_printf("write_data: %s ofs=%d %s", parameter.toString().c_str(), buffer.length() + kParamsOffset, __debugDumper(parameter, parameter._getParam().data(), parameter._param.length()).c_str());
            if (param.isWriteable()) {
                // write new data
                auto len = param._writeable->length();
                buffer.write(param._writeable->begin(), len);
                // align to match param.next_offset()
                while((len & 3) != 0) {
                    buffer.write(0);
                    len++;
                }
            }
            else {
                // data did not change
                // read from flash
                auto len = param.next_offset();
                if (!buffer.reserve(buffer.length() + len)) {
                    __DBG_panic("out of memory: buffer=%u size=%u", buffer.length(), _size);
                    break;
                }
                auto ptr = buffer.end();
                buffer.setLength(buffer.length() + len);
                if (!ESP.flashRead(oldAddress, ptr, len)) {
                    __DBG_panic("failed to read flash address=%u len=%u", oldAddress, len);
                    break;
                }
            }
            // next_offset() returns the offset of the data stored in the flash memory, not the current data in param
            oldAddress += param.next_offset();
            ConfigurationHelper::deallocate(parameter);
            parameter._getParam() = {};
        }

        if (buffer.length() > _size) {
            __DBG_panic("size exceeded: %u > %u", buffer.length(), _size);
        }

        // update header
        header.update(header.version() + 1, _params.size(), buffer.length());
        header.calcCrc(buffer.get(), buffer.length());

        // format flash and copy stored data
        address = ConfigurationHelper::getFlashAddress();

        std::unique_ptr<uint8_t[]> data;
        if constexpr (kHeaderOffset != 0) {
            // load data before the offset
            auto data = std::unique_ptr<uint8_t[]>(new uint8_t[kHeaderOffset]); // copying the previous data could be done before allocating "buffer" after the interrupt lock
            if (!data) {
                __DBG_panic("failed to allocate memory (preoffset data)");
            }
            if (!ESP.flashRead(address, data.get(), kHeaderOffset)) {
                __DBG_panic("failed to read flash (preoffset data)");
            }
        }

        // erase sector
        // TODO if something goes wrong here, all data is lost
        // add redundancy writing to multiple sectors, the header has an incremental version number
        // append mode can be used to fill the sector before erasing
        if (!ESP.flashEraseSector(address / SPI_FLASH_SEC_SIZE)) {
            __DBG_panic("failed to write configuration (erase)");
        }

        if constexpr (kHeaderOffset != 0) {
            // restore data before the offset
            ESP.flashWrite(address, data.get(), kHeaderOffset);
            // if this fails, we cannot do anything anymore
            // the sector has been erased already
            data.reset();
        }

        // write header
        address += kHeaderOffset;
        if (!ESP.flashWrite(address, header, sizeof(header))) {
            __DBG_panic("failed to write configuration (write header)");
        }
        // params and data
        address += sizeof(header);
        if (!ESP.flashWrite(address, buffer.get(), buffer.length())) {
            __DBG_panic("failed to write configuration (write buffer)");
        }

    }


#if DEBUG_CONFIGURATION_GETHANDLE
    ConfigurationHelper::writeHandles();
#endif

    // re-read parameters
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
    Header header;
    auto address = ConfigurationHelper::getFlashAddress(kHeaderOffset);

    if (!ESP.flashRead(address, header, sizeof(header)) || !header) {
        output.printf_P(PSTR("cannot read header, magic=0x%08x"), header.magic());
        return;
    }

    uint16_t dataOffset = getDataOffset(_params.size());
    output.printf_P(PSTR("Configuration:\noffset=%d data_ofs=%u params=%d len=%d version=%u\n"),
        kHeaderOffset,
        dataOffset,
        _params.size(),
        _size - kHeaderOffset,
        header.version()
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
            output.printf_P(PSTR("type=%s ofs=%d[+%u] len=%d dirty=%u value: "), (const char *)parameter.getTypeString(parameter.getType()),
                dataOffset, param.next_offset(), parameter.getLength(), parameter.isWriteable()
            );
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

    uint16_t dataOffset = getDataOffset(_params.size());
    bool n = false;
    for (auto &parameter : _params) {

        if (n) {
            output.print(F(",\n"));
        }
        else {
            n = true;
        }

        auto &param = parameter._param;
        output.printf_P(PSTR("\t\t\"%#04x\": {\n"), param.getHandle());
#if DEBUG_CONFIGURATION_GETHANDLE
        output.print(F("\t\t\t\"name\": \""));
        auto name = ConfigurationHelper::getHandleName(param.getHandle());
        JsonTools::printToEscaped(output, name, strlen(name));
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
    offset = getDataOffset(_params.size());
    for (auto it = _params.begin(); it != _params.end(); ++it) {
        if (*it == handle && ((type == ParameterType::_ANY) || (it->_param.type() == type))) {
            //__LDBG_printf("%s FOUND", it->toString().c_str());
            // it->_getParam()._usage._counter++;
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
    Header header;

    if (!ESP.flashRead(ConfigurationHelper::getFlashAddress(kHeaderOffset), header, sizeof(header))) {
        __LDBG_printf("read error offset %u", kHeaderOffset);
        clear();
        return false;
    }

#if DEBUG_CONFIGURATION
    DumpBinary dump(F("Header:"), DEBUG_OUTPUT);
    dump.dump(header, sizeof(header));
#endif

    do {
        if (!header) {
            __LDBG_printf("invalid magic 0x%08x", header.magic());
            break;
        }
        else if (header.crc() == ~0U) {
            __LDBG_printf("invalid CRC 0x%08x", header.crc());
            break;
        }
        else if (!header.isLengthValid(_size)) {
            __LDBG_printf("invalid length %d", header.length());
            break;
        }

        // read all data and validate CRC
        auto address = ConfigurationHelper::getFlashAddress(kParamsOffset);
        auto endAddress = address + header.length();
        auto crc = header.initCrc();
        uint16_t paramIdx = 0;
        uint32_t buf[128 / sizeof(uint32_t)];
        while(address < endAddress) {
            auto read = std::min<size_t>(endAddress - address, sizeof(buf));
            if (!ESP.flashRead(address, buf, read)) { // using uint32_t * since buf is aligned
                break;
            }
            crc = crc32(buf, read, crc);

            // copy parameters from data block
            auto startPtr = reinterpret_cast<uint8_t *>(buf);
            auto endPtr = startPtr + read;
            auto ptr = startPtr;
            for(; paramIdx < header.numParams() && ptr + sizeof(ParameterInfo()._header) < endPtr; paramIdx++) {
                ParameterInfo param;

                if constexpr (sizeof(param._header) == sizeof(uint32_t)) {
                    param._header = *reinterpret_cast<uint32_t *>(ptr);
                }
                else {
                    memcpy(&param._header, ptr, sizeof(param._header));
                }
                ptr += sizeof(param._header);

                // __DBG_printf("READ PARAMS idx=%u handle=%04x type=%u address=%u buf_ofs=%u", paramIdx, param._handle, param.type(), address, (ptr - startPtr));
                if (param.type() == ParameterType::_INVALID) {
                    __LDBG_printf("read error %u/%u type=%u offset=%u data=%u", paramIdx + 1, header.numParams(), param.type(), ConfigurationHelper::getOffsetFromFlashAddress(address + (ptr - startPtr)), getDataOffset(paramIdx));
                    break;
                }
                _params.emplace_back(param._header);
            }
            address += read;
        }

        if (!header.validateCrc(crc)) {
            __LDBG_printf("CRC mismatch 0x%08x<>0x%08x", crc, header.crc());
            break;
        }
        if (_params.size() == header.numParams()) {
            return true;
        }

    }
    while(false);

#if DEBUG_CONFIGURATION
    __dump_binary_to(DEBUG_OUTPUT, &header, sizeof(header));
#endif
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
