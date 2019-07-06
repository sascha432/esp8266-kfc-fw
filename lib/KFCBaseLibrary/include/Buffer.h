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

    void clear();
    uint8_t *dupClear();

    uint8_t *get() const;
    const uint8_t *getConst() const;
    char *getChar() const;
    const char *getBuffer() const;
    const char *getConstChar() const;
    char *getNulByteString();

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
