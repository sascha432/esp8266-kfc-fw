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
        return getNulByteString();
    }
    inline char *begin_str() {
        return getNulByteString();
    }
    char *getNulByteString(); // return NUL terminated string

    inline uint8_t charAt(size_t index) const {
        return _buffer[index];
    }
    inline uint8_t &charAt(size_t index) {
        return _buffer[index];
    }
    uint8_t &operator[](size_t index) {
        return _buffer[index];
    }

    void setBuffer(uint8_t *buffer, size_t size);

    size_t size() const;
    size_t length() const;

    uint8_t *begin() const {
        return _buffer;
    }
    uint8_t *end() const {
        return _buffer + _length;
    }

    const uint8_t *cbegin() const {
        return _buffer;
    }
    const uint8_t *cend() const {
        return _buffer + _length;
    }

    const char *cstr_begin() const {
        return reinterpret_cast<const char *>(_buffer);
    }
    const char *cstr_end() const {
        return reinterpret_cast<const char *>(_buffer + _length);
    }

    bool reserve(size_t newSize);
    bool shrink(size_t newSize = 0);
    bool _changeBuffer(size_t newSize);

    // bool available(size_t len) const;
    size_t available() const;

    size_t write(uint8_t data) {
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
    size_t writeObject(const T &data) {
        return write(reinterpret_cast<const uint8_t *>(&data), sizeof(T));
    }
    template<class T>
    size_t writeVector(std::vector<T> vector) {
        return write(reinterpret_cast<const uint8_t *>(vector.data()), vector.size() * sizeof(T));
    }

    template<class T>
    size_t writeRange(T first, T last) {
        size_t written = 0;
        for(auto iterator = first; iterator != last; ++iterator) {
            written += writeObject<typename T::value_type>(*iterator);
        }
        return written;
    }

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

    void setLength(size_t length) {
        _length = length;
    }

protected:
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
