/**
* Author: sascha_lammers@gmx.de
*/

// ESP8266 configuration is stored in flash memory
// ESP32 configuration is stored in NVS

#include <Buffer.h>
#if ESP8266
#include <interrupts.h>
#endif
#if ESP32
#include <nvs.h>
#define NVS_PARTITION_NAME "nvs"
#endif
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
#include "Configuration.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 26812)
#endif

Configuration::Configuration(uint16_t size) :
    #if ESP32
        _handle(0),
        _name("kfcfw_config"),
    #endif
    _readAccess(0),
    _size(size)
{
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

Configuration::WriteResultType Configuration::erase()
{
    #if ESP32
        _nvs_open();
        // clear previous configuration
        esp_err_t err;
        if ((err = nvs_erase_all(_handle)) != ESP_OK) {
            __DBG_printf_E("failed to erase NVS name=%s err=%08x", _name, err);
            return WriteResultType::NVS_ERASE_ALL;
        }

        #if DEBUG_CONFIGURATION || 1
            nvs_stats_t stats;
            if ((err = nvs_get_stats(NVS_PARTITION_NAME, &stats)) == ESP_OK) {
                __DBG_printf_N("NVS stats free=%u ns_count=%u total=%u used=%u", stats.free_entries, stats.namespace_count, stats.total_entries, stats.used_entries);
            }
            else {
                __DBG_printf_E("failed to get stats name=%s err=%08x", NVS_PARTITION_NAME, err);
            }
        #endif
    #endif
    return WriteResultType::SUCCESS;
}

Configuration::WriteResultType Configuration::write()
{
    __LDBG_printf("params=%u", _params.size());

    #if ESP32
        _nvs_open();
        Header header;
        esp_err_t err;

        size_t size = sizeof(header);
        if ((err = nvs_get_blob(_handle, "header", header, &size)) != ESP_OK) {
            __DBG_printf("cannot read header err=%08x", err);
            header = Header();
        }
        else if (size != sizeof(header)) {
            __DBG_printf("cannot read header size=%u stored=%u", sizeof(header), size);
            header = Header();
        }

        // update header
        header.update(header.version() + 1, _params.size(), Header::getParamsLength(_params.size()));
        Buffer buffer;

        // write new data and create params
        for (auto &parameter : _params) {
            auto param = parameter._getParam();
            if (parameter._getParam().isWriteable()) {
                param._length = param._writeable->length();
                param._is_writeable = false;
                if ((err = nvs_set_blob(_handle, _nvs_key_handle_name(param.type(), param.getHandle()), param._writeable->begin(), param._writeable->length())) != ESP_OK) {
                    __DBG_printf_E("failed to write data handle=%04x size=%u err=%08x", param.getHandle(), param._writeable->length(), err);
                    return WriteResultType::NVS_SET_BLOB_ERROR;
                }
            }
            // write parameter headers
            __LDBG_printf("write_header: %s ofs=%d", parameter.toString().c_str(), buffer.length() + kParamsOffset);
            buffer.push_back(param._header);
        }

        // write header
        if ((err = nvs_set_blob(_handle, "header", header, sizeof(header))) != ESP_OK) {
            __DBG_printf_E("failed to write header size=%u err=%08x", sizeof(header), err);
            return WriteResultType::NVS_SET_BLOB_ERROR;
        }

        // write params
        if ((err = nvs_set_blob(_handle, "params", buffer.begin(), buffer.length())) != ESP_OK) {
            __DBG_printf_E("failed to write params size=%u err=%08x", buffer.length(), err);
            return WriteResultType::NVS_SET_BLOB_ERROR;
        }

        // commit pending data
        if ((err = nvs_commit(_handle)) != ESP_OK) {
            __DBG_printf_E("failed to commit NVS name=%s err=%08x", _name, err);
            return WriteResultType::NVS_COMMIT_ERROR;
        }

        #if DEBUG_CONFIGURATION || 1
            nvs_stats_t stats;
            if ((err = nvs_get_stats(NVS_PARTITION_NAME, &stats)) == ESP_OK) {
                __DBG_printf_N("NVS stats free=%u ns_count=%u total=%u used=%u", stats.free_entries, stats.namespace_count, stats.total_entries, stats.used_entries);
            }
            else {
                __DBG_printf_E("failed to get stats name=%s err=%08x", NVS_PARTITION_NAME, err);
            }
            dump(DEBUG_OUTPUT);
        #endif

        return WriteResultType::SUCCESS;

    #else

        Header header;
        auto address = ConfigurationHelper::getFlashAddress(kHeaderOffset);

        // get header
        if (!flashRead(address, header, sizeof(header))) {
            __DBG_printf("cannot read header offset=%u address=0x%08x aligned=%u", ConfigurationHelper::getOffsetFromFlashAddress(address), address, (address % sizeof(uint32_t)) == 0);
            header = Header();
            // return WriteResultType::READING_HEADER_FAILED;
        }

        if (header) {
            __LDBG_printf("copying existing data");
            // check dirty data for changes
            bool dirty = false;
            for (const auto &parameter : _params) {
                if (parameter.hasDataChanged(*this)) {
                    dirty = true;
                    break;
                }
            }
            if (dirty == false) {
                __LDBG_printf("configuration did not change");
                return read() ? WriteResultType::SUCCESS : WriteResultType::READING_CONF_FAILED;
            }
        }

        // create new configuration in memory
        {
            // locked skope
            InterruptLock lock;
            Buffer buffer;

            for (const auto &parameter : _params) {
                // create copy
                auto param = parameter._getParam();
                if (parameter._getParam().isWriteable()) {
                    // update length and remove writeable flag
                    __LDBG_printf("writable: len=%u size=%u old_next_ofs=%u data=%s",
                        param._writeable->length(),
                        param._writeable->size(),
                        parameter._getParam().next_offset(),
                        printable_string(parameter._param.data(), param._writeable->length(), 32).c_str()
                    );
                    param._length = param._writeable->length();
                    param._is_writeable = false;
                }
                // write parameter headers
                __LDBG_printf("write_header: %s ofs=%d buflen=%u", parameter.toString().c_str(), kParamsOffset, buffer.length());
                buffer.push_back(param._header);
            }

            #if DEBUG_CONFIGURATION
                if ((buffer.length() & 3) != 0) {
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
                    // fill up to size
                    auto size = param._writeable->size();
                    while (len < size) {
                        buffer.write(0);
                        len++;
                    }
                    // align to match param.next_offset()
                    while((len & 3) != 0) {
                        buffer.write(0);
                        len++;
                    }
                    __LDBG_printf("len=%u next_ofs=%u old_next_ofs=%u", len, param.next_offset(), param.old_next_offset());
                    // update old address for the next parameter
                    oldAddress += param.old_next_offset();
                }
                else {
                    // data did not change
                    // reserve memory and read from flash
                    auto len = param.next_offset();
                    if (!buffer.reserve(buffer.length() + len)) {
                        __DBG_printf("out of memory: buffer=%u size=%u", buffer.length(), _size);
                        return WriteResultType::OUT_OF_MEMORY;
                    }
                    auto ptr = buffer.end();
                    buffer.setLength(buffer.length() + len);
                    if (!flashRead(oldAddress, ptr, len)) {
                        __DBG_panic("failed to read flash address=%u len=%u", oldAddress, len);
                        return WriteResultType::READING_PREV_CONF_FAILED;
                    }
                    __LDBG_printf("oldAddress=%u data=%s len=%u", oldAddress, printable_string(ptr, len, 32).c_str(), len);
                    // next_offset() returns the offset of the data stored in the flash memory, not the current data in param
                    oldAddress += param.next_offset();
                }
                ConfigurationHelper::deallocate(parameter);
                parameter._getParam() = ConfigurationHelper::ParameterInfo();
            }

            if (buffer.length() > _size) {
                __DBG_printf("size exceeded: %u > %u", buffer.length(), _size);
                return WriteResultType::MAX_SIZE_EXCEEDED;
            }

            // update header
            header = Header(header.version() + 1, buffer.length(), _params.size());
            header.calcCrc(buffer.get(), buffer.length());

            // format flash and copy stored data
            address = ConfigurationHelper::getFlashAddress();

            #if CONFIGURATION_HEADER_OFFSET
                std::unique_ptr<uint8_t[]> data;
                if __CONSTEXPR17 (kHeaderOffset != 0) {
                    // load data before the offset
                    data.reset(new uint8_t[kHeaderOffset]); // copying the previous data could be done before allocating "buffer" after the interrupt lock
                    if (!data) {
                        __DBG_printf("failed to allocate memory=%u (preoffset data)", kHeaderOffset);
                        return WriteResultType::OUT_OF_MEMORY;
                    }
                    if (!flashRead(address, data.get(), kHeaderOffset)) {
                        __DBG_printf("failed to read flash (preoffset data, address=0x%08x size=%u)", address, kHeaderOffset);
                        return WriteResultType::FLASH_READ_ERROR;
                    }
                }
            #endif

            __LDBG_printf("flash write %08x sector %u", address, address / SPI_FLASH_SEC_SIZE);

            // erase sector
            // TODO if something goes wrong here, all data is lost
            // add redundancy writing to multiple sectors, the header has an incremental version number
            // append mode can be used to fill the sector before erasing
            if (!flashEraseSector(address / SPI_FLASH_SEC_SIZE)) {
                __DBG_printf("failed to write configuration (erase)");
                return WriteResultType::FLASH_ERASE_ERROR;
            }

            #if CONFIGURATION_HEADER_OFFSET
                if __CONSTEXPR17 (kHeaderOffset != 0) {
                    // restore data before the offset
                    flashWrite(address, data.get(), kHeaderOffset);
                    // if this fails, we cannot do anything anymore
                    // the sector has been erased already
                    data.reset();

                    // move header offset
                    address += kHeaderOffset;
                }
            #endif

            if (!flashWrite(address, header, sizeof(header))) {
                __DBG_printf("failed to write configuration (write header, address=0x%08x, size=%u, aligned=%u)", address, sizeof(header), (address % sizeof(uint32_t)) == 0);
                return WriteResultType::FLASH_WRITE_ERROR;
            }
            // params and data
            address += sizeof(header);
            if (!flashWrite(address, buffer.get(), buffer.length())) {
                __DBG_printf("failed to write configuration (write buffer, address=0x%08x, size=%u)", address, buffer.length());
                return WriteResultType::FLASH_WRITE_ERROR;
            }

        }

        #if DEBUG_CONFIGURATION_GETHANDLE
            ConfigurationHelper::writeHandles();
        #endif

        // re-read parameters
        _params.clear();
        auto result = _readParams();
        if (!result) {
            _params.clear();
            return WriteResultType::READING_CONF_FAILED;
        }
        return WriteResultType::SUCCESS;

    #endif
}

