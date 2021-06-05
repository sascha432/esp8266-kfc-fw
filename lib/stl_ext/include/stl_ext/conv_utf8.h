/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>
#include <pgmspace.h>
#include <iterator>
#include "stl_ext/array.h"
#include "stl_ext/utility.h"

#if _MSC_VER
#define STL_STD_EXT_CONV_UTF8_ASSERT 1
#else
#define STL_STD_EXT_CONV_UTF8_ASSERT 0
#endif

#if STL_STD_EXT_CONV_UTF8_ASSERT
#include <assert.h>
#endif

#pragma push_macro("_ASSERTE")
#undef _ASSERTE

#if STL_STD_EXT_CONV_UTF8_ASSERT
#define _ASSERTE(...) assert(__VA_ARGS__)
#else
#define _ASSERTE(...)
#endif

#ifndef _MSC_VER
#include <push_optimize.h>
#pragma GCC optimize ("O2")
#endif

//
// examples
//
//const char *test1 = "100€ 5" "\xb0" "C 23°C";
//constexpr size_t dst_len_init = 256;
//size_t dst_len = dst_len_init;
//char dst[dst_len_init];
//auto len = strlen(test1);
//
//using namespace stdex::conv;
//
//std::string str;
//
//str += "23";
//Unicode::copyUTF8<0x00b0>(str);
//Unicode::copyUTF8<'C'>(str);
//
//Serial.printf(str.c_str());
//Serial.println();
//
//for (const auto byte : utf8::ReplaceInvalidEncodingUnicodeSymbol<0xfffd>()) {
//    Serial.printf("0x%02x ", byte);
//}
//Serial.println();
//
//auto tmp = utf8::strcpy<utf8::DefaultReplacement>(dst, test1, dst_len, len);
//Serial.printf("len=%u size=%u str=%s\n", tmp, dst_len, dst);
//
//dst_len = dst_len_init;
//tmp = utf8::strlen<utf8::DefaultReplacement>(test1, dst_len, len);
//Serial.printf("strlen=%u\n", tmp);
//

namespace STL_STD_EXT_NAMESPACE_EX {

    namespace conv {

        struct Array {

            //
            // convert uint32 to array of 0 - 4 bytes
            //
            template<uint32_t _Value, size_t length>
            struct uint32_to_bytes {
            };

            template<uint32_t _Value>
            struct uint32_to_bytes<_Value, 4> {
                //static constexpr const uint8_t value[4] = {
                //    static_cast<uint8_t>(_Value),
                //    static_cast<uint8_t>(_Value >> 8),
                //    static_cast<uint8_t>(_Value >> 16),
                //    static_cast<uint8_t>(_Value >> 24)
                //};
                inline static constexpr auto getArray() -> std::array<const uint8_t, 4> {
                    return std::array_of<const uint8_t>(static_cast<uint8_t>(_Value), static_cast<uint8_t>(_Value >> 8), static_cast<uint8_t>(_Value >> 16), static_cast<uint8_t>(_Value >> 24));
                }
            };

            template<uint32_t _Value>
            struct uint32_to_bytes<_Value, 3> {
                //static constexpr const uint8_t value[3] = {
                //    static_cast<uint8_t>(_Value),
                //    static_cast<uint8_t>(_Value >> 8),
                //    static_cast<uint8_t>(_Value >> 16)
                //};
                inline static constexpr auto getArray() -> std::array<const uint8_t, 3> {
                    return std::array_of<const uint8_t>(static_cast<uint8_t>(_Value), static_cast<uint8_t>(_Value >> 8), static_cast<uint8_t>(_Value >> 16));
                }
            };

            template<uint32_t _Value>
            struct uint32_to_bytes<_Value, 2> {
                //static constexpr const uint8_t value[2] = {
                //    static_cast<uint8_t>(_Value),
                //    static_cast<uint8_t>(_Value >> 8)
                //};
                inline static constexpr auto getArray()->std::array<const uint8_t, 2> {
                    return std::array_of<const uint8_t>(static_cast<const uint8_t>(_Value), static_cast<const uint8_t>(_Value >> 8));
                }
            };

