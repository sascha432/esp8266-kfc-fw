/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "color.h"
#include <NeoPixelEx.h>

extern "C" uint8_t getNeopixelShowMethodInt();

#if DEBUG_IOT_CLOCK
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// class to address LED matrix displays by x and y coordinates mapping it to its real coordinates
// the mapping is done during compilation and fixed for best performance
//
// the required memory is total pixels (including pixels that are not display, see _PixelOffset) * 3 byte and 32bit padded, plus an additonal pointer
// 118 pixel * 3 / 4 = 88.5 = with padding 89 * 4 + 4 byte = 360 byte

namespace Clock {

    enum class ShowMethodType : uint8_t {
        NONE = 0,
        FASTLED,
        NEOPIXEL,
        MAX
    };

#if IOT_LED_MATRIX_CONFIGURABLE_DISPLAY

    #error not supported yet

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

    // pixels are mapped with a zero offset
    // _PixelOffset is used to determine the type of the pixel and the storage size
    // unused pixels still need to be allocated and refreshed

    template<size_t _PixelOffset, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class PixelMapping : public Types<_PixelOffset, _Rows, _Columns> {
    public:
        using TypesType = Types<_PixelOffset, _Rows, _Columns>;
        using PixelAddressType = typename TypesType::PixelAddressType;
        using CoordinateType = typename TypesType::CoordinateType;
        using PixelCoordinatesType = typename TypesType::PixelCoordinatesType;
        using TypesType::kRows;
        using TypesType::kCols;
        using TypesType::kNumPixels;
    protected:
        using TypesType::kPixelOffset;
        using TypesType::kMaxPixelAddress; // kPixelOffset + kNumPixels

        static constexpr uint8_t kMappingTypeId = (_ReverseRows ? 0x01 : 0x00) | (_ReverseColumns ? 0x02 : 0x00) | (_Rotate ? 0x04 : 0) | (_Interleaved ? 0x08 : 0);

        struct CoordinateHelperType {
            using type = CoordinateType;

            static constexpr bool kRotate = _Rotate;
            static constexpr bool kReverseRows = _ReverseRows;
            static constexpr bool kReverseColumns = _ReverseColumns;
            static constexpr int kInterleaved = _Interleaved ? 1 : 0;
        };

#if __GNUG__ < 10

        #warning this code might not be up to date

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

#else

        // rotation

        template<typename _Ta = CoordinateHelperType>
        typename _Ta::type getRow(typename _Ta::type row, typename _Ta::type col) const {
            if constexpr (_Ta::kRotate) {
                return col;
            }
            else {
                return row;
            }
        }

        template<typename _Ta = CoordinateHelperType>
        typename _Ta::type getCol(typename _Ta::type row, typename _Ta::type col) const {
            if constexpr (_Ta::kRotate) {
                return row;
            }
            else {
                return col;
            }
        }

        // reversed and interleaved rows

        template<typename _Ta = CoordinateHelperType>
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            if constexpr (_Ta::kInterleaved) {
                if constexpr (_Ta::kReverseRows) {
                    if (col % 2 == _Ta::kInterleaved) {
                        return (kRows - 1) - row;
                    }
                    return row;
                }
                else {
                    if (col % 2 == _Ta::kInterleaved) {
                        return row;
                    }
                    return (kRows - 1) - row;
                }
            }
            else {
                if constexpr (_Ta::kReverseRows) {
                    return (kRows - 1) - row;
                }
                else {
                    return row;
                }
            }
        }

        // reversed columns

        template<typename _Ta = CoordinateHelperType>
        typename _Ta::type _col(typename _Ta::type row, typename _Ta::type col) const {
            if constexpr (_Ta::kReverseColumns) {
                return (kCols - 1) - col;
            }
            else {
                return col;
            }

        }

#endif

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

    // generic class for managing pixel mapping and shapes