void Configuration::dump(Print &output, bool dirty, const String &name)
{
    Header header;
    auto address = ConfigurationHelper::getFlashAddress(kHeaderOffset);

    #if ESP32

        // read header to display details
        size_t size = sizeof(header);
        esp_err_t err;
        if ((err = nvs_get_blob(_handle, "header", header, &size)) != ESP_OK) {
            __DBG_printf_E("failed to read header size=%u err=%08x", sizeof(header), err);
            return;
        }
        else if (size != sizeof(header)) {
            __DBG_printf_E("failed to read header size=%u read=%u", sizeof(header), size);
            return;
        }

    #else

        // read header to display details
        if (!flashRead(address, header, sizeof(header)) || !header) {
            output.printf_P(PSTR("cannot read header, magic=0x%08x"), header.magic());
            return;
        }

    #endif

    uint16_t dataOffset = getDataOffset(_params.size());
    output.printf_P(PSTR("Configuration:\noffset=%d data_ofs=%u size=%d len=%d version=%u\n"),
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
        parameter.read(*this, dataOffset);
        DEBUG_HELPER_INIT();
        if (display) {
            #if DEBUG_CONFIGURATION_GETHANDLE
                output.printf_P(PSTR("%s[%04x]: "), ConfigurationHelper::getHandleName(param.getHandle()), param.getHandle());
            #else
                output.printf_P(PSTR("%04x: "), param.getHandle());
            #endif
            output.printf_P(PSTR("type=%s ofs=%d[+%u] size=%d dirty=%u value: "), (const char *)parameter.getTypeString(parameter.getType()),
                dataOffset, param.next_offset(), parameter._param.size(), parameter.isWriteable()
            );
            parameter.dump(output);
        }
        dataOffset += param.next_offset();
    }
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

bool Configuration::_readParams()
{
    Header header;

    #if ESP32

        _nvs_open();

        // read header
        esp_err_t err;
        size_t size = sizeof(header);
        if ((err = nvs_get_blob(_handle, "header", header, &size)) != ESP_OK) {
            __DBG_printf_E("failed to read header size=%u err=%08x", sizeof(header), err);
            return false;
        }
        else if (size != sizeof(header)) {
            __DBG_printf_E("failed to read header size=%u read=%u", sizeof(header), size);
            return false;
        }

        // read parameters
        std::vector<ParameterHeaderType> params;
        params.resize(header.numParams());
        if (params.size() != header.numParams()) {
            __DBG_printf_E("cannot allocate memory");
            return false;
        }
        size = header.getParamsLength();
        if ((err = nvs_get_blob(_handle, "params", params.data(), &size)) != ESP_OK) {
            __DBG_printf_E("cannot read params size=%u err=%u", header.getParamsLength(), err);
            return false;
        }
        else if (size != header.getParamsLength()) {
            __DBG_printf_E("cannot read params size=%u read=%u", header.getParamsLength(), size);
            return false;
        }

        // copy parameter headers
        _params.clear();
        for(auto pHeader: params) {
            ParameterInfo param;
            param._header = pHeader;
            _params.emplace_back(param._header);
        }

        #if DEBUG_CONFIGURATION
            DumpBinary dump(F("Header:"), DEBUG_OUTPUT);
            dump.dump(header, sizeof(header));
        #endif

        return true;

    #else

        if (!flashRead(ConfigurationHelper::getFlashAddress(kHeaderOffset), header, sizeof(header))) {
            __LDBG_printf("read error offset %u", kHeaderOffset);
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
            static constexpr size_t kMaxParameterSize = 512; // max. argument length 512 byte
            uint32_t buf[kMaxParameterSize / sizeof(uint32_t)];
            while(address < endAddress) {
                auto read = std::min<size_t>(endAddress - address, sizeof(buf));
                if (!flashRead(address, buf, read)) { // using uint32_t * since buf is aligned
                    break;
                }
                crc = crc32(buf, read, crc);

                // copy parameters from data block
                auto startPtr = reinterpret_cast<uint8_t *>(buf);
                auto endPtr = startPtr + read;
                auto ptr = startPtr;
                for(; paramIdx < header.numParams() && ptr + sizeof(ParameterInfo()._header) < endPtr; paramIdx++) {
                    ParameterInfo param;

                    if __CONSTEXPR17 (sizeof(param._header) == sizeof(uint32_t)) {
                        param._header = *reinterpret_cast<uint32_t *>(ptr);
                    }
                    else {
                        memmove_P(&param._header, ptr, sizeof(param._header));
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
                __LDBG_printf("CRC mismatch 0x%08x<>0x%08x magic 0x%08x<>0x%08x", crc, header.crc(), header.magic(), CONFIG_MAGIC_DWORD);
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
    #endif
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
