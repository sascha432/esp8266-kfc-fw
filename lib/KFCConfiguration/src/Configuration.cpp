/**
* Author: sascha_lammers@gmx.de
*/

#include "Configuration.h"
#if !_WIN32
#include <EEPROM.h>
#endif
#include <Buffer.h>

#include "DumpBinary.h"

#if ESP8266
extern "C" {
    #include "spi_flash.h"
}
extern "C" uint32_t _SPIFFS_end;
#endif

#ifdef NO_GLOBAL_EEPROM
#ifndef EEPROM_ADDR
//#define EEPROM_ADDR 0x40200000
 #define EEPROM_ADDR 0x40201000       // 1MB/4MB flash size
// #define EEPROM_ADDR 0x40202000       // 4MB
// #define EEPROM_ADDR 0x40203000       // 4MB
#endif
EEPROMClass EEPROM((((uint32_t)&_SPIFFS_end - EEPROM_ADDR) / SPI_FLASH_SEC_SIZE));
#else
#define EEPROM_ADDR 0x40200000
#endif


unsigned long Configuration::_readAccess = 0;
bool Configuration::_eepromInitialized = false;
uint16_t Configuration::_eepromSize = 4096;

#if DEBUG_GETHANDLE
// if debugging is enable, getHandle() verifies that all hashes are unique
static std::map<const uint16_t, String> handles;

uint16_t getHandle(const char *name) {
    uint16_t crc = constexpr_crc16_calc((const uint8_t *)name, constexpr_strlen(name));
    for(const auto &map: handles) {
        if (map.first == crc && !map.second.equals(name)) {
            debug_printf_P(PSTR("getHandle(%s): CRC not unique: %x, %s\n"), name, crc, map.second.c_str());
            panic();
        }
    }
    handles[crc] = String(name);
    return crc;
}

const char *getHandleName(uint16_t crc) {
    for(const auto &map: handles) {
        if (map.first == crc) {
            return map.second.c_str();
        }
    }
    return "<Unknown>";
}
#else
const char *getHandleName(uint16_t crc) {
    return "N/A";
}
#endif

Configuration::Configuration(uint16_t offset, uint16_t size) {
    _offset = offset;
    _size = size;
}

Configuration::~Configuration() {
    clear();
}

void Configuration::clear() {
    debug_printf_P(PSTR("Configuration::clear()\n"));
    for (auto &parameter : _params) {
        parameter.free();
    }
    _params.clear();
    _readAccess = 0;
}

// read map only, data is read on demand
bool Configuration::read() {
    debug_printf_P(PSTR("Configuration::read()\n"));
    bool result;
    clear();
    if (false == (result = _readParams())) {
        clear();
    }
    return result;
}


// write data to EEPROM
bool Configuration::write() {
    debug_printf_P(PSTR("Configuration::write()\n"));
    Buffer buffer;

    beginEEPROM();
    uint16_t dataOffset = _offset;

    for (auto &it : _params) {
        auto &param = it.getParam();
        if (it.isDirty() && param.type == ConfigurationParameter::STRING) {
            it.updateStringLength();
        }
        buffer.write((const uint8_t *)&param, sizeof(param));
    }

    dataOffset += sizeof(ConfigurationParameter::Param_t) * _params.size() + sizeof(Header_t);
    debug_printf_P(PSTR("Data offset verification %d = %d\n"), _offset + (uint16_t)(buffer.length() + sizeof(Header_t)), dataOffset);

    for (auto &it : _params) {
        auto &param = it.getParam();
#if DEBUG
        auto tmp = dataOffset;
        _dataOffset = dataOffset;
        debug_printf_P(PSTR("Configuration::write(): %04x (%s), data offset %d (%d), size %d\n"), param.handle, getHandleName(param.handle), (int)(buffer.length() + _offset + sizeof(Header_t)), calculateOffset(param.handle), param.length);
        _dataOffset = tmp;
#endif
        if (!it.write(buffer)) {
            endEEPROM();
            return false;
        }
    }

    if (buffer.length() > _size) {
        debug_printf_P(PSTR("Configuration::write(): Size exceeded: %u > %u\n"), buffer.length(), _size);
        endEEPROM();
        return false;
    }

    auto len = (uint16_t)buffer.length();
    auto size = len + _offset + sizeof(Header_t);
    if (size > _eepromSize) {
        _eepromSize = size; // does not fit, resize
        endEEPROM();
        beginEEPROM();
    } else {
        _eepromSize = size; // update size to save memory
    }

    auto dptr = EEPROM.getDataPtr() + _offset;
    auto sptr = buffer.get();

    // write header
    auto headerPtr = (Header_t *)dptr;
    dptr += sizeof(Header_t);

    memset(headerPtr, 0, sizeof(Header_t));
    headerPtr->magic = CONFIG_MAGIC_DWORD;
    headerPtr->crc = crc16_calc(sptr, len);
    debug_printf_P(PSTR("Configuration::write(): CRC %04x, length %d\n"), headerPtr->crc, len);
    headerPtr->length = len;
    headerPtr->params = _params.size();

    // copy data
    memcpy(dptr, sptr, len);

    commitEEPROM();
    _dataOffset = dataOffset;

    for (auto &parameter : _params) {
        parameter.setDirty(false);
    }

    return true;
}

