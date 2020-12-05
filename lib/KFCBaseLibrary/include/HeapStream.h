/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// Stream that can read from memory without allocating any heap and no overhead like StreamString
// See ProgmemStream to read from flash memory directly

#include <Arduino_compat.h>

class HeapStream : public Stream
{
public:
    HeapStream() : _available(0) {
    }
    HeapStream(const char *data, size_t length) : _available(length) {
        setData(data);
    }
    HeapStream(const uint8_t *data, size_t length) : HeapStream(reinterpret_cast<const char *>(data), length) {
    }
    HeapStream(const uint8_t *data) : HeapStream(data, 0) {
    }
    // c string
    HeapStream(const char *data) : HeapStream(data, strlen(data)) {
    }
    // String class
    HeapStream(String &data) : HeapStream(data.c_str(), data.length()) {
    }

    void setLength(size_t length) {
        _available = length;
        _dataPtr = _data;
    }

    void setData(const char *data) {
        _dataPtr = data;
        _data = data;
    }
    void setData(const uint8_t *data) {
        setData(reinterpret_cast<const char *>(data));
    }

    void setData(const char *data, size_t length) {
        setData(data);
        _available = length;
    }
    void setData(const uint8_t *data, size_t length) {
        setData(reinterpret_cast<const char *>(data), length);
    }

    virtual int available() {
        return _available;
    }

    virtual int read() {
        if (_available) {
            _available--;
            return *_dataPtr++;
        }
        return -1;
    }

    virtual int peek() {
        if (_available) {
            return *_dataPtr;
        }
        return -1;
    }

    virtual size_t write(uint8_t data) { // write is not supported
        return 0;
    }

    virtual void flush() {
    }

protected:
    size_t _available;
    const char *_dataPtr;
    const char *_data;
};


class WriteableHeapStream : public HeapStream
{
public:
    WriteableHeapStream() : HeapStream() {
    }
    WriteableHeapStream(char *data, size_t length) : HeapStream(data, 0), _size(length), _outPtr(data) {
    }
    WriteableHeapStream(uint8_t *data, size_t length) : WriteableHeapStream(reinterpret_cast<char *>(data), length) {
    }

    virtual size_t write(uint8_t data) { // write is not supported
        if (!_size) {
            return 0;
        }
        _size--;
        *_outPtr++ = data;
        return 1;
    }

private:
    size_t _size;
    char *_outPtr;
};
