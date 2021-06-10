/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "color.h"

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace Clock {

#if IOT_LED_MATRIX_CONFIGURABLE_DISPLAY
#else

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
    class PixelMapping : public Types<_StartAddress, _Rows, _Columns> {
    public:
        using TypesType = Types<_StartAddress, _Rows, _Columns>;
        using PixelAddressType = typename TypesType::PixelAddressType;
        using CoordinateType = typename TypesType::CoordinateType;
        using PixelCoordinatesType = typename TypesType::PixelCoordinatesType;
        using TypesType::kRows;
        using TypesType::kCols;
        using TypesType::kNumPixels;
        using TypesType::kStartAddress;
    protected:
        using TypesType::kTotalPixelCount;

        static constexpr uint8_t kMappingTypeId = (_ReverseRows ? 0x01 : 0x00) | (_ReverseColumns ? 0x02 : 0x00) | (_Rotate ? 0x04 : 0) | (_Interleaved ? 0x08 : 0);

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


        struct CoordinateHelperType {
            using type = CoordinateType;

            static constexpr bool kRotate = _Rotate;
            static constexpr bool kReverseRows = _ReverseRows;
            static constexpr bool kReverseColumns = _ReverseColumns;
            static constexpr int kInterleaved = _Interleaved ? 1 : 0;
        };

        // rotated

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kRotate, int>::type = 0>
        typename _Ta::type getRow(typename _Ta::type row, typename _Ta::type col) const {
            return col;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kRotate, int>::type = 0>
        typename _Ta::type getCol(typename _Ta::type row, typename _Ta::type col) const {
            return row;
        }

        // non-rotated

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kRotate, int>::type = 0>
        typename _Ta::type getRow(typename _Ta::type row, typename _Ta::type col) const {
            return row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kRotate, int>::type = 0>
        typename _Ta::type getCol(typename _Ta::type row, typename _Ta::type col) const {
            return col;
        }

        // normalize rows

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kReverseRows && !_Ta::kInterleaved, int>::type = 0>
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            return row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kReverseRows && !_Ta::kInterleaved, int>::type = 0>
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            return (kRows - 1) - row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kReverseRows && _Ta::kInterleaved, int>::type = 0>
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            if (col % 2 == _Ta::kInterleaved) {
                return row;
            }
            return (kRows - 1) - row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kReverseRows && _Ta::kInterleaved, int>::type = 0>
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            if (col % 2 == _Ta::kInterleaved) {
                return (kRows - 1) - row;
            }
            return row;
        }

        // normalize columns

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kReverseColumns, int>::type = 0>
        typename _Ta::type _col(typename _Ta::type row, typename _Ta::type col) const {
            return col;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kReverseColumns, int>::type = 0>
        typename _Ta::type _col(typename _Ta::type row, typename _Ta::type col) const {
            return (kCols - 1) - col;
        }

        // CoordinateType _row(CoordinateType row) const {
        //      return row;
        // }
        // CoordinateType _col(CoordinateType col) const {
        //      return col;
        // }
        // CoordinateType getRow(CoordinateType col, CoordinateType row) const {
        //     if (_Rotate) {
        //         std::swap(row, col);
        //     }
        //     if (_ReverseRows) {
        //         if (_Interleaved) {
        //             col++;
        //         } else {
        //             return (kRows - 1) - row;
        //         }
        //     }
        //     if (_Interleaved && (col % 2 != kInterleaved)) {
        //         return (kRows - 1) - row;
        //     }
        //     return row;
        // }
        //
        // CoordinateType getCol(CoordinateType row, CoordinateType col) const {
        //     if (_Rotate) {
        //         std::swap(row, col);
        //     }
        //     if (_ReverseColumns) {
        //         col = (kCols - 1) - col;
        //     }
        //     return col;
        // }

        PixelAddressType getAddress(CoordinateType row, CoordinateType col) const {
            // return row + col * kRows;
            return _row(getRow(row, col), getCol(row, col)) + (_col(getRow(row, col), getCol(row, col)) * kRows);
        }

        PixelAddressType getAddress(PixelCoordinatesType coords) const {
            return getAddress(coords.row(), coords.col());
        }

        PixelCoordinatesType getPoint(PixelAddressType address) const {
            CoordinateType row = address % kRows;
            CoordinateType col = address / kRows;
            return PixelCoordinatesType(_row(getRow(row, col), getCol(row, col)), _col(getRow(row, col), getCol(row, col)));
        }
    };

#endif

    template<size_t _StartAddress, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class PixelDisplayBuffer : public PixelMapping<_StartAddress, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved> {
    public:
        using PixelMappingType = PixelMapping<_StartAddress, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved>;
        using PixelMappingType::getPoint;
        using PixelMappingType::getAddress;
        using PixelMappingType::kRows;
        using PixelMappingType::kCols;
        using PixelMappingType::kNumPixels;
        using PixelMappingType::kStartAddress;
    protected:
        using PixelMappingType::kTotalPixelCount;

    public:
        using PixelAddressType = typename PixelMappingType::PixelAddressType;
        using CoordinateType = typename PixelMappingType::CoordinateType;
        using PixelCoordinatesType = typename PixelMappingType::PixelCoordinatesType;
        using PixelBufferType = std::array<CRGB, kTotalPixelCount>;
        using ColorType = CRGB;

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
            // _pixels[getAddress(row, col) + kStartAddress] = color;
            (*this)[getAddress(row, col)] = color;
        }

        void setPixel(PixelCoordinatesType point, ColorType color) {
            setPixel(point.row(), point.col(), color);
        }

        void setPixel(PixelAddressType numPixel, ColorType color) {
            (*this)[getAddress(getPoint(numPixel))] = color;
            // pixels(getAddress(getPoint(numPixel)) + kStartAddress) = color;
        }

        ColorType getPixel(CoordinateType row, CoordinateType col) const {
            return (*this)[getAddress(row, col)];
        }

        ColorType getPixel(PixelCoordinatesType point) const {
            return getPixel(point.row(), point.col());
        }

        ColorType getPixel(PixelAddressType numPixel) const {
            return (*this)[getAddress(getPoint(numPixel))];
            // return pixels(getAddress(getPoint(numPixel)) + kStartAddress);
        }

        template<typename _Ta, typename _Tb, typename std::enable_if<_Ta::kMappingTypeId == _Tb::kMappingTypeId, int>::type = 0>
        static void copy(_Ta src, _Tb dst, PixelAddressType numPixel) {
            std::copy(src.begin(), src.begin() + numPixel, dst.begin());
        }

        template<typename _Ta, typename _Tb, typename std::enable_if<_Ta::kMappingTypeId != _Tb::kMappingTypeId, int>::type = 0>
        static void copy(_Ta src, _Tb dst, PixelAddressType numPixel) {
            for (typename _Ta::PixelAddressType i = 0; i < numPixel; i++) {
                // translate the address of each pixel if the mapping is different
                dst[dst.getAddress(src.getPoint(i))] = src[i];
            }
        }

        // access to all pixels, starting with first LED
        ColorType &pixels(PixelAddressType address) {
            __DBG_assert_printf(address >= 0 && address < _pixels.size(), "address out of bounds: %d", address);
            return _pixels[address];
        }

        ColorType pixels(PixelAddressType address) const {
            __DBG_assert_printf(address >= 0 && address < _pixels.size(), "address out of bounds: %d", address);
            return _pixels[address];
        }

        // access to LEDs after the start address
        ColorType &operator [](PixelAddressType idx) {
            return pixels(kStartAddress + idx);
        }

        ColorType operator [](PixelAddressType idx) const {
            return pixels(kStartAddress + idx);
        }

        ColorType *begin() {
            return &_pixels.data()[kStartAddress];
        }

        ColorType *begin() const {
            return &_pixels.data()[kStartAddress];
        }

        ColorType *end() {
            return &_pixels.data()[_pixels.size()];
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
        using PixelBufferType::begin;
        using PixelBufferType::end;
        using PixelBufferType::operator[];

    public:
        PixelDisplay() :
            _controller(_ControllerType::addLeds(_pixels.data(), _pixels.size()))
        {
            setDither(false);
        }

        void setDither(bool enable) {
            _controller.setDither(enable ? BINARY_DITHER : DISABLE_DITHER);
            FastLED.setDither(_controller.getDither());
        }

        void reset() {
            PixelBufferType::reset();
            FastLED.show();
        }

        void clear() {
            fill(0);
        }

        void setBrightness(uint8_t brightness) {
            FastLED.setBrightness(brightness);
        }

        void show() {
            FastLED.show();
        }

        void fill(ColorType color) {
            std::fill(_pixels.begin(), _pixels.end(), color);
        }

        void delay(unsigned long ms) {
            if (can_yield() && (_controller.getDither() != DISABLE_DITHER) && (FastLED.getBrightness() != 0) && (FastLED.getBrightness() != 255)) {
                FastLED.delay(ms);
            }
            else {
                ::delay(ms);
            }

        }

    private:
        CLEDController &_controller;
    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
