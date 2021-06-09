/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

template<size_t _Bits, bool _Reverse>
class BitsToStr {
public:
    BitsToStr(uint32_t value) : BitsToStr(reinterpret_cast<uint8_t *>(&value)) {}
    BitsToStr(const uint8_t *data) {
        if (_Reverse) {
            _buffer[_Bits] = 0;
            _fill<false>(data, _buffer);
        }
        else {
            auto ptr = &_buffer[_Bits];
            *ptr = 0;
            _fill<true>(data, ptr);
        }
    }

    // merge "value" by replacing high and/or low bits with another character
    // for example
    //  BitsToStr<4, false>(0b101).merge(~0U, 'n', 'y') = "nyny"
    //  BitsToStr<5, false>(0b10101).merge(0b11011, 'x') = "10x01"
    const char *merge(uint32_t value, char replaceLow = 'x', char replaceHigh = 0) {
        uint8_t n = 0;
        for(char ch: BitsToStr<_Bits, _Reverse>(value)) {
            if (replaceLow && !ch) {
                _buffer[n] = replaceLow;
            }
            if (replaceHigh && ch) {
                _buffer[n] = replaceHigh;
            }
        }
        return _buffer;
    }

    const char *begin() const {
        return _buffer;
    }

    const char *end() const {
        return _buffer + _Bits;
    }

    // return internal buffer. only valid while the object exists
    const char *c_str() const {
        return _buffer;
    }

    // create string from internal buffer
    String toString() const {
        return _buffer;
    }

private:
    template<bool _Decr>
    void  _fill(const uint8_t *data, char *dst) {
        if (_Bits > 8) {
            uint8_t fullBytes = _Bits / 8;
            while(fullBytes--) {
                dst = _fill_n<_Decr, 8>(data++, dst);
            }
        }
        _fill_n<_Decr, _Bits & 0x7>(data, dst);
    }

    template<bool _Decr, uint8_t _Count>
    char *_fill_n(const uint8_t *data, char *dst) {
        uint8_t tmp = *data;
        for(uint8_t i = 0; i < _Count; i++) {
            ((_Decr) ? *(--dst) : *dst++) = (tmp & 0x1) + 48;
            tmp >>= 1;
        }
        return dst;
    }

private:
    char _buffer[_Bits + 1];
};
