/**
* Author: sascha_lammers@gmx.de
*/

#include "Configuration.h"
#include "JsonConfigReader.h"
#include <Buffer.h>
#include <JsonTools.h>
#include "misc.h"

#include "DumpBinary.h"

#if defined(ESP8266)
extern "C" {
    #include "spi_flash.h"
}

#if defined(ARDUINO_ESP8266_RELEASE_2_6_3)
extern "C" uint32_t _EEPROM_start;
#else
extern "C" uint32_t _SPIFFS_end;
#define _EEPROM_start _SPIFFS_end
#warning check if eeprom start is correct
#endif


#ifdef NO_GLOBAL_EEPROM
// allows to use a different sector in flash memory
#ifndef EEPROM_ADDR
// #define EEPROM_ADDR 0x40200000           // default
#define EEPROM_ADDR 0x40201000           // 1MB/4MB flash size
// #define EEPROM_ADDR 0x40202000           // 4MB
// #define EEPROM_ADDR 0x40203000           // 4MB
#endif
EEPROMClass EEPROM((((uint32_t)&_EEPROM_start - EEPROM_ADDR) / SPI_FLASH_SEC_SIZE));
#else
#define EEPROM_ADDR 0x40200000           // sector of the configuration for direct access
#endif
#endif

#if defined(ESP32)
PROGMEM_STRING_DEF(EEPROM_partition_name, "eeprom");
#endif


#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if DEBUG_GETHANDLE
// if debugging is enable, getHandle() verifies that all hashes are unique
static std::map<const ConfigurationParameter::Handle_t, String> handles;

uint16_t getHandle(const char *name) {
    ConfigurationParameter::Handle_t crc = constexpr_crc16_calc((const uint8_t *)name, constexpr_strlen(name));
    for(const auto &map: handles) {
        if (map.first == crc && !map.second.equals(name)) {
            __debugbreak_and_panic_printf_P(PSTR("getHandle(%s): CRC not unique: %x, %s\n"), name, crc, map.second.c_str());
        }
    }
    handles[crc] = String(name);
    return crc;
}

const char *getHandleName(ConfigurationParameter::Handle_t crc) {
    for(const auto &map: handles) {
        if (map.first == crc) {
            return map.second.c_str();
        }
    }
    return "<Unknown>";
}
#else
const char *getHandleName(ConfigurationParameter::Handle_t crc) {
    return "N/A";
}
#endif

Configuration::Configuration(uint16_t offset, uint16_t size) {
#if defined(ESP32)
    _partition = nullptr;
#endif
    _offset = offset;
    _size = size;
    _eepromInitialized = false;
    _readAccess = 0;
    _eepromSize = 4096;
}

Configuration::~Configuration() {
    clear();
}

void Configuration::clear() {
    _debug_printf_P(PSTR("Configuration::clear()\n"));
    for (auto &parameter : _params) {
        parameter.freeData();
    }
    _params.clear();
    _readAccess = 0;
}

// read map only, data is read on demand
bool Configuration::read() {
    clear();
    _debug_printf_P(PSTR("Configuration::read()\n"));
    auto result = _readParams();
    if (!result) {
        _debug_printf_P(PSTR("Configuration::_readParams() = false\n"));
        clear();
    }
    return result;
}

