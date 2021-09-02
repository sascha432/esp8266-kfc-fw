/**
* Author: sascha_lammers@gmx.de
*/

#include "JsonConfigReader.h"
#include "Configuration.hpp"

PROGMEM_STRING_DECL(config_object_name);
PROGMEM_STRING_DEF(config_object_name, "config");

bool JsonConfigReader::beginObject(bool isArray)
{
    if (_isConfigObject) {
        if (getLevel() == 3) {
            _handle = (uint16_t)stringToLl(getKey());
            _type = ParameterType::_INVALID;
            _length = 0;
            _data = String();
            //Serial.printf("key %s\n", getKey().c_str());
        }
    }
    else if (!isArray && getLevel() == 2 && !strcmp_P(getKey().c_str(), SPGM(config))) {
        _isConfigObject = true;
    }
    return true;
}

bool JsonConfigReader::endObject()
{
    if (_isConfigObject) {
        if (getLevel() == 3) {
            if (_handle != INVALID_HANDLE && _type != ParameterType::_INVALID) {
                if (_handles) {
                    bool found = false;
                    auto current = _handles;
                    while(*current != 0 && *current != INVALID_HANDLE) {
                        if (*current == _handle) {
                            found = true;
                            break;
                        }
                        current++;
                    }
                    if (!found) {
                        _type = ParameterType::_INVALID;
                    }
                }
                __LDBG_printf("JsonConfigReader::endObject(): handle %04x type %u valid %u", _handle, _type, _type != ParameterType::_INVALID);
                bool imported = true;
                switch (_type) {
                case ParameterType::BYTE: {
                        uint8_t byte = (uint8_t)stringToLl(_data);
                        _config.set<uint8_t>(_handle, byte);
                    }
                    break;
                case ParameterType::WORD: {
                        uint16_t word = (uint16_t)stringToLl(_data);
                        _config.set<uint16_t>(_handle, word);
                    }
                    break;
                case ParameterType::DWORD: {
                        uint32_t dword = (uint32_t)stringToLl(_data);
                        _config.set<uint32_t>(_handle, dword);
                    }
                    break;
                case ParameterType::QWORD: {
                        uint64_t qword = (uint64_t)stringToLl(_data);
                        _config.set<uint64_t>(_handle, qword);
                    }
                    break;
                case ParameterType::FLOAT: {
                        float number = _data.toFloat();
                        _config.set<float>(_handle, number);
                    }
                    break;
                case ParameterType::DOUBLE: {
                        double number = strtod(_data.c_str(), nullptr);
                        _config.set<double>(_handle, number);
                    }
                    break;
                case ParameterType::BINARY: {
                        if (_length * 2 == _data.length()) {
                            Buffer buffer;
                            auto ptr = _data.c_str();
                            while (_length--) {
                                uint8_t byte;
                                char buf[3] = { *ptr++, *ptr++, 0 };
                                byte = (uint8_t)strtol(buf, nullptr, 16);
                                buffer.write(byte);
                            }
                            _config.setBinary(_handle, buffer.get(), (uint16_t)buffer.length());
                        }
                    }
                    break;
                case ParameterType::STRING:
                    _config.setString(_handle, _data);
                    break;
                case ParameterType::_INVALID:
                default:
                    imported = false;
                    break;
                }
                if (imported) {
                    _imported.push_back(_handle);
                }
            }
            //Serial.printf("handle %04x done\n", _handle);
            _handle = INVALID_HANDLE;
        }
        else if (getLevel() == 2 && getKey() == FSPGM(config_object_name)) {
            _isConfigObject = false;
        }
    }
    return true;
}

bool JsonConfigReader::processElement()
{
    if (_isConfigObject) {
        auto keyStr = getKey();
        auto key = keyStr.c_str();
        //auto pathStr = getPath(false);
        //auto path = pathStr.c_str();
        //Serial.printf("key %s value %s type %s path %s index %d\n", key, getValue().c_str(), jsonType2String(getType()).c_str(), path, getObjectIndex());

        if (!strcmp_P(key, PSTR("type"))) {
            _type = (ParameterType)stringToLl(getValue());
        }
        else if (!strcmp_P(key, PSTR("length"))) {
            _length = (uint16_t)stringToLl(getValue());
        }
        else if (!strcmp_P(key, PSTR("data"))) {
            _data = getValue();
        }
    }
    return true;
}
