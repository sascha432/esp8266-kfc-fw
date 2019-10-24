/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class Buffer {
public:
    static const int MALLOC_PADDING = 16;

    Buffer(size_t size = 0);
    virtual ~Buffer();

    bool operator ==(const Buffer &buffer) const;
    bool operator !=(const Buffer &buffer) const;

    bool equals(const Buffer &buffer) const;

    void clear();
    uint8_t *dupClear();

    inline uint8_t *get() const {
        return _buffer;
    }
    inline const uint8_t *getConst() const {
        return reinterpret_cast<const uint8_t *>(_buffer);
    }
    inline char *getChar() const {
        return reinterpret_cast<char *>(_buffer);
    }
    inline const char *getBuffer() const {
        return reinterpret_cast<const char *>(_buffer);
    }
    inline const char *getConstChar() const {
        return reinterpret_cast<const char *>(_buffer);
    }
    char *getNulByteString();

    inline char charAt(size_t index) const {
        return _buffer[index];
    }

    void setBuffer(uint8_t *buffer, size_t size);

    size_t size() const;
    size_t length() const;

    bool reserve(size_t size, bool shrink = false);
    bool available(size_t len);

    String toString() const;
    operator String() const;

    size_t write(uint8_t data) {
        return write(&data, sizeof(data));
    }
    size_t write(int data) {
        return write((uint8_t)data);
    }
    size_t write(char data) {
        return write((uint8_t)data);
    }

    size_t write(const uint8_t *data, size_t len) {
        return write((uint8_t *)data, len);
    }
    size_t write(const char *data, size_t len) {
        return write((uint8_t *)data, len);
    }
    size_t write(char *data, size_t len) {
        return write((uint8_t *)data, len);
    }
    size_t write(uint8_t *data, size_t len);
    size_t write_P(PGM_P data, size_t len);

    void remove(unsigned int index, size_t count);
    void removeAndShrink(unsigned int index, size_t count);

    Buffer &operator+=(char data);
    Buffer &operator+=(uint8_t data);
    Buffer &operator+=(const char *data);
    Buffer &operator+=(const String &data);

protected:
    uint8_t *_buffer;

private:
    void _free();

    size_t _length;
    size_t _size;
};