// write data to EEPROM
bool Configuration::write() {
    _debug_printf_P(PSTR("Configuration::write()\n"));

    Buffer buffer;
    uint16_t dataOffset;

    beginEEPROM();

    // store EEPROM location for unmodified parameters
    std::vector<const uint8_t *> _eepromPtr;
    _eepromPtr.reserve(_params.size());
    dataOffset = _dataOffset;
    for (auto &parameter : _params) {
        if (parameter.isDirty()) {
            _eepromPtr.push_back(nullptr);
        } else {
#if defined(ESP32)
            _eepromPtr.push_back(EEPROM.getDataPtr() + dataOffset);
#else
            _eepromPtr.push_back(EEPROM.getConstDataPtr() + dataOffset);
#endif
        }
        dataOffset += parameter.getParam().length;
    }

#if CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE && CONFIGURATION_PARAMETER_MEM_ADDR_ALIGNMENT
    // change order that data which fits into the data pointer is aligned
    for(uint16_t i = 0; i < _params.size(); i++) {
        auto &parameter = _params.at(i);
        if (!parameter.isAligned() && !parameter.needsAlloc()) { // find suitable position
            uint16_t j;
            for(j = 0; j < _params.size(); j++) {
                if (i != j) {
                    auto &parameter2 = _params.at(j);
                    if (parameter2.isAligned() && parameter2.needsAlloc()) {
                        _debug_printf_P(PSTR("Configuration::write(): Swap %d and %d\n"), i, j);
                        auto &eepromPtr = _eepromPtr.at(i);
                        auto &eepromPtr2 = _eepromPtr.at(j);
                        const uint8_t *etmp = eepromPtr;
                        eepromPtr = eepromPtr2;
                        eepromPtr2 = etmp;
                        ConfigurationParameter tmp = parameter;
                        parameter = parameter2;
                        parameter2 = tmp;
                        break;
                    }
                }
            }
            if (j == _params.size()) {  // nothing available anymore
                break;
            }
        }
    }
#endif

    // write parameter headers
    for (auto &parameter : _params) {
        auto &param = parameter.getParam();
        if (parameter.isDirty() && param.type == ConfigurationParameter::STRING) {
            parameter.updateStringLength();
        }
        buffer.write((const uint8_t *)&param, sizeof(param));
    }

    dataOffset = _offset + (uint16_t)(buffer.length() + sizeof(Header_t)); // new offset
    _debug_printf_P(PSTR("Configuration::write(): Data offset verification %d = %d\n"), dataOffset, (uint16_t)(_offset + (sizeof(ConfigurationParameter::Param_t) * _params.size()) + sizeof(Header_t)));

    // write data
    uint16_t index = 0;
    for (auto &parameter : _params) {
#if DEBUG_CONFIGURATION
        _debug_printf_P(PSTR("Configuration::write(): %04x (%s), data offset %d, size %d, dirty %d, eeprom offset %p\n"),
            parameter.getParam().handle, getHandleName(parameter.getParam().handle), (int)(buffer.length() + _offset + sizeof(Header_t)), parameter.getParam().length, parameter.isDirty(), _eepromPtr[index]);
#endif
        if (parameter.isDirty()) {
            buffer.write(parameter.getDataPtr(), parameter.getParam().length); // store new data
        } else {
            buffer.write(_eepromPtr[index], parameter.getParam().length); // copy data
        }
        index++;
    }

    if (buffer.length() > _size) {
        _debug_printf_P(PSTR("Configuration::write(): Size exceeded: %u > %u\n"), buffer.length(), _size);
        endEEPROM();
        clear();    // discard all data
        return false;
    }

    auto len = (uint16_t)buffer.length();
    auto eepromSize = _offset + len + sizeof(Header_t);
    if (eepromSize > _eepromSize) {
        _eepromSize = eepromSize; // increase max. size
        endEEPROM();
        beginEEPROM();
    } else {
        _eepromSize = eepromSize; // reduce/keep max. size
    }

    auto dptr = EEPROM.getDataPtr() + _offset;
    auto sptr = buffer.get();

    // write header
    auto headerPtr = (Header_t *)dptr;
    dptr += sizeof(Header_t);

    memset(headerPtr, 0, sizeof(Header_t));
    headerPtr->magic = CONFIG_MAGIC_DWORD;
    headerPtr->crc = crc16_calc(sptr, len);
    _debug_printf_P(PSTR("Configuration::write(): CRC %04x, length %d\n"), headerPtr->crc, len);
    headerPtr->length = len;
    headerPtr->params = _params.size();

    // copy data
    memcpy(dptr, sptr, len);

    commitEEPROM();
    _dataOffset = dataOffset;

    // dumpEEPROM(Serial, false, _offset, 160);

    for (auto &parameter : _params) {
        if (parameter.isDirty()) {
            parameter.setDirty(false);
        }
    }

    return true;
}

ConfigurationParameter & Configuration::getParameterByPosition(uint16_t position) {
    return _params.at(position);
}

