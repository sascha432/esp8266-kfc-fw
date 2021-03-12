/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <algorithm>
#include <type_traits>

// bitset with dynamic size and predefined max. bits up to 65535 bits
// default object size is 4 byte and stores up to 24bit
// 32bit is 5 (or 8 byte depending on the alignment)
// 56bit is 8 byte
template<size_t _MaxBits = 24>
class dynamic_bitset {
public:
    using SizeType = typename std::conditional<(_MaxBits > 255), uint16_t, uint8_t>::type;
    using BytesSizeType = typename std::conditional<((_MaxBits / 8) > 255), uint16_t, uint8_t>::type;

    // max. capacity in bits
    static constexpr size_t kMaxSize = _MaxBits;
    static constexpr size_t kMaxBits = _MaxBits;
    // max capacity in bytes
    static constexpr size_t kCapacity = (_MaxBits + 7) / 8;
    static constexpr size_t kMaxBytes = kCapacity;

private:
    // template<size_t _HelperMaxBits = _MaxBits>
    class bit_helper {
    private:
        friend dynamic_bitset;

        bit_helper(dynamic_bitset *bitset, SizeType index)
        {
            _index = index;
            _bitset = bitset;
        }

    public:
        bit_helper &operator=(bool value) {
            _bitset->set(_index, value);
            return *this;
        }