            template<uint32_t _Value>
            struct uint32_to_bytes<_Value, 1> {
                //static constexpr const uint8_t value[1] = {
                //    static_cast<uint8_t>(_Value)
                //};
                inline static constexpr auto getArray() -> std::array<const uint8_t, 1> {
                    return std::array_of<const uint8_t>(static_cast<const uint8_t>(_Value));
                }
            };

            template<uint32_t _Value>
            struct uint32_to_bytes<_Value, 0> {
                //static constexpr const uint8_t value[1] = {};
                inline static constexpr auto getArray() -> std::array<const uint8_t, 0> {
                    return {};
                }
            };

        };

        struct Unicode {

            struct __utf8_helper {

                inline static constexpr size_t getLengthFromUnicode(const uint32_t value) {
                    return (value <= 0x7f ? (1) : (value <= 0x7ff ? (2) : (value <= 0xffff ? (3) : ((value <= 0x10ffff ? (4) : (0))))));
                }

                inline static constexpr uint32_t encodeValueByLength(const uint32_t value, size_t length) {
                    return length == 1 ?
                        (value & 0x7f) :
                        (length == 2 ?
                            (((value >> 6 & 0x1f) | 0xc0)) | (((value & 0x3f) | 0x80) << 8) :
                            (length == 3 ?
                                (((value >> 12) & 0x0f) | 0xe0) | ((((value >> 6) & 0x3f) | 0x80) << 8) | ((((value & 0x3f)) | 0x80) << 16) :
                                (length == 4 ?
                                    (((value >> 18) & 0x07) | 0xf0) | ((((value >> 12) & 0x3f) | 0x80) << 16) | ((((value >> 6) & 0x3f) | 0x80) << 16) | (((value & 0x3f) | 0x80) << 16) :
                                    (0)
                                    )
                                )
                            );
                }

                template<uint32_t _UnicodeValue, size_t N = getLengthFromUnicode(_UnicodeValue)>
                inline static constexpr auto encodeUnicodeAsArray()->std::array<const uint8_t, N> {
                    return Array::uint32_to_bytes<encodeValueByLength(_UnicodeValue, N), N>::getArray();
                }

            };

            template<uint32_t _Unicode>
            struct toUTF8 {

                static constexpr uint32_t unicode_value = _Unicode;
                static constexpr auto value = Unicode::__utf8_helper::encodeUnicodeAsArray<unicode_value>();

                inline static constexpr size_t size() {
                    return value.size();
                }

                inline static constexpr const uint8_t *begin() {
                    return &(*value.begin());
                    //return &value.front();
                }

                inline static constexpr const uint8_t *end() {
                    return &(*std::prev(value.end())) + 2;
                    //return &value.back() + 1;
                }

            };

            template<uint32_t _Unicode, typename _Container>
            inline static void copyUTF8(_Container &container) {
                std::copy(toUTF8<_Unicode>::begin(), toUTF8<_Unicode>::end(), std::back_inserter(container));
            }

        };

        //template<typename _UnicodeValue>
        //constexpr std::array<const uint8_t, Unicode::__utf8_helper::getLengthFromUnicode(_UnicodeValue)> Unicode::toUTF8<_UnicodeValue>::value;

        // template<>
        // constexpr std::array<const uint8_t, 3> Unicode::toUTF8<0xfffd>::value;


        struct utf8 {

            struct RemoveInvalidEncoding : Unicode::toUTF8 <~0U> {
            };

            template<uint32_t _Unicode>
            struct ReplaceInvalidEncodingUnicodeSymbol : Unicode::toUTF8<_Unicode> {
                static_assert(_Unicode >= 0x20 && _Unicode < 0xffff, "Unicode is limit to 0x0020 - 0xffff");
            };

            template<uint32_t _Ascii>
            struct ReplaceInvalidEncodingAsciiSymbol : Unicode::toUTF8<_Ascii> {
                static_assert(_Ascii >= 0x20 && _Ascii < 0x80, "ASCII symbols are limited to 0x20 - 0x7f");
            };

            using QuestionMarkReplacement = Unicode::toUTF8<'?'>;
            // using DefaultReplacement = Unicode::toUTF8<0xfffd>;

            struct DefaultReplacement {
            //    static constexpr auto value = Unicode::toUTF8<0xfffd>::value;
               static constexpr std::array<const uint8_t, 3U> value = {{(unsigned char)239U, (unsigned char)191U, (unsigned char)189U}};
               static constexpr size_t size() {
                   return 3;
               }
            };

