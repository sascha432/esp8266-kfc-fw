/**
* Author: sascha_lammers@gmx.de
*/

#include "Configuration.h"
#include <Buffer.h>

Configuration::Configuration(uint16_t offset, uint16_t size) {
    _offset = offset;
    _size = size;
}

// read map only, data is read on demand

bool Configuration::read() {
    debug_printf_P(PSTR("Configuration::read()\n"));
    return _readMap();
}


// write data to EEPROM

bool Configuration::write() {
    debug_printf_P(PSTR("Configuration::write()\n"));
    Buffer buffer;

    if (!EEPROM.begin(_offset + _size)) {
        return false;
    }

    Header_t header = { CONFIG_MAGIC_DWORD, 0, 0 };

    buffer.write((const uint8_t *)&header, sizeof(header));

    for (auto &map : _map) {
        if (!map.second.write(buffer, map.first, _offset, _size)) {
            EEPROM.end();
            return false;
        }
    }
    buffer.write((char)ConfigurationParameter::_EOF);

    if (buffer.length() > _size) {
        debug_printf_P(PSTR("Configuration::write(): Size exceeded %u > %u\n"), buffer.length(), _size);
        EEPROM.end();
        return false;
    }

    auto ptr = buffer.get();
    auto len = (uint16_t)buffer.length();
    auto position = _offset;

    // update CRC and length
    header.crc = crc16_calc(ptr + sizeof(header), len - sizeof(header));
    header.length = len;
    memcpy(ptr, &header, sizeof(header));

    while (len--) {
        EEPROM.write(position, *ptr++);
        position++;
    }

    EEPROM.commit();
    return true;
}

const char * Configuration::getString(uint16_t handle) {
    auto param = _findParam(ConfigurationParameter::STRING, handle);
    if (!param) {
        return nullptr;
    }
    return param->getString(_offset, _size);
}

void Configuration::setString(uint16_t handle, const String & string) {
    auto &param = _getParam(ConfigurationParameter::STRING, handle);
    param.setData((const uint8_t *)string.c_str(), (uint16_t)string.length());
}

void Configuration::setString(uint16_t handle, const __FlashStringHelper * string) {
    auto &param = _getParam(ConfigurationParameter::STRING, handle);
    param.setData(string, (uint16_t)strlen_P(reinterpret_cast<PGM_P>(string)));
}

void Configuration::release() {
    for (auto &map : _map) {
        map.second.release();
    }
}

bool Configuration::isDirty() const {
    for (auto map : _map) {
        if (map.second.isDirty()) {
            return true;
        }
    }
    return false;
}

ConfigurationParameter * Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, uint16_t handle) {
    for (auto &map : _map) {
        if (map.first == handle && map.second.getType() == type) {
            return &map.second;
        }
    }
    return nullptr;
}

ConfigurationParameter & Configuration::_getParam(ConfigurationParameter::TypeEnum_t type, const uint16_t handle) {
    auto param = _findParam(type, handle);
    if (!param) {
        ConfigurationParameter param(type, 0, 0);
        _map[handle] = param;
        return *_findParam(type, handle);
    }
    return *param;
}

bool Configuration::_readMap() {
    uint16_t position = _offset;

    _map.clear();
    if (!EEPROM.begin()) {
        return false;
    }

    Header_t header = { 0, 0xffff, 0xffff };
    EEPROM.get(position, header);
    position += sizeof(header);

    for (;;) {

        if (header.magic != CONFIG_MAGIC_DWORD) {
            debug_printf_P(PSTR("Configuration::_readMap(): Invalid magic %08x\n"), header.magic);
            break;
        }
        else if (header.crc == -1) {
            debug_printf_P(PSTR("Configuration::_readMap(): Invalid CRC %04x\n"), header.crc);
            break;
        }
        else if (header.length <= sizeof(header) || header.length > _size) {
            debug_printf_P(PSTR("Configuration::_readMap(): Invalid length %d\n"), header.length);
            break;
        }

        uint16_t crc = crc16_calc(EEPROM.getDataPtr() + _offset + sizeof(header), header.length - sizeof(header));
        if (header.crc != crc) {
            debug_printf_P(PSTR("Configuration::_readMap(): CRC mismatch %04x != %04x\n"), crc, header.crc);
            break;
        }

        for (;;) {

            ConfigurationParameter::TypeEnum_t type = ConfigurationParameter::_INVALID;
            EEPROM.get(position, type);
            if (type == ConfigurationParameter::_INVALID) {
                debug_printf_P(PSTR("Configuration::_readMap(): Invalid type\n"));
                break;
            }
            else if (type == ConfigurationParameter::_EOF) {
                EEPROM.end();
                return true;
            }
            position += sizeof(type);
            uint16_t handle = -1;
            EEPROM.get(position, handle);
            if (handle == -1) {
                debug_printf_P(PSTR("Configuration::_readMap(): Invalid handle\n"));
                break;
            }
            position += sizeof(handle);

            uint16_t size = ConfigurationParameter::getSize(type, position, _size);
            if (size == -1) {
                debug_printf_P(PSTR("Configuration::_readMap(): Invalid size\n"));
                break;
            }

            ConfigurationParameter param(type, size, position);
            position += size;
            _map[handle] = param;

        }
        break;

    }
    EEPROM.end();
    return false;
}