uint16_t Configuration::getPosition(const ConfigurationParameter *parameter) const {
    if (parameter) {
        uint16_t pos = 0;
        for (auto &param : _params) {
            if (param.getConstParam().handle == parameter->getConstParam().handle) {
                return pos;
            }
            pos++;
        }
        _debug_printf_P(PSTR("Configuration::getPosition(): could not find %04x (%s)\n"), parameter->getConstParam().handle, getHandleName(parameter->getConstParam().handle));
        __debugbreak_and_panic();
    }
    return 0xffff;
}

const char *Configuration::getString(Handle_t handle) {
    uint16_t offset;
    auto param = _findParam(ConfigurationParameter::STRING, handle, offset);
    if (param == _params.end()) {
        return _sharedEmptyString.c_str();
    }
    return param->getString(this, offset);
}

char * Configuration::getWriteableString(Handle_t handle, uint16_t maxLength) {
    auto &param = getWritableParameter<char *>(handle, maxLength);
    param.setDirty(true);
    return reinterpret_cast<char *>(param.getDataPtr());
}

const void *Configuration::getBinary(Handle_t handle, uint16_t &length) {
    uint16_t offset;
    auto param = _findParam(ConfigurationParameter::BINARY, handle, offset);
    if (param == _params.end()) {
        return nullptr;
    }
    return param->getBinary(this, length, offset);
}

void Configuration::setString(Handle_t handle, const char *string) {
    uint16_t offset;
    auto &param = _getOrCreateParam(ConfigurationParameter::STRING, handle, offset);
    param.setData((const uint8_t *)string, (uint16_t)strlen(string), true);
}

void Configuration::setString(Handle_t handle, const String &string) {
    uint16_t offset;
    auto &param = _getOrCreateParam(ConfigurationParameter::STRING, handle, offset);
    param.setData((const uint8_t *)string.c_str(), (uint16_t)string.length(), true);
}

void Configuration::setString(Handle_t handle, const __FlashStringHelper *string) {
    uint16_t offset;
    auto &param = _getOrCreateParam(ConfigurationParameter::STRING, handle, offset);
    param.setData(string, (uint16_t)strlen_P(reinterpret_cast<PGM_P>(string)), true);
}

void Configuration::setBinary(Handle_t handle, const void *data, uint16_t length) {
    uint16_t offset;
    auto &param = _getOrCreateParam(ConfigurationParameter::BINARY, handle, offset);
    param.setData((const uint8_t *)data, length);
}

void Configuration::dumpEEPROM(Print & output, bool asByteArray, uint16_t offset, uint16_t length) {
    if (length == 0) {
        length = _eepromSize;
    }
    endEEPROM();
    EEPROM.begin(offset + length);
    output.printf_P(PSTR("Dumping EEPROM %d:%d\n"), offset, length);
    if (asByteArray) {
        uint16_t pos = offset;
        while (pos < _eepromSize) {
            output.printf_P(PSTR("0x%02x, "), EEPROM.read(pos));
            if (++pos % (76 / 6) == 0) {
                output.println();
            }
            delay(1);
        }
        output.println();
        output.printf_P(PSTR("%d,\n"), _eepromSize);
    } else {
        DumpBinary dumper(output);
#if defined(ESP32)
        dumper.dump(EEPROM.getDataPtr() + offset, length);
#else
        dumper.dump(EEPROM.getConstDataPtr() + offset, length);
#if 1
        output.printf_P(PSTR("Dumping flash (spi_read) %d:%d\n"), offset, length);
        endEEPROM();
        uint8_t *buffer = (uint8_t *)malloc((length + 4) & ~3);
        getEEPROM(buffer, offset, length, (length + 4) & ~3);
        dumper.dump(buffer, length);
        free(buffer);
#endif
#endif
    }
    EEPROM.end();
}