const char *Configuration::getString(uint16_t handle) {
    auto param = _findParam(ConfigurationParameter::STRING, handle);
    if (!param) {
        return _sharedEmptyString.c_str();
    }
    return param->getString(calculateOffset(handle));
}

char * Configuration::getWriteableString(uint16_t handle, uint16_t maxLength) {
    auto &param = _getParam(ConfigurationParameter::STRING, handle);
    if (param.isWriteable(maxLength)) {
        return (char *)param.getDataPtr();
    }
    char *data = (char *)calloc(maxLength + 1, 1);
    char *ptr = (char *)param.getString(_offset);
    if (ptr) {
        strcpy(data, ptr);
    }
    param.setDataPtr((uint8_t *)data, maxLength);
    return data;
}

const void *Configuration::getBinary(uint16_t handle, uint16_t &length) {
    auto param = _findParam(ConfigurationParameter::BINARY, handle);
    if (!param) {
        return nullptr;
    }
    return param->getBinary(length, calculateOffset(handle));
}

void Configuration::setString(uint16_t handle, const char *string) {
    auto &param = _getParam(ConfigurationParameter::STRING, handle);
    param.setData((const uint8_t *)string, (uint16_t)strlen(string), true);
}

void Configuration::setString(uint16_t handle, const String &string) {
    auto &param = _getParam(ConfigurationParameter::STRING, handle);
    param.setData((const uint8_t *)string.c_str(), (uint16_t)string.length(), true);
}

void Configuration::setString(uint16_t handle, const __FlashStringHelper *string) {
    auto &param = _getParam(ConfigurationParameter::STRING, handle);
    param.setData(string, (uint16_t)strlen_P(reinterpret_cast<PGM_P>(string)), true);
}

void Configuration::setBinary(uint16_t handle, const void *data, uint16_t length) {
    auto &param = _getParam(ConfigurationParameter::BINARY, handle);
    param.setData((const uint8_t *)data, length);
}

void Configuration::dumpEEPROM(Print & output)
{
    beginEEPROM();
    uint16_t pos = 0;
    while (pos < _eepromSize) {
        output.printf_P(PSTR("0x%02x, "), EEPROM[pos]);
        if (++pos % (76 / 6) == 0) {
            output.println();
        }
    }
    output.println();
    output.printf_P(PSTR("%d,\n"), _eepromSize);
    endEEPROM();
}

void Configuration::dump(Print &output) {

    output.println(F("Configuration:"));
    output.printf_P(PSTR("Data offset %d, base offset %d, eeprom size %d, parameters %d, length %d\n"), _dataOffset, _offset, _eepromSize, _params.size(), _eepromSize - _offset);
    output.printf_P(PSTR("Min. memory usage %d, Size of header %d, Param_t %d, ConfigurationParameter %d, Configuration %d\n"), sizeof(Configuration) + _params.size() * sizeof(ConfigurationParameter), sizeof(Configuration::Header_t), sizeof(ConfigurationParameter::Param_t), sizeof(ConfigurationParameter), sizeof(Configuration));

    uint16_t offset = _dataOffset;
    for (auto &parameter : _params) {
        auto &param = parameter.getParam();
        output.printf_P(PSTR("%04x (%s): "), param.handle, getHandleName(param.handle));
        auto length = parameter.read(offset);
        output.printf_P(PSTR("type %s, data offset %d (%d), size %d, value: "), parameter.getTypeString(parameter.getType()).c_str(), offset, calculateOffset(param.handle), length);
        parameter.dump(output);
        offset += param.length;
    }
}