    template<size_t _PixelOffset, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class PixelDisplayBuffer : public PixelMapping<_PixelOffset, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved> {
    public:
        using PixelMappingType = PixelMapping<_PixelOffset, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved>;
        using PixelMappingType::getPoint;
        using PixelMappingType::getAddress;
        using PixelMappingType::kRows;
        using PixelMappingType::kCols;
        using PixelMappingType::kPixelOffset;
        using PixelMappingType::kNumPixels;
    protected:
        using PixelMappingType::kMaxPixelAddress; // = kPixelOffset + kNumPixels

    public:
        using PixelAddressType = typename PixelMappingType::PixelAddressType;
        using CoordinateType = typename PixelMappingType::CoordinateType;
        using PixelCoordinatesType = typename PixelMappingType::PixelCoordinatesType;
        using ColorType = CRGB;
        using PixelBufferPtr = ColorType *;
        using PixelBufferType = std::array<ColorType, kMaxPixelAddress>;
        using NeoPixelDataType = NeoPixelEx::DataWrapper<kMaxPixelAddress, NeoPixelEx::CRGB>;

    public:

        PixelDisplayBuffer() : _pixels(__pixels.data() + kPixelOffset)
        {
        }

        void reset() {
            __pixels.fill(ColorType());
        }

        void clear() {
            fill(ColorType());
        }

        void fill(ColorType color) {
            std::fill(begin(), end(), color);
        }

        void setPixel(CoordinateType row, CoordinateType col, ColorType color) {
            _set(getAddress(row, col), color);
        }

        // sequential pixel address 0 to kNumPixels - 1, addressing __pixels[kPixelOffset] to __pixels[kTotalPixels - 1]
        void setPixel(PixelAddressType numPixel, ColorType color) {
            _set(getAddress(getPoint(numPixel)), color);
        }

        void setPixel(PixelCoordinatesType point, ColorType color) {
            _set(getAddress(point), color);
        }

        const ColorType &getPixel(CoordinateType row, CoordinateType col) const {
            return _get(getAddress(row, col));
        }

        // sequential pixel address 0 to kNumPixels - 1, addressing __pixels[kPixelOffset] to __pixels[kTotalPixels - 1]
        const ColorType &getPixel(PixelAddressType numPixel) const {
            return _get(getAddress(getPoint(numPixel)));
        }

        const ColorType &getPixel(PixelCoordinatesType point) const {
            return _get(point);
        }

#if __GNUG__ < 10

        #warning this code might not be up to date

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

#else

        template<typename _Ta, typename _Tb>
        static void copy(_Ta src, _Tb dst, PixelAddressType numPixel) {
            if constexpr (_Ta::kMappingTypeId == _Tb::kMappingTypeId) {
                std::copy(src.begin(), src.begin() + numPixel, dst.begin());
            }
            else {
                for (typename _Ta::PixelAddressType i = 0; i < numPixel; i++) {
                    // translate the address of each pixel if the mapping is different
                    dst[dst.getAddress(src.getPoint(i))] = src[i];
                }
            }
        }

#endif

        // access to all pixels, starting with first LED
        ColorType &pixels(PixelAddressType address) {
            __DBG_assert_printf(address >= 0 && address < size(), "address out of bounds: %d", address);
            return __pixels[address];
        }

        ColorType pixels(PixelAddressType address) const {
            __DBG_assert_printf(address >= 0 && address < size(), "address out of bounds: %d", address);
            return __pixels[address];
        }

        // access to LEDs without the pixel offset
        ColorType &operator [](PixelAddressType idx) {
            return _get(idx);
        }

        const ColorType &operator [](PixelAddressType idx) const {
            return _get(idx);
        }

        PixelBufferPtr begin() {
            return &_pixels[0];
        }

        const PixelBufferPtr begin() const {
            return &_pixels[0];
        }

        PixelBufferPtr end() {
            return &_pixels[kNumPixels];
        }

        constexpr const PixelBufferPtr end() const {
            return &_pixels[kNumPixels];
        }

        constexpr size_t size() const {
            return kNumPixels;
        }

        // debug and dummy methods
        //

        void print(const String &text) {
        }

        void print(const char *text) {
        }

        void setPixelState(PixelAddressType, bool)  {
        }

        bool getPixelState(PixelAddressType) const {
            return true;
        }

        void showAll()  {
        }

        void hideAll() {
        }

        void dump(Print &output) {
            output.printf_P(PSTR("data=%p pixels=%p offset=%u num=%u mode=led_matrix brightness=%u\n"), __pixels.data(), _pixels, kPixelOffset, kNumPixels, FastLED.getBrightness());
        }

    protected:
        void _set(PixelAddressType idx, ColorType color) {
            if (idx < kNumPixels) {
                _pixels[idx] = color;
            }
        }

        ColorType &_get(PixelAddressType idx) {
            if (idx < kNumPixels) {
                return _pixels[idx];
            }
            static ColorType invalid = ColorType(0);
            return invalid;
        }

        const ColorType &_get(PixelAddressType idx) const {
            if (idx < kNumPixels) {
                return _pixels[idx];
            }
            static ColorType invalid = ColorType(0);
            return invalid;
        }

    protected:
        PixelBufferType __pixels;
        PixelBufferPtr _pixels; // __pixels.data() + kPixelOffset
    };

