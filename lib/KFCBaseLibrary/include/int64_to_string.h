/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino.h>
#include <String.h>

size_t concat_to_string(String &str, const int64_t value);
size_t concat_to_string(String &str, const uint64_t value);

size_t print_string(Print &output, const int64_t value);
size_t print_string(Print &output, const uint64_t value);

String to_string(const int64_t value);
String to_string(const uint64_t value);

// buffer must be at least 22 byte
char *ulltoa(unsigned long long value, char *buffer);
char *lltoa(long long value, char *buffer);

// Example code:
//
//Serial.println("--------------------");
//Serial.println("to_string");
//Serial.println("--------------------");
//int64_t iii1 = INT64_MIN;
//Serial.println(to_string(iii1));
//uint64_t iii2 = UINT64_MAX;
//Serial.println(to_string(iii2));
//Serial.println("--------------------");
//Serial.println("concat_to_string");
//Serial.println("--------------------");
//String out;
//concat_to_string(out, iii1); out += '\n';
//concat_to_string(out, iii2); out += '\n';
//Serial.print(out);
//Serial.println("--------------------");
//Serial.println("[u]lltoa");
//Serial.println("--------------------");
//char buffer[22];
//Serial.println(lltoa(iii1, buffer));
//Serial.println(ulltoa(iii2, buffer));
//Serial.println("--------------------");
// --------------------
// to_string
// --------------------
// -9223372036854775808
// 18446744073709551615
// --------------------
// concat_to_string
// --------------------
// -9223372036854775808
// 18446744073709551615
// --------------------
// [u]lltoa
// --------------------
// -9223372036854775808
// 18446744073709551615
// --------------------


// from microsoft STL
namespace Integral_to_string {

    template <class _UTy>
    char *_UIntegral_to_buff(char *_RNext, _UTy _UVal) { // format _UVal into buffer *ending at* _RNext
        constexpr bool _Big_uty = sizeof(_UTy) > 4;
        if (_Big_uty) { // For 64-bit numbers, work in chunks to avoid 64-bit divisions.
            while (_UVal > 0xFFFFFFFFU) {
                auto _UVal_chunk = static_cast<unsigned long>(_UVal % 1000000000);
                _UVal /= 1000000000;
                for (int _Idx = 0; _Idx != 9; ++_Idx) {
                    *--_RNext = static_cast<char>('0' + _UVal_chunk % 10);
                    _UVal_chunk /= 10;
                }
            }
        }
        auto _UVal_trunc = static_cast<unsigned long>(_UVal);
        do {
            *--_RNext = static_cast<char>('0' + _UVal_trunc % 10);
            _UVal_trunc /= 10;
        } while (_UVal_trunc != 0);
        return _RNext;
    }

    template<class _Ty, class _UTy>
    size_t _Integral_to_string(const _Ty _Val, char *buffer) {
        char *endPtr = buffer + 21;
        char *_RNext = endPtr;
        const auto _UVal = static_cast<_UTy>(_Val);
        if (_Val < 0) {
            _RNext = _UIntegral_to_buff(_RNext, 0 - _UVal);
            *--_RNext = '-';
        }
        else {
            _RNext = _UIntegral_to_buff(_RNext, _UVal);
        }
        return endPtr - _RNext;
    }

}
