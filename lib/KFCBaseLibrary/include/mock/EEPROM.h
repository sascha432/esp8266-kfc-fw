/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Arduino_compat.h"

class EEPROMFile : public Stream {
public:
    EEPROMFile(uint16_t size = 4096);
    ~EEPROMFile();

    void clear();
    size_t length() const;
    uint8_t *getDataPtr() const;
    uint8_t *getConstDataPtr() const;

    uint8_t& operator[](int const address) {
        return _eeprom[address];
    }

    uint8_t const & operator[](int const address) const {
        return _eeprom[address];
    }

    uint8_t read(int const address);
    void write(int const address, uint8_t const val);

    int peek();
    int read();
    int available();

    bool begin(int size = -1);
    void end();
    bool commit();
    void close();

    size_t position() const;
    bool seek(long pos, SeekMode mode);
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }
    int read(uint8_t *buffer, size_t len);
    size_t write(uint8_t data);
    size_t write(uint8_t *data, size_t len);
    void flush();
    size_t size() const;

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

private:
    FILE *fp;
    uint8_t *_eeprom;
    uint16_t _position;
    uint16_t _size;
};


typedef EEPROMFile EEPROMClass;

extern EEPROMClass EEPROM;