    // FastLED controller adapter

    template<uint8_t _OutputPin>
    class NeoPixelController {
    public:
        static CLEDController &addLeds(CRGB *data, int sizeOrOffset, int size = 0) {
            return FastLED.addLeds<NEOPIXEL, _OutputPin>(data, sizeOrOffset, size);
        }
    };

    // FastLED display

    template<typename _ControllerType, typename _PixelDisplayBufferType>
    class PixelDisplay : public _PixelDisplayBufferType
    {
    public:
        using PixelBufferType = _PixelDisplayBufferType;
        using PixelBufferType::__pixels;
        using PixelBufferType::_pixels;
        using PixelBufferType::begin;
        using PixelBufferType::end;
        using PixelBufferType::operator[];
        using PixelBufferType::clear;
        using PixelBufferType::fill;
        using PixelBufferType::reset;
        using PixelBufferType::size;
        using PixelBufferType::kPixelOffset;
        using PixelBufferType::kNumPixels;

    public:
        PixelDisplay() :
            _controller(_ControllerType::addLeds(__pixels.data(), __pixels.size(), _PixelDisplayBufferType::kPixelOffset))
        {
            setDither(false);
        }

        void setDither(bool enable) {
            _controller.setDither(enable ? BINARY_DITHER : DISABLE_DITHER);
            FastLED.setDither(_controller.getDither());
        }

        void reset() {
            PixelBufferType::reset();
            show();
        }

        void setBrightness(uint8_t brightness) {
            FastLED.setBrightness(brightness);
        }

        void show() {
            show(FastLED.getBrightness());
        }

        void show(uint8_t brightness) {
            if (getNeopixelShowMethodInt() == static_cast<uint8_t>(Clock::ShowMethodType::FASTLED)) {
                FastLED.show(brightness);
            }
            else if (getNeopixelShowMethodInt() == static_cast<uint8_t>(Clock::ShowMethodType::NEOPIXEL)) {
                NeoPixel_espShow(IOT_CLOCK_WS2812_OUTPUT, reinterpret_cast<uint8_t *>(_pixels), kNumPixels, brightness);
            }
        }

        void delay(unsigned long ms) {
            if (can_yield() && (getNeopixelShowMethodInt() == static_cast<uint8_t>(Clock::ShowMethodType::FASTLED)) && (_controller.getDither() != DISABLE_DITHER) && (FastLED.getBrightness() != 0) && (FastLED.getBrightness() != 255)) {
                // use FastLED.delay for dithering
                FastLED.delay(ms);
            }
            else {
                ::delay(ms);
            }
        }

    protected:
        CLEDController &_controller;
    };

}

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