        bit_helper &operator=(const bit_helper &helper) {
            _bitset->set(_index, static_cast<bool>(helper));
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
    dynamic_bitset(SizeType size = 0) : _size(size), _buffer{} {}

    dynamic_bitset(uint8_t value, SizeType size) : _size(size), _buffer{} {
        set(value);
    }

    dynamic_bitset(uint16_t value, SizeType size) : _size(size), _buffer{} {
        set(value);
    }

    dynamic_bitset(uint32_t value, SizeType size) : _size(size), _buffer{} {
        set(value);
    }

    dynamic_bitset(uint64_t value, SizeType size) : _size(size), _buffer{} {
        set(value);
    }

    // size in bits must not exceed the size of data
    dynamic_bitset(const char *data, SizeType size) : dynamic_bitset(reinterpret_cast<const uint8_t *>(data), size) {}
    dynamic_bitset(const uint8_t *data, SizeType size) : _size(size) {
        setBytes(data, size);
    }

    template<size_t _FromMaxBits>
    dynamic_bitset(const dynamic_bitset<_FromMaxBits> &bits) {
        *this = bits;
    }

    // copy from another dynamic bitset with different capacity
    // size is trucated at capacity()
    template<size_t _FromMaxBits>
    dynamic_bitset &operator =(const dynamic_bitset<_FromMaxBits> &bitset)
    {
        _size = std::min<size_t>(bitset.size(), kMaxSize);
        std::copy_n(bitset.data(), std::min<size_t>(capacity(), bitset.capacity()), data());
        return *this;
    }

    // size does not change using the = operator
    // use setSize() before
    inline __attribute__((__always_inline__))
	dynamic_bitset &operator=(const char * str) {
		setString(str);
		return *this;
    }

    // size does not change using the = operator
    // use setSize() before
    inline __attribute__((__always_inline__))
	dynamic_bitset &operator=(const String &str) {
		setString(str);
		return *this;
    }

    // size does not change using the = operator
    // use setSize() before
    inline __attribute__((__always_inline__))
	dynamic_bitset &operator=(const __FlashStringHelper *str) {
        setString(str);
		return *this;
    }

    // size does not change using the = operator
    // use setSize() before or the set() method directly
    template<typename _Ta>
    inline __attribute__((__always_inline__))
    dynamic_bitset &operator=(_Ta value) {
        set<_Ta>(value, _size);
        return *this;
    }

    inline __attribute__((__always_inline__))
    operator int() const {
        return get<int>();
    }

    inline __attribute__((__always_inline__))
    explicit operator uint8_t() const {
        return get<uint8_t>();
    }

    inline __attribute__((__always_inline__))
    explicit operator uint16_t() const {
        return get<uint16_t>();
    }

    inline __attribute__((__always_inline__))
    explicit operator uint32_t() const {
        return get<uint32_t>();
    }

    inline __attribute__((__always_inline__))
    explicit operator uint64_t() const {
        return get<uint64_t>();
    }

    bit_helper operator [](SizeType index) {
        return bit_helper(this, index);
    }

    bool operator [](SizeType index) const {
        return test(index);
    }

    inline __attribute__((__always_inline__))
    bool operator==(int value) const {
        return get<int>() == value;
    }

    inline __attribute__((__always_inline__))
    bool operator!=(int value) const {
        return get<int>() != value;
    }

    template<typename _Ta>
    inline __attribute__((__always_inline__))
    bool operator==(std::pair<SizeType, _Ta> value) const {
        return value.first == _size && value.second == get<_Ta>();
    }

    template<typename _Ta>
    inline __attribute__((__always_inline__))
    bool operator!=(std::pair<SizeType, _Ta> value) const {
        return value.first != _size || value.second != get<_Ta>();
    }

    // PROGMEM safe
    inline __attribute__((__always_inline__))
    void setBytes(const char *buffer, SizeType size) {
        setBytes(reinterpret_cast<const uint8_t *>(buffer), size);
    }

    // size in bits must not exceed sizeof(buffer)
    // PROGMEM safe
    void setBytes(const uint8_t *buffer, SizeType size) {
        memcpy_P(data(), buffer, _size_limit(size));
    }

    inline __attribute__((__always_inline__))
    const uint8_t *data() const {
        return _buffer;
    }

    inline __attribute__((__always_inline__))
    uint8_t *data() {
        return _buffer;
    }

    inline __attribute__((__always_inline__))
    void setString(const String &str) {
        setString(str.c_str(), str.length());
    }

    // PROGMEM safe
    inline __attribute__((__always_inline__))
	void setString(const char *str) {
        setString(str, strlen(str));
    }

    inline __attribute__((__always_inline__))
	void setString(const __FlashStringHelper *str) {
		setString(reinterpret_cast<PGM_P>(str), strlen_P(reinterpret_cast<PGM_P>(str)));
    }

    // set bits from string, '1' = set
    // first byte in the string is the highest bit
    // if bigger, the upper bits are truncated (first bytes of the string)
    // if smaller, the upper bits are zero filled (left side zero padded)
    // PROGMEM safe
    void setString(const char *str, size_t len) {
        zero_fill();
        if (len > kMaxSize) {
            // "100100" -> 4 bit 0b0100
            //                     3210 (len inside loop)
            // strlen() 6, kMaxSize 4, offset 2, len 4
            int offset = len - kMaxSize;
            len -= offset;
            str += offset;
        }
        // filling in reverse does not require any offset if equal or smaller
        // "110" -> 5 bit 0b00110
        //                    210 (len inside loop)
        while (len--) {
            set(len, pgm_read_byte(str++) == '1');
        }
    }

    // buffer capacity in bytes
    static constexpr SizeType capacity() {
        return kCapacity;
    }

    void setSize(SizeType bits) {
        auto newSize = _size_limit(bits, kMaxSize);
        if (newSize > _size) {
            zero_fill_unused(); // fill unused space before resizing
            _size = newSize;
        }
    }

    SizeType size() const {
        return _size;
    }

    // requires size in bytes to store bitset
    BytesSizeType minSize() const {
        return ((_size + 7) >> 3);
    }

    void fromString(const String &str, bool reverse = true) {
        fromString(str.c_str(), reverse, str.length());
    }

    // PROGMEM safe
    void fromString(const char *pattern, bool reverse = true) {
        fromString(pattern, reverse, strlen_P(pattern));
    }

    // PROGMEM safe
    void fromString(const char *pattern, bool reverse, size_t len) {
        zero_fill();
        _size = std::min<SizeType>(static_cast<SizeType>(len), kMaxBits);
        auto endPtr = pattern + _size;
        auto startPtr = reverse ? pattern : &pattern[len - _size];
        uint8_t pos = 0;
        uint8_t endPos = _size - 1;
        if (reverse) {
            std::swap(pos, endPos);
        }
        int ch;
        while (startPtr < endPtr && pos != endPos && (ch = pgm_read_byte(startPtr++)) != 0) {
            set(pos, ch == '1');
            reverse ? --pos : ++pos;
        }
    }

    String toString(uint8_t groups = 0) const {
        String output;
        output.reserve(_size);
        for (int i = _size - 1; i >= 0; i--) {
            output += (test(i) ? '1' : '0');
            if ((groups > 1) && i && ((i + groups) % groups == 0)) {
                output += ' ';
            }
        }
        return output;
    }

    // returns bits as C string
    // unused bits in the last byte are zero filled
    // 0b0010100000 (size=10) = "\x0a\x00"
    String getBytesAsString() const {
        String output;
        for (int i = minSize() - 1; i >= 0; i--) {
            char buf[4];
            snprintf_P(buf, sizeof(buf), PSTR("\\x%02x"), static_cast<uint8_t>(_buffer[i]));
            output += buf;
        }
        return output;
    }

    void set(SizeType pos, bool value = true) {
        if (pos < kMaxBits) {
            uint8_t bytePos = (pos >> 3);
            uint8_t mask = (1 << (pos - (bytePos << 3)));
            if (value) {
                data()[bytePos] |= mask;
            }
            else {
                data()[bytePos] &= ~mask;
            }
        }
    }

    bool test(SizeType pos) const {
        uint8_t bytePos = (pos >> 3);
        return (pos < kMaxBits) ? data()[bytePos] & (1 << (pos - (bytePos << 3))) : false;
    }

    void dumpCode(Print &output, const char *variable = "pattern", bool setBytes = false) const {
        auto numBytes = minSize();
        if (setBytes) {
            if (numBytes > 8) {
                output.printf_P(PSTR("%s.setBytes<%u>("), variable, kMaxSize);
            }
            else {
                output.printf_P(PSTR("%s.set("), variable);
            }
        }
        else {
            output.printf_P(PSTR("auto %s = dynamic_bitset<%u>("), variable, kMaxSize);

            if (numBytes > 8) {
                output.print('\\');
                output.print('"');
                output.print(getBytesAsString());
                output.printf_P(PSTR("\", %u);\n"), _size);
            }
            else if (numBytes > 4) {
                auto split = static_cast<uint64_t>(this);
                output.printf_P(PSTR("static_cast<uint64_t>(0x%08x%08x)"), static_cast<uint32_t>(split >> 32), static_cast<uint32_t>(split & 0xffffffff));
            } else {
                output.printf_P(PSTR("static_cast<uint32_t>(0x%08x)"), static_cast<uint32_t>(this));
            }

            output.printf_P(PSTR(", %u);\n"), _size);
        }
    }

    template<typename _Ta>
    inline __attribute__((__always_inline__))
    void set(_Ta value, SizeType size) {
        zero_fill();
        _size = static_cast<uint8_t>(std::min<size_t>(std::min<size_t>(sizeof(value) * 8, kMaxBits), size));
        memcpy_P(data(), &value, minSize());
        // std::copy_n(reinterpret_cast<uint8_t *>(&value), minSize(), data());
    }

    template<typename _Ta>
    inline __attribute__((__always_inline__))
    _Ta get() const {
        auto value = _Ta();
        std::copy_n(data(), _size_limit(sizeof(value)), reinterpret_cast<uint8_t *>(&value));
        return value;
    }

private:
    // returns size or capacity, depending on which is msaller
    inline __attribute__((__always_inline__))
    BytesSizeType _size_limit(size_t size) const {
        return static_cast<BytesSizeType>(std::min<size_t>(static_cast<size_t>(size), capacity()));
    }

    inline __attribute__((__always_inline__))
    void zero_fill() {
        std::fill_n(data(), capacity(), 0);
    }

    void zero_fill_unused() {
        auto lastByte = static_cast<BytesSizeType>(_size >> 3);
        // keep bits of the last byte
        _buffer[lastByte] &= (1 << (size - (lastByte << 3))) - 1;
        lastByte++;
        std::fill(_buffer + lastByte, _buffer + capacity(), 0);
    }


private:
    struct __attribute__((packed)) {
        SizeType _size;
        uint8_t _buffer[kCapacity];
    };
};

static constexpr size_t kSize24 = sizeof(dynamic_bitset<24>);
static constexpr size_t kSize32 = sizeof(dynamic_bitset<32>);
static constexpr size_t kSize56 = sizeof(dynamic_bitset<56>);
