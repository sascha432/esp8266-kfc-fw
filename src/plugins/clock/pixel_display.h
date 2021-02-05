/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "color.h"

#if defined(ESP8266)
#ifndef FASTLED_ESP8266_RAW_PIN_ORDER
#define FASTLED_ESP8266_RAW_PIN_ORDER                           1
#endif
#endif

// #ifndef FASTLED_ALLOW_INTERRUPTS
// #define FASTLED_ALLOW_INTERRUPTS                                0
// #endif
// #ifndef FASTLED_INTERRUPT_RETRY_COUNT
// #define FASTLED_INTERRUPT_RETRY_COUNT                           1
// #endif
#define FASTLED_INTERNAL
#include <FastLED.h>


#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace Clock {

    // template<size_t _StartAddress, size_t _Rows, size_t _Columns>
    // class Types {
    // private:
    //     static constexpr size_t kTotalPixelCount = _Rows * _Colums + _StartAddress;
    //     static constexpr size_t kCols = _Colums;
    //     static constexpr size_t kRows = _Rows;

    // public:
    //     using PixelAddressType = typename std::conditional<(kTotalPixelCount > 255), uint16_t, uint8_t>::type;
    //     using CoordinateType = typename std::conditional<(kRows > 255 || kCols > 255), uint16_t, uint8_t>::type;
    //     using PixelCoordinatesType = PixelCoordinates<CoordinateType, _Rows, _Columns>;
    // };

    template<class _CoordinateType, _CoordinateType _Rows, _CoordinateType _Columns>
    class PixelCoordinates {
    public:
        using CoordinateType = _CoordinateType;

        static constexpr CoordinateType kRows = _Rows;
        static constexpr CoordinateType kCols = _Columns;

    public:
        PixelCoordinates(CoordinateType row = 0, CoordinateType col = 0) : _row(row), _col(col) {}

        CoordinateType row() const {
            return _row;
        }
        CoordinateType &row() {
            return _row;
        }
        CoordinateType col() const {
            return _col;
        }
        CoordinateType &col() {
            return _col;
        }

        void invertColumn() {
            _col = (kCols - 1) - _col;
        }
        void invertRow() {
            _row = (kRows - 1) - _row;
        }

        void rotate() {
            std::swap(_row, _col);
        }

    private:
        CoordinateType _row;
        CoordinateType _col;
    };

    // row:col=address (4x3)
    // -> rows
    // 0:0=00 1:0=01 2:0=02 3:0=03
    // 0:1=04 1:1=05 2:1=06 3:1=07
    // 0:2=08 1:2=09 2:2=10 3:2=11
    //
    // <- rows (_ReverseRows = true)
    // 3:0=00 2:0=01 1:0=02 0:0=03
    // 3:1=04 2:1=05 1:1=06 0:1=07
    // 3:2=08 2:2=09 1:2=10 0:2=11

    template<size_t _StartAddress, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class PixelMapping {
    public:
        static constexpr size_t kTotalPixelCount = (_Rows * _Columns) + _StartAddress;
        static constexpr size_t kStartAddress = _StartAddress;
        static constexpr int kInterleaved = _ReverseRows ? 1 : 0;

        using PixelAddressType = typename std::conditional<(kTotalPixelCount > 255), uint16_t, uint8_t>::type;
        using CoordinateType = typename std::conditional<(_Rows > 255 || _Columns > 255), uint16_t, uint8_t>::type;

        static constexpr CoordinateType kRows = _Rows;
        static constexpr CoordinateType kCols = _Columns;
        static constexpr uint8_t kMappingTypeId = (_ReverseRows ? 0x01 : 0x00) | (_ReverseColumns ? 0x02 : 0x00) | (_Rotate ? 0x04 : 0) | (_Interleaved ? 0x08 : 0);

        using PixelCoordinatesType = PixelCoordinates<CoordinateType, kRows, kCols>;

        // non-rotated

        // template<class T, typename = std::enable_if<_ReverseRows == false && _Rotate == false && _Interleaved == false>>
        // CoordinateType applyRowDirection(T row, T col) const {
        //     return row;
        // }

        // template<typename T, typename = std::enable_if<_ReverseRows == true && _Rotate == false && _Interleaved == false>>
        // CoordinateType applyRowDirection(T row, T col) const {
        //     return (kRows - 1) - row;
        // }

        // template<typename T, typename = std::enable_if<_Rotate == false && _Interleaved == true>>
        // CoordinateType applyRowDirection(T row, T col) const {
        //     if (col % 2 == kInterleaved) {
        //         return row;
        //     }
        //     else {
        //         return (kRows - 1) - row;
        //     }
        // }

        // // rotated

        // template<typename T, typename = std::enable_if<_ReverseRows == false && _Rotate == true && _Interleaved == false>>
        // CoordinateType applyRowDirection(T col, T row) const {
        //     return row;
        // }

        // template<typename T, typename = std::enable_if<_ReverseRows == true && _Rotate == true && _Interleaved == false>>
        // CoordinateType applyRowDirection(T col, T row) const {
        //     return (kRows - 1) - row;
        // }

        // template<typename T, typename = std::enable_if<_Rotate == true && _Interleaved == true>>>
        // CoordinateType applyRowDirection(T col, T row) const {
        //     if (col % 2 == kInterleaved) {
        //         return row;
        //     }
        //     else {
        //         return (kRows - 1) - row;
        //     }
        // }


        CoordinateType applyRowDirection(CoordinateType col, CoordinateType row) const {
            if (_Rotate) {
                std::swap(row, col);
            }
            if (_ReverseRows) {
                if (_Interleaved) {
                    col++;
                } else {
                    return (kRows - 1) - row;
                }
            }
            if (_Interleaved && (col % 2 != kInterleaved)) {
                return (kRows - 1) - row;
            }
            return row;
        }

        CoordinateType applyColDirection(CoordinateType col, CoordinateType row) const {
            if (_Rotate) {
                std::swap(row, col);
            }
            if (_ReverseColumns) {
                col = (kCols - 1) - col;
            }
            return col;
        }

        PixelAddressType getAddress(CoordinateType row, CoordinateType col) const {
            // return row + col * kRows;
            return applyRowDirection(row, col) + (applyColDirection(row, col) * kRows);
        }

        PixelAddressType getAddress(PixelCoordinatesType coords) const {
            return getAddress(coords.row(), coords.col());
        }

        PixelCoordinatesType getPoint(PixelAddressType address) const {
            CoordinateType row = address % kRows;
            CoordinateType col = address / kRows;
            return PixelCoordinatesType(
                applyRowDirection(row, col),
                applyColDirection(row, col)
            );
            // return PixelCoordinatesType(row, col); //applyRowDirection(row, col), applyColDirection(row, col));
        }
    };

    template<size_t _StartAddress, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class PixelDisplayBuffer : public PixelMapping<_StartAddress, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved> {
    public:
        using PixelMappingType = PixelMapping<_StartAddress, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved>;
        using PixelMappingType::getPoint;
        using PixelMappingType::getAddress;

    private:
        static constexpr size_t kTotalPixelCount = (_Rows * _Columns) + PixelMappingType::kStartAddress;

    public:
        using PixelAddressType = typename PixelMappingType::PixelAddressType;
        using CoordinateType = typename PixelMappingType::CoordinateType;
        using PixelCoordinatesType = typename PixelMappingType::PixelCoordinatesType;
        using PixelBufferType = std::array<CRGB, kTotalPixelCount>;
        using ColorType = CRGB;

        static constexpr CoordinateType kRows = PixelMappingType::kRows;
        static constexpr CoordinateType kCols = PixelMappingType::kCols;
        static constexpr PixelAddressType kNumPixels = kRows * kCols;
        static constexpr CoordinateType kStartAddress = PixelMappingType::kStartAddress;

    public:
        void reset() {
            _pixels.fill(0);
        }

        void clear() {
            std::fill(_pixels.begin() + kStartAddress, _pixels.end(), 0);
        }

        void fill(ColorType color) {
            std::fill(_pixels.begin() + kStartAddress, _pixels.end(), color);
        }

        void setPixel(CoordinateType row, CoordinateType col, ColorType color) {
            __DBG_assert_printf(getAddress(row, col) + kStartAddress >= 0 && getAddress(row, col) + kStartAddress < kTotalPixelCount,
                "address out of bounds: %d for %d:%d", getAddress(row, col) + kStartAddress, row, col);
            _pixels[getAddress(row, col) + kStartAddress] = color;
        }

        void setPixel(PixelCoordinatesType point, ColorType color) {
            setPixel(point.row(), point.col(), color);
        }

        void setPixel(PixelAddressType numPixel, ColorType color) {
            pixels(getAddress(getPoint(numPixel))) = color;
        }

        ColorType getPixel(CoordinateType row, CoordinateType col) const {
            __DBG_assert_printf(getAddress(row, col) + kStartAddress >= 0 && getAddress(row, col) + kStartAddress < kTotalPixelCount,
                "address out of bounds: %d for %d:%d", getAddress(row, col) + kStartAddress, row, col);
            return _pixels[getAddress(row, col) + kStartAddress];
        }

        ColorType getPixel(PixelCoordinatesType point) const {
            return getPixel(point.row(), point.col());
        }

        ColorType getPixel(PixelAddressType numPixel) const {
            return pixels(getAddress(getPoint(numPixel)));
        }

        ColorType &pixels(PixelAddressType address) {
            __DBG_assert_printf(address >= 0 && address < _pixels.size(), "address out of bounds: %d", address);
            return _pixels[address];
        }

        ColorType pixels(PixelAddressType address) const {
            __DBG_assert_printf(address >= 0 && address < _pixels.size(), "address out of bounds: %d", address);
            return _pixels[address];
        }

        template<typename _Ta, typename _Tb, typename std::enable_if<_Ta::kMappingTypeId == _Tb::kMappingTypeId, int>::type = 0>
        static void copy(_Ta src, _Tb dst) {
            static_assert(_Ta::kNumPixels == _Tb::kNumPixels, "number of pixels does not match");
            std::copy(src._pixels.begin() + src.kStartAddress, src._pixels.end(), dst._pixels.begin() + dst.kStartAddress);
        }

        template<typename _Ta, typename _Tb, typename std::enable_if<_Ta::kMappingTypeId != _Tb::kMappingTypeId, int>::type = 0>
        static void copy(_Ta src, _Tb dst) {
            static_assert(_Ta::kNumPixels == _Tb::kNumPixels, "number of pixels does not match");
            for (typename _Ta::PixelAddressType i = 0; i < _Ta::kNumPixels; i++) {
                dst._pixels[dst.getAddress(src.getPoint(i)) + dst.kStartAddress] = src._pixels[i + src.kStartAddress];
            }
        }

        // void copy(_Ta src, _Tb dst) {
        //     static_assert(_Ta::kNumPixels == _Tb::kNumPixels, "number of pixels does not match");
        //     // TODO change to enable_if
        //     if (_Ta::kMappingTypeId == _Tb::kMappingTypeId) {
        //         std::copy(src._pixels.begin() + src.kStartAddress, src._pixels.end(), dst._pixels.begin() + dst.kStartAddress);
        //     }
        //     else {
        //         for (_Ta::PixelAddressType i = 0; i < _Ta::kNumPixels; i++) {
        //             dst.setPixel(i)
        //             src.getPixel(i)
        //         }
        //     }
        // }

        ColorType *begin() {
            return &_pixels.data()[kStartAddress];
        }

        ColorType *begin() const {
            return &_pixels.data()[kStartAddress];
        }

        ColorType *end() const {
            return &_pixels.data()[_pixels.size()];
        }

    protected:
        PixelBufferType _pixels;
    };

    template<uint8_t _OutputPin>
    class NeoPixelController {
    public:
        static CLEDController &addLeds(CRGB *data, int sizeOrOffset, int size = 0) {
            return FastLED.addLeds<NEOPIXEL, _OutputPin>(data, sizeOrOffset, size);
        }
    };

    template<class _ControllerType, class _PixelDisplayBufferType>
    class PixelDisplay : public _PixelDisplayBufferType
    {
    public:
        using PixelBufferType = _PixelDisplayBufferType;
        using PixelBufferType::_pixels;

    public:
        PixelDisplay() :
            _controller(_ControllerType::addLeds(_pixels.data(), _pixels.size()))
        {
            _controller.setDither(DISABLE_DITHER);
        }

        void setDither(bool enable) {
            _controller.setDither(enable ? BINARY_DITHER : DISABLE_DITHER);
        }

        void reset() {
            PixelBufferType::reset();
            FastLED.show();
        }

        void clear() {
            FastLED.clear(true);
        }

        void setBrightness(uint8_t brightness) {
            FastLED.setBrightness(brightness);
        }

        void show() {
            FastLED.show();
        }

        void show(uint8_t brightness) {
            FastLED.show(brightness);
        }

    private:
        CLEDController &_controller;
    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
