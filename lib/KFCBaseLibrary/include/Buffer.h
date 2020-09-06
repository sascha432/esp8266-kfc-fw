/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_BUFFER
#define DEBUG_BUFFER                            0
#endif

#ifndef DEBUG_BUFFER_ALLOC
#define DEBUG_BUFFER_ALLOC                      0
#endif

#ifndef BUFFER_ZERO_FILL
#define BUFFER_ZERO_FILL                        0
#endif

#if DEBUG_BUFFER
#define __DBG_BUFFER_assert(...)                assert(__VA_ARGS__)
#define __DBG_BUFFER_asserted(cmp, ...)         { auto res = (bool)(__VA_ARGS__); __DBG_BUFFER_assert(res == cmp); }
#else
#define __DBG_BUFFER_assert(...)
#define __DBG_BUFFER_asserted(cmp, ...)         __VA_ARGS__
#endif

#if DEBUG_BUFFER_ALLOC
#define IF_DEBUG_BUFFER_ALLOC(...)              __VA_ARGS__
#else
#define IF_DEBUG_BUFFER_ALLOC(...)
#endif

class Buffer;

class MoveStringHelper : public String {
public:
    MoveStringHelper();
    static void move(Buffer &buffer, String &&move);
};

class Buffer {
public:
    using reference = uint8_t &;
    using const_reference = const uint8_t &;
    using pointer = uint8_t *;
    using const_pointer = const uint8_t *;
    using value_type = uint8_t;
    using size_type = size_t;
    using difference_type = typename std::make_signed<size_type>::type;
    using iterator_category = std::random_access_iterator_tag;

    class back_inserter : public std::iterator<std::output_iterator_tag, void, void, void, void>
    {
    public:
        back_inserter(Buffer &buffer) : _buffer(&buffer) {}

        template<class Ta>
        back_inserter operator =(const Ta &obj) {
            _buffer->push_back(obj);
            return *this;
        }

        back_inserter operator++() noexcept {
            return *this;
        }

        back_inserter operator++(int) noexcept {
            return *this;
        }

        back_inserter &operator*() {
            return *this;
        }

    private:
        Buffer *_buffer;
    };

    class iterator : public std::iterator<iterator_category, value_type, difference_type, pointer, reference> {
    public:
        iterator(Buffer &buffer, size_type offset) : _buffer(&buffer), _offset(offset) {
        }

        iterator(const Buffer &buffer, size_type offset) : iterator(const_cast<Buffer &>(buffer), offset) {
        }

        iterator &operator=(const iterator iter) {
            _offset = iter._offset;
            return *this;
        }

        const iterator operator+(difference_type step) const {
            auto tmp = *this;
            tmp += step;
            return tmp;
        }

        iterator operator+(difference_type step) {
            auto tmp = *this;
            tmp += step;
            return tmp;
        }

        difference_type operator-(const iterator iter) const {
            return _offset - iter._offset;
        }

        const iterator operator-(difference_type step) const {
            auto tmp = *this;
            tmp -= step;
            return tmp;
        }

        iterator operator-(difference_type step) {
            auto tmp = *this;
            tmp -= step;
            return tmp;
        }

        iterator &operator+=(difference_type step) {
            _offset += step;
            return *this;
        }

        iterator &operator-=(difference_type step) {
            _offset -= step;
            return *this;
        }

        iterator &operator++() {
            ++_offset;
            return *this;
        }

        iterator &operator--() {
            --_offset;
            return *this;
        }

        const_pointer operator*() const {
            return &_buffer->operator[](_offset);
        }

        pointer operator*() {
            return &_buffer->_buffer[_offset];
        }

        bool operator==(iterator iter) const {
            return _offset == iter._offset;
        }

        bool operator!=(iterator iter) const {
            return _offset != iter._offset;
        }

    private:
        friend Buffer;

        Buffer *_buffer;
        size_type _offset;
    };

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
        if (_length == _fp_size && reserve(_length + 1)) {
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
    inline uint8_t operator[](size_t index) const {
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

public:
     template <class T>
     void push_back(const T &data) {
         __DBG_BUFFER_asserted(sizeof(T), write(reinterpret_cast<const uint8_t *>(&data), sizeof(T)));
     }

     inline void push_back(uint8_t data) {
         __DBG_BUFFER_asserted(1, write(data));
     }

     inline void push_back(const char *str) {
         __DBG_BUFFER_asserted(strlen(str), write(str));
     }

     inline void push_back(const String &str) {
         __DBG_BUFFER_asserted(str.length(), write(str));
     }

     inline void push_back(const __FlashStringHelper *fpstr) {
         __DBG_BUFFER_asserted(strlen_P(reinterpret_cast<PGM_P>(fpstr)), write(fpstr, strlen_P(reinterpret_cast<PGM_P>(fpstr))));
     }

     template <typename Ta, typename std::enable_if<std::is_pointer<Ta>::value, int>::type = 0>
     inline size_t copy(const Ta first, const Ta last) {
         return write(
            reinterpret_cast<const uint8_t *>(first),
            reinterpret_cast<const uint8_t *>(last) - reinterpret_cast<const uint8_t *>(first) + sizeof(std::remove_pointer<Ta>::type) - 1
         );
     }

     //
     // Write raw data of objects to the buffer
     //
     // specialized version for String, const char * and const __FlashStringHelper * implemented
     //
     // Buffer::copy(begin_iterator, end_iterator)
     // or
     // std::copy(begin_iterator, end_iterator, Buffer::back_inserter(buffer));
     //

     template <typename Ta, typename std::iterator_traits<Ta>::iterator_category* = nullptr, typename std::enable_if<!std::is_pointer<Ta>::value, int>::type = 0>
     void copy(Ta first, Ta last) {
         for (auto iterator = first; iterator != last; ++iterator) {
             push_back(*iterator);
         }
     }

protected:
    bool _changeBuffer(size_t newSize);
    void _remove(size_t index, size_t count);

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
#if DEBUG_BUFFER_ALLOC

public:
    void dumpAlloc(Print &output);

    uint32_t _realloc;
    uint32_t _alloc;
    uint32_t _free;
#endif
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
