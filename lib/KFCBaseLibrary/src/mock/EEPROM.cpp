/**
* Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#include <Arduino_compat.h>
#include "EEPROM.h"

EEPROMFile::EEPROMFile(uint16_t size) : fp(nullptr), _eeprom(nullptr), _position(0), _size(size) {
}

EEPROMFile::~EEPROMFile() {
    close();
    if (_eeprom) {
        free(_eeprom);
    }
}

int EEPROMFile::peek() {
    if (_position < _size) {
        return _eeprom[_position];
    }
    return -1;
}

int EEPROMFile::read() {
    uint8_t buf[1];
    auto len = read(buf, 1);
    return len == 1 ? buf[0] : -1;
}

int EEPROMFile::available() {
    return _size - _position;
}

void EEPROMFile::clear() {
    begin();
    memset(_eeprom, 0, _size);
    commit();
}

size_t EEPROMFile::length() const {
    return _size;
}

uint8_t *EEPROMFile::getDataPtr() const {
    return _eeprom;
}

uint8_t *EEPROMFile::getConstDataPtr() const {
    return _eeprom;
}

uint8_t EEPROMFile::read(int const address) {
    return _eeprom[address];
}

void EEPROMFile::write(int const address, uint8_t const val) {
    _eeprom[address] = val;
}

bool EEPROMFile::begin(int size) {
    if (_eeprom) {
        free(_eeprom);
    }
    _eeprom = (uint8_t *)malloc(_size);
    if (!_eeprom) {
        return false;
    }
    FILE *fp;
    errno_t err = fopen_s(&fp, "eeprom.bin", "rb");
    if (err) {
        memset(_eeprom, 0, _size);
        if (commit()) {
            return true;
        }
        return false;
    }
    if (fread(_eeprom, _size, 1, fp) != 1) {
        perror("read");
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

void EEPROMFile::end() {
    if (_eeprom) {
        commit();
        //free(_eeprom);
        //_eeprom = nullptr;
    }
}

bool EEPROMFile::commit() {
    FILE *fp;
    errno_t err = fopen_s(&fp, "eeprom.bin", "wb");
    if (err) {
        return false;
    }
    if (fp) {
        if (fwrite(_eeprom, _size, 1, fp) != 1) {
            perror("write");
            return false;
        }
        fclose(fp);
    }
    return true;
}

void EEPROMFile::close() {
    end();
}

size_t EEPROMFile::position() const {
    return _position;
}

bool EEPROMFile::seek(long pos, SeekMode mode) {
    if (mode == SeekSet) {
        if (pos >= 0 && pos < _size) {
            _position = (uint16_t)pos;
            return true;
        }
    }
    else if (mode == SeekCur) {
        return seek(_position + pos, SeekSet);
    }
    else if (mode == SeekEnd) {
        return seek(_size - pos, SeekSet);
    }
    return false;
}

int EEPROMFile::read(uint8_t *buffer, size_t len) {
    if (_position + len >= _size) {
        len = _size - _position;
    }
    memcpy(buffer, _eeprom + _position, len);
    _position += (uint16_t)len;
    return len;
}

size_t EEPROMFile::write(uint8_t data) {
    return write(&data, sizeof(data));
}

size_t EEPROMFile::write(uint8_t *data, size_t len) {
    if (_position + len >= _size) {
        len = _size - _position;
    }
    memcpy(_eeprom + _position, data, len);
    _position += (uint16_t)len;
    return len;
}

void EEPROMFile::flush() {
}

size_t EEPROMFile::size() const {
    return _size;
}

#endif
