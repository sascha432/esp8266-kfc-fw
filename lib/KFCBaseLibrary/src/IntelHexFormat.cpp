/**
* Author: sascha_lammers@gmx.de
*/

#include "IntelHexFormat.h"

const char _error_0[] PROGMEM = { "" };
const char _error_1[] PROGMEM = { "Failed to open" };
const char _error_3[] PROGMEM = { "Read error" };
const char _error_4[] PROGMEM = { "Invalid checksum" };
const char _error_5[] PROGMEM = { "Record type not supported" };
const char _error_6[] PROGMEM = { "Invalid characters" };
const char _error_7[] PROGMEM = { "EOF before end" };
PGM_P _errors[] PROGMEM = { _error_0, _error_1, _error_3, _error_4, _error_5, _error_6, _error_7  };

IntelHexFormat::IntelHexFormat() {
}

bool IntelHexFormat::open(const String &filename)
{
    _error = NONE;
    _endAddress = 0;
    _file = SPIFFS.open(filename, "r");
    if (!_file) {
        _error = FAILED_TO_OPEN;
        return false;
    }
    return true;
}

void IntelHexFormat::close()
{
    _file.close();
    _record.clear();
}

bool IntelHexFormat::validate()
{
    _endAddress = 0;
    reset();
    while (!_eof) {
        if (!_readRecord(false)) {
            return false;
        }
    }
    reset();
    return true;
}

int IntelHexFormat::readBytes(char *buffer, size_t length, uint16_t &address)
{
    if (_error != NONE) {
        return _error;
    }
    if (!_file) {
        return READ_ERROR;
    }
    else {
        do {
            if (!_record.isEmpty()) {
                address = _record.getAddress();
                auto read = _record.length();
                if (read > length) {
                    read = (uint8_t)length;
                }
                memcpy(buffer, _record.getDataPtr(), read);
                if (read == _record.length()) {
                    _record.clear();
                }
                else {
                    _record.getData().remove(0, read);
                }
                _record.incrAddress(read);
                return read;
            }
            else if (isEOF()) {
                return 0;
            }
        }
        while (_readRecord(true));
    }
    if (isEOF()) {
        return 0;
    }
    return -1;
}

PGM_P IntelHexFormat::getErrorMessage() const
{
    return _errors[_error];
}

IntelHexFormat::ErrorEnum_t IntelHexFormat::getError() const
{
    return _error;
}

bool IntelHexFormat::isEOF() const
{
    return _eof;
}

uint16_t IntelHexFormat::getEndAddress() const
{
    return _endAddress;
}

uint16_t IntelHexFormat::getCurAddress() const
{
    return _record.getAddress();
}

void IntelHexFormat::reset()
{
    _eof = false;
    _error = NONE;
    _record.clear();
    _file.seek(0, SeekSet);
}

int IntelHexFormat::_readHexByte()
{
    auto highByte = _file.read();
    auto lowByte = _file.read();
    if (!isxdigit(highByte) || !isxdigit(lowByte)) {
        return -1;
    }
    char str[3] = { (char)highByte, (char)lowByte, 0 };
    return (uint16_t)strtol(str, nullptr, 16);
}

bool IntelHexFormat::_readRecord(bool store)
{
    while (_file.available()) {
        auto byte = _file.read();
        if (byte == -1) {
            _error = READ_ERROR;
            return false;
        }
        else if (byte == ':') { // record start indicator
            uint8_t checksum = 0;
            uint8_t bytes[4];   // count, address hi, low, record type
            for(uint8_t i = 0; i < 4; i++) {
                if ((byte = _readHexByte()) == -1) {
                    _error = READ_ERROR;
                    return false;
                }
                bytes[i] = (uint8_t)byte;
                checksum += (uint8_t)byte;
            }
            uint8_t data[255];
            auto ptr = data;
            auto count = bytes[0];
            while (count--) {
                if ((byte = _readHexByte()) == -1) {
                    _error = READ_ERROR;
                    return false;
                }
                *ptr++ = (uint8_t)byte;
                checksum += (uint8_t)byte;
            }
            if ((byte = _readHexByte()) == -1) { // check sum
                _error = READ_ERROR;
                return false;
            }
            checksum = ~checksum + 1;
            if (byte != checksum) { // checksum error
                _error = INVALID_CHECKSUM;
                return false;
            }
            switch(bytes[3]) {
            case RT_EOF:
                _eof = true;
                return true;
            case RT_DATA:
                break;
            default:
                _error = RECORD_TYPE_NOT_SUPPORTED;
                return false;
            }

            uint16_t address = (bytes[1] << 8) | bytes[2];
            if (store) {
                _record.setData(address, data, bytes[0]);
            }
            else {
                _endAddress = std::max(_endAddress, (uint16_t)(address + bytes[0]));
            }
            return true;

        }
        else if (isspace(byte)) {
            // ignore
        }
        else {
            _error = INVALID_CHARACTER;
            return false; // error
        }
    }
    _error = EOF_BEFORE_END;
    return false;
}