void Configuration::dump(Print &output) {

    output.println(F("Configuration:"));
    output.printf_P(PSTR("Data offset %d, base offset %d, eeprom size %d, parameters %d, length %d\n"), _dataOffset, _offset, _eepromSize, _params.size(), _eepromSize - _offset);
    output.printf_P(PSTR("Min. memory usage %d, Size of header %d, Param_t %d, ConfigurationParameter %d, Configuration %d\n"), sizeof(Configuration) + _params.size() * sizeof(ConfigurationParameter), sizeof(Configuration::Header_t), sizeof(ConfigurationParameter::Param_t), sizeof(ConfigurationParameter), sizeof(Configuration));

    uint16_t offset = _dataOffset;
    for (auto &parameter : _params) {
        auto &param = parameter.getParam();
        output.printf_P(PSTR("%04x (%s): "), param.handle, getHandleName(param.handle));
        auto length = parameter.read(this, offset);
        output.printf_P(PSTR("type %s, data offset %d, size %d, value: "), parameter.getTypeString(parameter.getType()).c_str(), offset, /*calculateOffset(param.handle),*/ length);
        parameter.dump(output);
        offset += param.length;
    }
}

void Configuration::release() {
    _debug_printf_P(PSTR("void Configuration::release(): Last read %d, dirty %d\n"), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
    for (auto &parameter : _params) {
        parameter.release();
    }
    endEEPROM();
    _readAccess = 0;
}

bool Configuration::isDirty() const {
    for(const auto &param: _params) {
        if (param.isDirty()) {
            return true;
        }
    }
    return false;
}

void Configuration::exportAsJson(Print& output, const String &version)
{
    output.printf_P(PSTR("{\n\t\"magic\": \"%#08x\",\n\t\"version\": \"%s\",\n"), CONFIG_MAGIC_DWORD, version.c_str());
    output.print(F("\t\"config\": {\n"));

    uint16_t offset = _dataOffset;
    for (auto& parameter : _params) {
        auto& param = parameter.getParam();

        if (offset != _dataOffset) {
            output.print(F(",\n"));
        }

        output.printf_P(PSTR("\t\t\"%#04x\": {\n"), param.handle);
#if DEBUG_GETHANDLE
        output.print(F("\t\t\t\"name\": \""));
        auto name = getHandleName(param.handle);
        JsonTools::printToEscaped(output, name, strlen(name), false);
        output.print(F("\",\n"));
#endif

        auto length = parameter.read(this, offset);
        output.printf_P(PSTR("\t\t\t\"type\": %d,\n"), parameter.getType());
        output.printf_P(PSTR("\t\t\t\"type_name\": \"%s\",\n"), parameter.getTypeString(parameter.getType()).c_str());
        output.printf_P(PSTR("\t\t\t\"length\": %d,\n"), length);
        output.print(F("\t\t\t\"data\": "));
        parameter.exportAsJson(output);
        output.print(F("\n"));
        offset += param.length;

        output.print(F("\t\t}"));
    }

    output.print(F("\n\t}\n}\n"));
}

bool Configuration::importJson(Stream& stream, uint16_t *handles)
{
    JsonConfigReader reader(&stream, *this, handles);
    reader.initParser();
    return reader.parseStream();
}

void Configuration::endEEPROM() {
    if (_eepromInitialized) {
        _debug_printf_P(PSTR("Configuration::endEEPROM()\n"));
        EEPROM.end();
        _eepromInitialized = false;
    }
}

void Configuration::commitEEPROM() {
    if (_eepromInitialized) {
        _debug_printf_P(PSTR("Configuration::commitEEPROM()\n"));
        EEPROM.commit();
        _eepromInitialized = false;
    }
    else {
        _debug_printf_P(PSTR("Configuration::commitEEPROM(): EEPROM is not initialized\n"));
    }
}

void Configuration::getEEPROM(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size) {

#if defined(ESP8266)
    // if the EEPROM is not intialized, copy data from flash directly
    if (_eepromInitialized) {
        memcpy(dst, EEPROM.getConstDataPtr() + offset, length); // data is already in RAM
        return;
    }
    _debug_printf_P(PSTR("Configuration::getEEPROM(%p, %d, %d, %d)\n"), dst, offset, length, size);

    auto eeprom_start_address = ((uint32_t)&_EEPROM_start - EEPROM_ADDR) + offset;

    uint8_t alignment = eeprom_start_address % 0x4;
    if (alignment) {
        eeprom_start_address -= alignment; // align start
    }
    uint16_t readSize = length + alignment; // add offset to length
    if (readSize & 0x3) {
        readSize = (readSize + 0x4) & ~0x3; // align read length
    }

    // _debug_printf_P(PSTR("Configuration::getEEPROM(offset %d, length %d): alignment %d\n"), offset, length, alignment);


    // _debug_printf_P(PSTR("Configuration::getEEPROM(): flash: %08X:%08X (%d) aligned: %08X:%08X (%d)\n"),
    //     eeprom_start_address + alignment, eeprom_start_address + length + alignment, length,
    //     eeprom_start_address, eeprom_start_address + readSize, readSize
    // );

    SpiFlashOpResult result;
    if (readSize > size) { // does not fit into the buffer
        uint8_t buf[64];
        uint8_t *ptr = buf;
        if (readSize > 64) {
            _debug_printf_P(PSTR("Configuration::getEEPROM(): allocating read buffer %d (%d, %d)\n"), readSize, length, size); // large read operation should have an aligned address already to avoid this
            ptr = (uint8_t *)malloc(readSize);
        }
        noInterrupts();
        result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t *>(ptr), readSize);
        interrupts();
        if (result == SPI_FLASH_RESULT_OK) {
            memcpy(dst, ptr + alignment, length); // add alignment offset
        }
        if (buf != ptr) {
            ::free(ptr);
        }
    } else {
        noInterrupts();
        result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t*>(dst), readSize);
        interrupts();
    }
    if (result != SPI_FLASH_RESULT_OK) {
        memset(dst, 0, length);
    }
    _debug_printf_P(PSTR("Configuration::getEEPROM(): spi_flash_read(%08x, %d) = %d, offset %u\n"), eeprom_start_address, readSize, result, offset);