            //
            // helper returns
            // 0            for success, src/dst have been moved to the next character
            // not 0        for invalid character or end of dst/src reached
            //              src points to the next character, dst is unchanged

            //struct __strcpy_helper {

            //    //
            //    // append 0 byte
            //    //
            //    inline static uint8_t *func(uint8_t *dst)
            //    {
            //        *dst = 0;
            //        return dst;
            //    }

            //    //
            //    // copy single byte
            //    //
            //    inline static int func(uint8_t **dst, uint8_t *dst_end, const uint8_t **src, const uint8_t *src_end)
            //    {
            //        //return func(dst, dst_end, src, src_end, (uint8_t)len);
            //        _ASSERTE(*dst < dst_end);
            //        _ASSERTE(*src < src_end);
            //        *(*dst)++ = *(*src)++;
            //        return 0;
            //    }

            //    //
            //    // validate utf8 sequence and copy valid data
            //    //
            //    inline static int func(uint8_t **dst, uint8_t *dst_end, const uint8_t **src, const uint8_t *src_end, uint8_t len)
            //    {
            //        _ASSERTE(*dst < dst_end);
            //        _ASSERTE(*src < src_end);
            //        auto in = *src + 1;
            //        auto out = *dst + 1;
            //        while (--len && out < dst_end && in < src_end && (*in & 0xc0) == 0x80) {
            //            in++;
            //            out++;
            //        }
            //        if (len == 0) {
            //            // copy validated sequence
            //            while (*src < in) {
            //                *(*dst)++ = *(*src)++;
            //            }
            //            return 0;
            //        }
            //        else {
            //            // point to the next character after the invalid sequence
            //            *src = in + len;
            //        }
            //        return len;
            //    }

            //};

            struct __strcpy_helper {

                //
                // append 0 byte
                //
                template<typename _Iter>
                inline static _Iter end(_Iter dst)
                {
                    *dst = 0;
                    return dst;
                }

                //
                // copy single byte
                //
                template <typename _Iter1, typename _Iter2>
                inline static int copy_one(_Iter1 &dst, _Iter1 dst_end, _Iter2 &src, _Iter2 src_end)
                {
                    //return func(dst, dst_end, src, src_end, (uint8_t)len);
                    _ASSERTE(dst < dst_end);
                    _ASSERTE(src < src_end);
                    *dst = *src;
                    ++dst;
                    ++src;
                    return 0;
                }

                 //
                // validate utf8 sequence and copy valid data
                //
                template <typename _Iter1, typename _Iter2>
                inline static int copy(_Iter1 &dst, _Iter1 dst_end, _Iter2 &src, _Iter2 src_end, uint8_t len)
                {
                    _ASSERTE(dst < dst_end);
                    _ASSERTE(src < src_end);
                    auto in = src + 1;
                    auto out = dst + 1;
                    while (--len && out < dst_end && in < src_end && (static_cast<uint8_t>(*in) & 0xc0) == 0x80) {
                        ++in;
                        ++out;
                    }
                    if (len == 0) {
                        // copy validated sequence
                        while (src < in) {
                            *dst = *src;
                            ++dst;
                            ++src;
                        }
                        return 0;
                    }
                    else {
                        // point to the next character after the invalid sequence
                        src = in + len;
                    }
                    return len;
                }
            };

            struct __strlen_helper {

                //
                // append 0 byte
                //
                template<typename _Iter>
                inline static _Iter end(_Iter dst)
                {
                    return dst;
                }

                //
                // copy single byte
                //
                template <typename _Iter1, typename _Iter2>
                inline static int copy_one(_Iter1 &dst, _Iter1 dst_end, _Iter2 &src, _Iter2 src_end)
                {
                    _ASSERTE(dst < dst_end);
                    _ASSERTE(src < src_end);
                    ++dst;
                    ++src;
                    return 0;
                }