void Configuration::release() {
    debug_printf_P(PSTR("void Configuration::release(): Last read %d, dirty %d\n"), (int)(_readAccess == 0 ? -1 : millis() - _readAccess), isDirty());
    // if (isDirty()) {
        for (auto &parameter : _params) {
            parameter.release();
        }
        endEEPROM();
    // }
    // else {
    //     _map.clear();
    //     endEEPROM();
    // }
}

bool Configuration::isDirty() const {
    for (auto it = _params.begin(); it != _params.end(); ++it) {
        if (it->isDirty()) {
            return true;
        }
    }
    return false;
}

void Configuration::beginEEPROM() {
    if (!_eepromInitialized) {
        _eepromInitialized = true;
        debug_printf_P(PSTR("Configuration::beginEEPROM() = %d\n"), _eepromSize);
        EEPROM.begin(_eepromSize);
    }
}

void Configuration::endEEPROM() {
    if (_eepromInitialized) {
        debug_printf_P(PSTR("Configuration::end()\n"));
        EEPROM.end();
        _eepromInitialized = false;
    }
}

void Configuration::commitEEPROM() {
    if (_eepromInitialized) {
        debug_printf_P(PSTR("Configuration::commit()\n"));
        EEPROM.commit();
        _eepromInitialized = false;
    }
    else {
        debug_printf_P(PSTR("Configuration::commit(): EEPROM is not initialized\n"));
    }
}

bool Configuration::isEEPROMInitialized() {
    return _eepromInitialized;
}

uint16_t Configuration::getEEPROMSize() {
    return _eepromSize;
}

#if ESP8266
void Configuration::copyEEPROM(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size) {
    debug_printf_P(PSTR("Configuration::copyEEPROM(%p, %d, %d, %d)\n"), dst, offset, length, size);

    auto eeprom_start_address = ((uint32_t)&_SPIFFS_end - EEPROM_ADDR);
#if DEBUG
    auto eeprom_end_address = eeprom_start_address + length;
#endif
    uint8_t alignment;

    alignment = eeprom_start_address % 4;
    if (alignment != 0) {
        eeprom_start_address -= alignment;
    }
    uint16_t requiredSize = length + alignment;
    if (requiredSize % 4 != 0) {
        requiredSize += 4 - (requiredSize % 4);
    }

    if (requiredSize > size) {
        #define STATIC_BUFFER_SIZE 256
        if (requiredSize > STATIC_BUFFER_SIZE) {
            uint32_t *tmp = (uint32_t *)malloc(requiredSize);
            debug_printf_P(PSTR("Configuration::copyEEPROM(): allocating temporary buffer %p, dword aligned: %d:%d => %d:%d\n"), tmp, eeprom_start_address + alignment, eeprom_end_address, eeprom_start_address, eeprom_start_address + requiredSize);
            cli();
            IF_DEBUG(auto result =) spi_flash_read(eeprom_start_address, tmp, requiredSize);
            sei();
            debug_printf_P(PSTR("Configuration::copyEEPROM(): spi_flash_read(%d, %d) = %d\n"), eeprom_start_address,result, requiredSize);
            memcpy(dst, tmp + alignment, length);
            ::free(tmp);
        } else {
            uint32_t tmp[STATIC_BUFFER_SIZE / 4]; //TODO check if this is using the stack only inside the else block
            debug_printf_P(PSTR("Configuration::copyEEPROM(): using buffer on stack, dword aligned: %d:%d => %d:%d\n"), eeprom_start_address + alignment, eeprom_end_address, eeprom_start_address, eeprom_start_address + requiredSize);
            cli();
            IF_DEBUG(auto result =) spi_flash_read(eeprom_start_address, tmp, requiredSize);
            sei();
            debug_printf_P(PSTR("Configuration::copyEEPROM(): spi_flash_read(%d, %d) = %d\n"), eeprom_start_address, result, requiredSize);
            memcpy(dst, tmp + alignment, length);
        }
    } else { // copy directly, the data is dword aligned

        cli();
        IF_DEBUG(auto result =) spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t*>(dst), requiredSize);
        sei();
        debug_printf_P(PSTR("Configuration::copyEEPROM(): spi_flash_read(%d, %d) = %d\n"), eeprom_start_address, result);
    }

    beginEEPROM();
    char *tmp2 = (char *)malloc(length);
    char *ptr = tmp2;
    auto len = length;
    auto pos = offset;
    while(len--) {
        *ptr++ = EEPROM.read(pos++);
    }
    debug_printf_P(PSTR("memcpy EEPROM vs flash_read: %d\n"), memcmp(dst, tmp2, length));
    DumpBinary x(Serial);
    x.setPerLine(32);
    x.dump(dst,length);
    x.dump((uint8_t*)tmp2,length);
    memcpy(dst,tmp2,length);
    free(tmp2);

}
#endif