#elif defined(ESP32)

    if (_eepromInitialized) {
        if (!EEPROM.readBytes(offset, dst, length)) {
            memset(dst, 0, length);
        }
    }
    else {
        if (!_partition) {
            _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, SPGM(EEPROM_partition_name));
        }
        _debug_printf_P(PSTR("Configuration::getEEPROM(offset %d, length %d): using esp_partition_read(%p)\n"), offset, length, _partition);
        if (esp_partition_read(_partition, offset, (void *)dst, length) != ESP_OK) {
            memset(dst, 0, length);
        }
    }

#else

    beginEEPROM();
    memcpy(dst, EEPROM.getConstDataPtr() + offset, length);

#endif
}

Configuration::ParameterVectorIterator Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset) {
    offset = _dataOffset;
    if (type == ConfigurationParameter::_ANY) {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (it->getHandle() == handle) {
                return it;
            }
            offset += it->getParam().length;
        }
    }
    else {
        for (auto it = _params.begin(); it != _params.end(); ++it) {
            if (it->getHandle() == handle && it->getParam().type == type) {
                return it;
            }
            offset += it->getParam().length;
        }
    }
    //_debug_printf_P(PSTR("Configuration::_findParam(%d, %04x (%s)) = not found\n"), (int)type, handle, getHandleName(handle));
    return _params.end();
}

ConfigurationParameter &Configuration::_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, Handle_t handle, uint16_t &offset) {
    auto iterator = _findParam(ConfigurationParameter::_ANY, handle, offset);
    if (iterator == _params.end()) {
        ConfigurationParameter::Param_t param;
        param.handle = handle;
        param.type = type;
        param.length = 0;
        ConfigurationParameter parameter(param);
        parameter.setSize(ConfigurationParameter::getDefaultSize(type));
        _params.push_back(parameter);
        return _params.back();
    }
    else if (type != iterator->getType()) {
        _debug_printf_P(PSTR("Configuration::_readParams(): reading %04x (%s), cannot override type %d with %d\n"), iterator->getParam().handle, getHandleName(iterator->getParam().handle), iterator->getType(), type);
        __debugbreak_and_panic();
        //iterator->freeData();
        //iterator->setSize(ConfigurationParameter::getDefaultSize(type));
        //auto &param = iterator->getParam();
        //param.type = type;
        //param.length = 0;
    }
    return *iterator;
}