                //
               // validate utf8 sequence and copy valid data
               //
                template <typename _Iter1, typename _Iter2>
                inline static int copy(_Iter1 &dst, _Iter1 dst_end, _Iter2 &src, _Iter2 src_end, uint8_t len)
                {
                    _ASSERTE(dst < dst_end);
                    _ASSERTE(src < src_end);
                    auto in = src + 1;
                    auto out = dst + 1;
                    while (--len && out < dst_end && in < src_end && (static_cast<uint8_t>(*in) & 0xc0) == 0x80) {
                        ++in;
                        ++out;
                    }
                    if (len == 0) {
                        src = in;
                        dst = out;
                        return 0;
                    }
                    else {
                        // point to the next character after the invalid sequence
                        src = in + len;
                    }
                    return len;
                }
            };


             // template for reading, validating and copying utf8 strings
            //
            // dst              destination buffer
            // dst_len          max. buffer length in bytes, required to be greater than zero
            // src              source string
            // src_len          length in bytes without terminating 0 byte == strlen(src)
            //                  required length to fit src is strlen(src) + 1
            // _SymbolType      type that represents the symbol for invalid utf8 sequences
            //                  stdex::conv::utf8::DefaultReplacement = �
            //                  stdex::conv::utf8::QuestionMarkReplacement = ?
            //                  stdex::conv::utf8::RemoveInvalidEncoding
            //                  stdex::conv::utf8::ReplaceInvalidEncodingAsciiSymbol<'.'> = .
            //                  stdex::conv::utf8::ReplaceInvalidEncodingUnicodeSymbol<0x00bf> = ¿
            //
            // _Fn              type with statics methods to read/write data
            //
            // returns          length of utf8 characters written
            //                  dst_len is set to the number of bytes written to dst == strlen(dst)
            //                  without the trailing 0 byte
            //
            //template <typename _SymbolType, typename _Fn>
            //static size_t __validate_utf8_encoding(char *dst, const char *str, size_t &dst_len, size_t src_len)
            //{
            //    _ASSERTE(dst_len);
            //    auto src_begin = (const uint8_t *)str;
            //    auto src_end = src_begin + src_len;
            //    auto dst_begin = (uint8_t *)dst;
            //    auto dst_end = dst_begin + dst_len - 1;
            //    size_t out_len = 0;

            //    while (src_begin < src_end && dst_begin < dst_end) {
            //        int result;
            //        if (*src_begin < 0x80) {
            //            result = _Fn::func(&dst_begin, dst_end, &src_begin, src_end);
            //        }
            //        else {
            //            uint8_t code1 = *src_begin >> 3;
            //            uint8_t advance;
            //            if (code1 == 0x1e) {
            //                advance = 4;
            //            }
            //            else if ((code1 = (code1 >> 1)) == 0xe) {
            //                advance = 3;
            //            }
            //            else if ((code1 = (code1 >> 1)) == 0x6) {
            //                advance = 2;
            //            }
            //            else {
            //                advance = 1;
            //                result = 1;
            //                src_begin++;
            //            }
            //            if (advance > 1) {
            //                result = _Fn::func(&dst_begin, dst_end, &src_begin, src_end, advance);
            //                if (result == 0) {
            //                    out_len++;
            //                    continue;
            //                }
            //            }

            //            if (_SymbolType::size()) {
            //                // on failure, try to fit in the invalid symbol character
            //                if (!(dst_begin + _SymbolType::size() <= dst_end)) {
            //                    break;
            //                }

            //                auto tmpPtr = &_SymbolType::value.front();
            //                result = _Fn::func(&dst_begin, dst_end, &tmpPtr, &_SymbolType::value.back() + 2, (uint8_t)_SymbolType::size());
            //            }
            //        }

            //        if (result == 0) {
            //            out_len++;
            //            continue;
            //        }
            //        if (result == -1) {
            //            break;
            //        }

            //    }

            //    dst_len = _Fn::func(dst_begin) - (uint8_t *)dst;
            //    return out_len;
            //}

