/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class Buffer;

class MoveStringHelper : public String {
public:
    MoveStringHelper();
    void move(Buffer &stream);
};

class Buffer {
public:
    Buffer();
    Buffer(Buffer &&buffer) noexcept;
    Buffer(size_t size);
    virtual ~Buffer();

    Buffer(const __FlashStringHelper *str);
    Buffer(String &&str);
    Buffer(const String &str);

    Buffer &operator =(Buffer &&buffer) noexcept;
    Buffer &operator =(const Buffer &buffer);

    Buffer &operator =(const String &str);
    Buffer &operator =(String &&str);

    inline bool operator ==(const Buffer &buffer) const {
        return equals(buffer);
    }
    inline bool operator !=(const Buffer &buffer) const {
        return !equals(buffer);
    }

    bool equals(const Buffer &buffer) const;

    void clear();
    // move buffer pointer. needs to be freed with free() if not null
    void move(uint8_t** ptr);

    inline uint8_t *operator*() const {
        return _buffer;
    }
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

    String toString() const;
    inline const char *c_str() {
        return begin_str();
    }
    // return NUL terminated string
    inline char *begin_str() {
#if 0
        // the buffer is filled zero padded already
        if (_length == _size && reserve(_length + 1)) {
            _buffer[_length] = 0;
        }
#else
        // safe version in case _buffer was modified outside and is not zero padded anymore
        _length -= write(0);
#endif
        return reinterpret_cast<char *>(_buffer);
    }

    inline uint8_t charAt(size_t index) const {
        return _buffer[index];
    }
    inline uint8_t &charAt(size_t index) {
        return _buffer[index];
    }
    inline uint8_t &operator[](size_t index) {
        return _buffer[index];
    }

    void setBuffer(uint8_t *buffer, size_t size);

    inline size_t size() const {
        return _size;
    }
    inline size_t capacity() const {
        return _size;
    }

    inline size_t length() const {
        return _length;
    }

    inline uint8_t *begin() const {
        return _buffer;
    }
    inline uint8_t *end() const {
        return _buffer + _length;
    }

    inline const uint8_t *cbegin() const {
        return begin();
    }
    inline const uint8_t *cend() const {
        return end();
    }

    inline const char *cstr_begin() const {
        return reinterpret_cast<const char *>(begin());
    }
    inline const char *cstr_end() const {
        return reinterpret_cast<const char *>(end());
    }

    // reserve buffer
    // on success size() will be greater or equal newSize
    // size() will not decrease
    // length() does not change
    inline bool reserve(size_t newSize) {
        return (newSize <= _size || _changeBuffer(newSize));
    }
    // reduce size and length
    // on success size() will be greater or equal newSize
    // length() will be smaller or equal newSize
    inline bool shrink(size_t newSize) {
        return (newSize >= _size) || _changeBuffer(newSize);
    }
    // reduce size to fit the existing content
    // on success size() will be greater or equal _length
    inline bool shrink_to_fit() {
        return shrink(_length);
    }
    // increase or decrease size, filling unused space with zeros
    // on success size() will be greater or equal newSize
    // length() will be smaller or equal newSize
    inline bool resize(size_t newSize) {
        return (newSize != _size) || _changeBuffer(newSize);
    }

    // bool available(size_t len) const;
    size_t available() const;

    // fastest single byte write() version
    inline size_t write(uint8_t data) {
        if (_length < _size) {
            _buffer[_length++] = data;
            return 1;
        }
        return write(&data, sizeof(data));
    }

    size_t write(const uint8_t *data, size_t len);
    size_t write_P(PGM_P data, size_t len);

    inline size_t write(const char *data, size_t len) {
        return write(reinterpret_cast<const uint8_t *>(data), len);
    }
    inline size_t write(const __FlashStringHelper *data, size_t len) {
        return write_P(reinterpret_cast<PGM_P>(data), len);
    }
    inline size_t write(const String &str) {
        return write(str.c_str(), str.length());
    }

    template<class T>
    size_t copy(const T &data) {
        return write(reinterpret_cast<const uint8_t *>(&data), sizeof(T));
    }

    template<class T>
    size_t copy(T first, T last) {
        size_t written = 0;
        for(auto iterator = first; iterator != last; ++iterator) {
            // written += copy<typename T::value_type>(*iterator);
            written += copy<decltype(*iterator)>(*iterator);
        }
        return written;
    }

    // read one byte and remove it from the buffer
    // release memory after reading the last byte
    // returns -1 if no data is available
    int read();
    // reads one byte without removing
    int peek();

    void remove(size_t index, size_t count);
    // reduce size if more than minFree bytes are available
    void removeAndShrink(size_t index, size_t count, size_t minFree = 32);

    inline Buffer &operator+=(char data) {
        write(data);
        return *this;
    }
    inline Buffer &operator+=(uint8_t data) {
        write(data);
        return *this;
    }
    Buffer &operator+=(const char *str);
    Buffer &operator+=(const String &str);
    Buffer &operator+=(const __FlashStringHelper *str);
    Buffer &operator+=(const Buffer &buffer);

    void setLength(size_t length);

protected:
    bool _changeBuffer(size_t newSize);

    inline uint8_t *_data_end() const {
        return _buffer + _length;
    }
    inline uint8_t *_buffer_end() const {
        return _buffer + _size;
    }

    uint8_t *_buffer;
    size_t _length;
    size_t _size;

private:
    inline size_t _alignSize(size_t size) const {
#if ESP8266 || ESP32 || 1
        return (size + 7) & ~7; // 8 byte aligned, umm block size
#else
        return size;
#endif
    }
};

inline size_t Buffer::available() const
{
    if (_length < _size) {
        return _size - _length;
    }
    return 0;
}

inline void Buffer::setLength(size_t length)
{
    _length = length;
}

inline Buffer &Buffer::operator+=(const char *str)
{
    write(str, strlen(str));
    return *this;
}

inline Buffer &Buffer::operator+=(const String &str)
{
    write(str.c_str(), str.length());
    return *this;
}

inline Buffer &Buffer::operator+=(const __FlashStringHelper *str)
{
    auto pstr = reinterpret_cast<PGM_P>(str);
    write_P(pstr, strlen_P(pstr));
    return *this;
}

inline Buffer &Buffer::operator+=(const Buffer &buffer)
{
    write(buffer.get(), buffer.length());
    return *this;
}