bool Configuration::_readParams() {
    uint16_t offset = _offset;

    union {
        uint8_t headerBuffer[(sizeof(Header_t) + 8) & ~0x07]; // add extra space for getEEPROM in case the read offset is not aligned
        Header_t header;
    } hdr;
    memset(&hdr, 0, sizeof(hdr));

    endEEPROM();
#if defined(ESP8266) || defined(ESP32)
    // read header directly from flash since we do not know the size of the configuration
    getEEPROM(hdr.headerBuffer, (uint16_t)offset, (uint16_t)sizeof(hdr.header), (uint16_t)sizeof(hdr.headerBuffer));
#if DEBUG_CONFIGURATION
    _debug_println(F("Header:"));
    DumpBinary dump(DEBUG_OUTPUT);
    dump.dump((const uint8_t *)&hdr, sizeof(hdr.header));
#endif
#else
    _eepromSize = _offset + sizeof(hdr.header);
    beginEEPROM();
    EEPROM.get(offset, hdr.header);
    endEEPROM();
#endif
    offset += sizeof(hdr.header);

    for (;;) {

        if (hdr.header.magic != CONFIG_MAGIC_DWORD) {
#if DEBUG_CONFIGURATION
            _debug_printf_P(PSTR("Configuration::_readParams(): Invalid magic %08x\n"), hdr.header.magic);
            dumpEEPROM(Serial, false, _offset, 160);
#endif
            break;
        }
        else if (hdr.header.crc == -1) {
            _debug_printf_P(PSTR("Configuration::_readParams(): Invalid CRC %04x\n"), hdr.header.crc);
            break;
        }
        else if (hdr.header.length <= 0 || hdr.header.length + sizeof(hdr.header) > _size) {
            _debug_printf_P(PSTR("Configuration::_readParams(): Invalid length %d\n"), hdr.header.length);
            break;
        }

        // now we know the required size, initialize EEPROM
        _eepromSize = _offset + sizeof(hdr.header) + hdr.header.length;
        beginEEPROM();

#if defined(ESP32)
        uint16_t crc = crc16_calc(EEPROM.getDataPtr() + _offset + sizeof(hdr.header), hdr.header.length);
#else
        uint16_t crc = crc16_calc(EEPROM.getConstDataPtr() + _offset + sizeof(hdr.header), hdr.header.length);
#endif
        if (hdr.header.crc != crc) {
            _debug_printf_P(PSTR("Configuration::_readParams(): CRC mismatch %04x != %04x\n"), crc, hdr.header.crc);
            break;
        }

        _params.reserve(hdr.header.params);
        _dataOffset = sizeof(ConfigurationParameter::Param_t) * hdr.header.params + offset;
        auto dataOffset = _dataOffset;

        for(;;) {

            ConfigurationParameter::Param_t param;
            param.type = ConfigurationParameter::_INVALID;
            EEPROM.get(offset, param);
            if (param.type == ConfigurationParameter::_INVALID) {
                _debug_printf_P(PSTR("Configuration::_readParams(): Invalid type\n"));
                break;
            }
            offset += sizeof(param);

            ConfigurationParameter parameter(param);
            _params.push_back(parameter);
#if CONFIGURATION_PARAMETER_USE_DATA_PTR_AS_STORAGE
            if (parameter.isAligned() && !parameter.needsAlloc()) {
                _debug_printf_P(PSTR("Configuration::_readParams(): reading %04x (%s), size %d\n"), parameter.getParam().handle, getHandleName(parameter.getParam().handle), parameter.getParam().length);
                _params.back().read(this, dataOffset);
            }
#endif
            dataOffset += param.length;

            if (_params.size() == hdr.header.params) {
                endEEPROM();
                return true;
            }
        }
        _debug_printf_P(PSTR("Configuration::_readParams(): failure before reading all parameters %d/%d\n"), _params.size(), hdr.header.params);
        break;

    }

    clear();
    endEEPROM();
    return false;
}

#if DEBUG_CONFIGURATION
uint16_t Configuration::calculateOffset(Handle_t handle) {
    uint16_t offset = _dataOffset;
    for (auto &parameter: _params) {
        if (parameter.getHandle() == handle) {
            return offset;
        }
        offset += parameter.getParam().length;
    }
    return 0;
}
#endif