unsigned long Configuration::getLastReadAccess() {
    return _readAccess;
}

ConfigurationParameter *Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, uint16_t handle) {
    for (auto &parameter : _params) {
        auto &param = parameter.getParam();
        if (param.handle == handle && param.type == type) {
            debug_printf_P(PSTR("Configuration::_findParam(%d, %04x (%s)) = %p, data offset %d\n"), (int)type, handle, getHandleName(handle), (void *)&parameter, calculateOffset(handle));
            return &parameter;
        }
    }
    debug_printf_P(PSTR("Configuration::_findParam(%d, %04x (%s)) = nullptr\n"), (int)type, handle, getHandleName(handle));
    return nullptr;
}

ConfigurationParameter &Configuration::_getParam(ConfigurationParameter::TypeEnum_t type, uint16_t handle) {
    auto parameterPtr = _findParam(type, handle);
    if (!parameterPtr) {
        ConfigurationParameter::Param_t param;
        param.handle = handle;
        param.type = type;
        param.length = ConfigurationParameter::getDefaultSize(type);
        ConfigurationParameter parameter(param);
        _params.push_back(parameter);
        parameterPtr = &_params.back();
    }
    return *parameterPtr;
}

bool Configuration::_readParams() {
    uint16_t offset = _offset;

    beginEEPROM();

    Header_t header;
    memset(&header, 0, sizeof(header));
    EEPROM.get(offset, header);
    offset += sizeof(header);

    for (;;) {

        if (header.magic != CONFIG_MAGIC_DWORD) {
            debug_printf_P(PSTR("Configuration::_readParams(): Invalid magic %08x\n"), header.magic);
            break;
        }
        else if (header.crc == -1) {
            debug_printf_P(PSTR("Configuration::_readParams(): Invalid CRC %04x\n"), header.crc);
            break;
        }
        else if (header.length <= 0 || header.length + sizeof(header) > _size) {
            debug_printf_P(PSTR("Configuration::_readParams(): Invalid length %d\n"), header.length);
            break;
        }

        uint16_t crc = crc16_calc(EEPROM.getConstDataPtr() + _offset + sizeof(header), header.length);
        if (header.crc != crc) {
            debug_printf_P(PSTR("Configuration::_readParams(): CRC mismatch %04x != %04x\n"), crc, header.crc);
            break;
        }

        _params.reserve(header.params);
        _dataOffset = sizeof(ConfigurationParameter::Param_t) * header.params + offset;
        auto dataOffset = _dataOffset;

        for(;;) {

            ConfigurationParameter::Param_t param;
            param.type = ConfigurationParameter::_INVALID;
            EEPROM.get(offset, param);
            if (param.type == ConfigurationParameter::_INVALID) {
                debug_printf_P(PSTR("Configuration::_readParams(): Invalid type\n"));
                break;
            }
            offset += sizeof(param);

            ConfigurationParameter parameter(param);

            if (!parameter.needsAlloc()) {
                parameter.read(dataOffset);
            }
            dataOffset += param.length;

            _params.push_back(parameter);
            if (_params.size() == header.params) {
                endEEPROM();
                return true;
            }
        }
        break;

    }

    endEEPROM();
    return false;
}

uint16_t Configuration::calculateOffset(uint16_t handle) {
    uint16_t offset = _dataOffset;
    for (auto &parameter: _params) {
        if (parameter.getParam().handle == handle) {
            return offset;
        }
        offset += parameter.getParam().length;
    }
    return 0;
}
