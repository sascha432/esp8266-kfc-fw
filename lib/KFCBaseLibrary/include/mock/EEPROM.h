/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Arduino_compat.h"

class EEPROMFile : public Stream {
public:
    EEPROMFile(uint16_t size = 4096) {
        _position = 0;
        _size = size;
        _eeprom = nullptr;
    }
    ~EEPROMFile() {
        close();
        if (_eeprom) {
            free(_eeprom);
        }
    }

    void clear() {
        begin();
        memset(_eeprom, 0, _size);
        commit();
    }

    size_t length() const {
        return _size;
    }

    uint8_t *getDataPtr() const {
        return _eeprom;
    }

    uint8_t *getConstDataPtr() const {
        return _eeprom;
    }

    uint8_t& operator[](int const address) {
        return _eeprom[address];
    }

    uint8_t const & operator[](int const address) const {
        return _eeprom[address];
    }

    uint8_t read(int const address) {
        return _eeprom[address];
    }

    void write(int const address, uint8_t const val) {
        _eeprom[address] = val;
    }

    template<typename T>
    T &get(int const address, T &t) {
        if (address < 0 || address + sizeof(T) > _size)
            return t;

        memcpy((uint8_t*) &t, _eeprom + address, sizeof(T));
        return t;
    }

    template<typename T>
    const T &put(int const address, const T &t) {
        if (address < 0 || address + sizeof(T) > _size) {
            return t;
        }
        memcpy(_eeprom + address, (const uint8_t*)&t, sizeof(T));
        return t;
    }

    bool begin(int size = -1) {
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

    void end() {
        if (_eeprom) {
            commit();
            //free(_eeprom);
            //_eeprom = nullptr;
        }
    }

    bool commit() {
        FILE *fp;
        errno_t err = fopen_s(&fp, "eeprom.bin", "wb");
        if (err) {
            return false;
        }
        if (fwrite(_eeprom, _size, 1, fp) != 1) {
            perror("write");
            return false;
        }
        fclose(fp);
        return true;
    }

    void close() {
        end();
    }

    size_t position() {
        return _position;
    }

    bool seek(long pos, int mode) override {
        if (mode == SeekSet) {
            if (pos >= 0 && pos < _size) {
                _position = (uint16_t)pos;
                return true;
            }
        } else if (mode == SeekCur) {
            return seek(_position + pos, SeekSet);
        } else if (mode == SeekEnd) {
            return seek(_size - pos, SeekSet);
        }
        return false;
    }
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }

    int read() override {
        uint8_t ch;
        if (read(&ch, sizeof(ch)) != 1) {
            return -1;
        }
        return ch;
    }
    int read(uint8_t *buffer, size_t len) {
        if (_position + len >= _size) {
            len = _size - _position;
        }
        memcpy(buffer, _eeprom + _position, len);
        _position += (uint16_t)len;
        return len;
    }


    size_t write(char data) {
        return write((uint8_t)data);
    }
    size_t write(uint8_t data) {
        return write(&data, sizeof(data));
    }
    size_t write(uint8_t *data, size_t len) {
        if (_position + len >= _size) {
            len = _size - _position;
        }
        memcpy(_eeprom + _position, data, len);
        _position += (uint16_t)len;
        return len;
    }

    void flush() {
    }

    size_t size() const override {
        return _size;
    }

    int available() override {
        return _size - _position;
    }

private:
    FILE *fp;
    uint8_t *_eeprom;
    uint16_t _position;
    uint16_t _size;
};


typedef EEPROMFile EEPROMClass;

extern EEPROMClass EEPROM;
