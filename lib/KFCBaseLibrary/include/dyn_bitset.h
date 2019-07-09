/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <algorithm>

class dynamic_bitset {
private:
    class bit_helper {
    public:
        bit_helper(dynamic_bitset *bitset, uint8_t index);

        bit_helper &operator=(unsigned int value) {
            _bitset->set(_index, (bool)value);
            return *this;
        }
        bit_helper &operator=(const bit_helper &helper) {
            _bitset->set(_index, (bool)helper);
            return *this;
        }
        operator bool() const {
            return _bitset->test(_index);
        }

    private:
        uint8_t _index;
        dynamic_bitset *_bitset;
    };

public:
    static const uint8_t defaultSize = sizeof(int32_t) << 3;

    dynamic_bitset();
    dynamic_bitset(uint8_t maxSize);
    dynamic_bitset(uint8_t size, uint8_t maxSize);
    dynamic_bitset(uint32_t value, uint8_t size, uint8_t maxSize);
    dynamic_bitset(const char *fromBytes, uint8_t length, uint8_t size);
    ~dynamic_bitset();

    dynamic_bitset & operator =(unsigned int value) {
        setValue(value);
        return *this;
    }
    dynamic_bitset & operator =(const dynamic_bitset &bitset) {
		setMaxSize(bitset._maxSize);
        setSize(bitset._size);
        memcpy(_buffer, bitset._buffer, ((_maxSize + 7) >> 3));
        return *this;
    }
	dynamic_bitset & operator =(const char * str) {
		setString(str);
		return *this;
    }
    bit_helper operator [](unsigned int index) {
        return bit_helper(this, index);
    }
    bool operator [](unsigned int index) const {
        return test(index);
    }

    void setBytes(uint8_t *bytes, uint8_t len);
    const uint8_t *getBytes() const;
    String getBytesAsString() const;

    void setValue(uint32_t value);
    const uint32_t getValue() const;

#if defined(__int64_t_defined)
    void setValueInt64(uint64_t value) {
        memcpy(_buffer, &value, std::min((int)sizeof(value), ((_maxSize + 7) >> 3)));
    }

    uint64_t getValueInt64() {
        uint64_t value;
        memcpy(&value, _buffer, std::min((int)sizeof(value), ((_maxSize + 7) >> 3)));
        return value;
    }
#endif

    void setString(const String &str) {
        setString(str.c_str(), str.length());
    }
	void setString(const char *str) {
        setString(str, strlen(str));
    }
    void setString(const char *str, size_t len);

    void setSize(uint8_t size);
    const uint8_t size() const;
    void setMaxSize(uint8_t maxSize);
    const uint8_t getMaxSize() const;

    void fromString(const char *pattern, bool reverse = false);
    String toString() const;

    void set(uint8_t pos, bool value = true);
    bool test(uint8_t pos) const;

    String generateCode(const char *variable = "pattern", bool setBytes = false) const;

private:
    void _init();

private:
    uint8_t _size;
    uint8_t _maxSize;
    uint8_t *_buffer;
};

/*
*/