            template <typename _SymbolType, typename _Fn, typename _Iter1, typename _Iter2>
            static size_t __validate_utf8_encoding(_Iter1 dst_begin, _Iter1 &_dst_end, const _Iter2 &_src_begin, _Iter2 src_end)
            {
                auto src_begin = _src_begin;
                auto dst_end = std::prev(_dst_end);
                size_t out_len = 0;

                while (src_begin < src_end && dst_begin < dst_end) {
                    int result;
                    auto code1 = static_cast<uint8_t>(*src_begin);
                    if (code1 < 0x80) {
                        result = _Fn::copy_one(dst_begin, dst_end, src_begin, src_end);
                    }
                    else {
                        uint8_t advance;
                        code1 >>= 3;
                        if (code1 == 0x1e) {
                            advance = 4;
                        }
                        else if ((code1 = (code1 >> 1)) == 0xe) {
                            advance = 3;
                        }
                        else if ((code1 = (code1 >> 1)) == 0x6) {
                            advance = 2;
                        }
                        else {
                            ++src_begin;
                            if (_SymbolType::size()) {
                                result = 1;
                                goto checkSymbol;
                            }
                            continue;
                        }
                        result = _Fn::copy(dst_begin, dst_end, src_begin, src_end, advance);
                        if (result == 0) {
                            out_len++;
                            continue;
                        }

 /*                       if (code1 == 0x1e) {
                            advance = 4;
                        }
                        else if ((code1 = (code1 >> 1)) == 0xe) {
                            advance = 3;
                        }
                        else if ((code1 = (code1 >> 1)) == 0x6) {
                            advance = 2;
                        }
                        else {
                            advance = 1;
                            result = 1;
                            ++src_begin;
                        }
                        if (advance > 1) {
                            result = _Fn::copy(dst_begin, dst_end, src_begin, src_end, advance);
                            if (result == 0) {
                                out_len++;
                                continue;
                            }
                        }*/

                        checkSymbol:
                        if (_SymbolType::size()) {
                            // on failure, try to fit in the invalid symbol
                            if (!(dst_begin + _SymbolType::size() <= dst_end)) {
                                break;
                            }

#if __GNUC__
                            static constexpr auto value = _SymbolType::value;
                            auto symbol_begin = &value.front();
                            result = _Fn::copy(dst_begin, dst_end, symbol_begin, &value.back() + 1, (uint8_t)_SymbolType::size());
#else
                            auto symbol_begin = _SymbolType::begin();
                            result = _Fn::copy(dst_begin, dst_end, symbol_begin, _SymbolType::end(), (uint8_t)_SymbolType::size());
#endif
                        }
                    }

                    if (result == 0) {
                        out_len++;
                        continue;
                    }
                    if (result == -1) {
                        break;
                    }

                }

                _dst_end = _Fn::end(dst_begin);
                return out_len;
            }

            //template<typename _SymbolType>
            //inline static size_t validate(char *dst, const char *str, size_t &dst_len, size_t src_len)
            //{
            //    return __validate_utf8_encoding<_SymbolType, __validate_helper>(dst, str, dst_len, src_len);
            //}

            //template<typename _SymbolType>
            //inline static size_t strcpy(char *dst, const char *str, size_t &dst_len, size_t src_len)
            //{
            //    return __validate_utf8_encoding<_SymbolType, __strcpy_helper>(dst, str, dst_len, src_len);
            //}

            template<typename _SymbolType>
            inline static size_t strcpy(char *dst, const char *str, size_t &dst_len, size_t src_len)
            {
                auto endPtr = dst + dst_len + 1;
                auto result = __validate_utf8_encoding<_SymbolType, __strcpy_helper>(dst, endPtr, str, str + src_len + 1);
                dst_len = std::distance(dst, endPtr);
                return result;
            }

            // returns number of utf8 characters
            //
            // dst_len is the destination buffer used for copying
            //
            template<typename _SymbolType>
            inline static size_t strlen(const char *str, size_t &dst_len, size_t src_len)
            {
                auto dst_begin = const_cast<char *>(str);
                auto dst_end = dst_begin + dst_len + 1;
                auto result = __validate_utf8_encoding<_SymbolType, __strlen_helper>(dst_begin, dst_end, str, str + src_len + 1);
                dst_len = std::distance(dst_begin, dst_end);
                return result;
            }

            template<typename _SymbolType>
            inline static size_t strlen(const char *str, size_t src_len) {
                size_t dst_len = ~0 >> 2;
                return strlen<_SymbolType>(str, dst_len, src_len + 1);
            }

        };

    }

}

#ifndef _MSC_VER
#include <pop_optimize.h>
#endif


#if STL_STD_EXT_CONV_UTF8_ASSERT
#pragma pop_macro("_ASSERTE")
#endif
