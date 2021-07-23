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

#pragma GCC push_options
#pragma GCC optimize ("O3")

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
        NEOPIXEL_REPEAT,
        MAX
    };

    inline static ShowMethodType getNeopixelShowMethodType() {
        return static_cast<ShowMethodType>(getNeopixelShowMethodInt());
    }

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

    private:
        static constexpr uint8_t kMappingTypeId = (_ReverseRows ? 0x01 : 0x00) | (_ReverseColumns ? 0x02 : 0x00) | (_Rotate ? 0x04 : 0) | (_Interleaved ? 0x08 : 0);

    public:
        constexpr uint8_t getMappingTypeId() const {
            return kMappingTypeId;
        }

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
        inline __attribute__((__always_inline__))
        typename _Ta::type getRow(typename _Ta::type row, typename _Ta::type col) const {
            return col;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kRotate, int>::type = 0>
        typename _Ta::type getCol(typename _Ta::type row, typename _Ta::type col) const {
            return row;
        }

        // non-rotated

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kRotate, int>::type = 0>
        inline __attribute__((__always_inline__))
        typename _Ta::type getRow(typename _Ta::type row, typename _Ta::type col) const {
            return row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kRotate, int>::type = 0>
        inline __attribute__((__always_inline__))
        typename _Ta::type getCol(typename _Ta::type row, typename _Ta::type col) const {
            return col;
        }

        // normalize rows

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kReverseRows && !_Ta::kInterleaved, int>::type = 0>
        inline __attribute__((__always_inline__))
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            return row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kReverseRows && !_Ta::kInterleaved, int>::type = 0>
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            return (kRows - 1) - row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kReverseRows && _Ta::kInterleaved, int>::type = 0>
        inline __attribute__((__always_inline__))
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            if ((col & 1) == _Ta::kInterleaved) {
                return row;
            }
            return (kRows - 1) - row;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kReverseRows && _Ta::kInterleaved, int>::type = 0>
        inline __attribute__((__always_inline__))
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            if ((col & 1) == _Ta::kInterleaved) {
                return (kRows - 1) - row;
            }
            return row;
        }

        // normalize columns

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<!_Ta::kReverseColumns, int>::type = 0>
        inline __attribute__((__always_inline__))
        typename _Ta::type _col(typename _Ta::type row, typename _Ta::type col) const {
            return col;
        }

        template<typename _Ta = CoordinateHelperType, typename std::enable_if<_Ta::kReverseColumns, int>::type = 0>
        inline __attribute__((__always_inline__))
        typename _Ta::type _col(typename _Ta::type row, typename _Ta::type col) const {
            return (kCols - 1) - col;
        }

#else

        // rotation

        template<typename _Ta = CoordinateHelperType>
        inline __attribute__((__always_inline__))
        typename _Ta::type getRow(typename _Ta::type row, typename _Ta::type col) const {
            if constexpr (_Ta::kRotate) {
                return col;
            }
            else {
                return row;
            }
        }

        template<typename _Ta = CoordinateHelperType>
        inline __attribute__((__always_inline__))
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
        inline __attribute__((__always_inline__))
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const {
            if constexpr (_Ta::kInterleaved) {
                if constexpr (_Ta::kReverseRows) {
                    if ((col & 1) == _Ta::kInterleaved) {
                        return (kRows - 1) - row;
                    }
                    return row;
                }
                else {
                    if ((col & 1) == _Ta::kInterleaved) {
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
        inline __attribute__((__always_inline__))
        typename _Ta::type _col(typename _Ta::type row, typename _Ta::type col) const {
            if constexpr (_Ta::kReverseColumns) {
                return (kCols - 1) - col;
            }
            else {
                return col;
            }

        }

#endif

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(CoordinateType row, CoordinateType col) const {
            // return row + col * kRows;
            return _row(getRow(row, col), getCol(row, col)) + (_col(getRow(row, col), getCol(row, col)) * kRows);
        }

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(PixelCoordinatesType coords) const {
            return getAddress(coords.row(), coords.col());
        }

        inline __attribute__((__always_inline__))
        PixelCoordinatesType getPoint(PixelAddressType address) const {
            if constexpr (kRows == 1) {
                return PixelCoordinatesType(_row(getRow(0, address), getCol(0, address)), _col(getRow(0, address), getCol(0, address)));
            }
            else {
                CoordinateType row = address % kRows;
                CoordinateType col = address / kRows;
                return PixelCoordinatesType(_row(getRow(row, col), getCol(row, col)), _col(getRow(row, col), getCol(row, col)));
            }
        }
    };



    template<size_t _PixelOffset, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class DynamicPixelMapping : public Types<_PixelOffset, _Rows, _Columns> {
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

    public:
        // the arguments can be changed during runtime
        // it is 10-30% slower than the static version
        bool _reverseRows{_ReverseRows};
        bool _reverseColumns{_ReverseColumns};
        bool _rotate{_Rotate};
        bool _interleaved{_Interleaved};
        CoordinateType _rows{kRows};
        CoordinateType _cols{kCols};

        uint8_t getMappingTypeId() const {
            return (_reverseRows ? 0x01 : 0x00) | (_reverseColumns ? 0x02 : 0x00) | (_rotate ? 0x04 : 0) | (_interleaved ? 0x08 : 0);
        }


        // rotation
        inline __attribute__((__always_inline__))
        CoordinateType getRow(CoordinateType row, CoordinateType col) const {
            if (_rotate) {
                return col;
            }
            else {
                return row;
            }
        }

        inline __attribute__((__always_inline__))
        CoordinateType getCol(CoordinateType row, CoordinateType col) const {
            if (_rotate) {
                return row;
            }
            else {
                return col;
            }
        }

        // reversed and interleaved rows
        inline __attribute__((__always_inline__))
        CoordinateType _row(CoordinateType row, CoordinateType col) const {
            if (_interleaved) {
                if (_reverseRows) {
                    if ((col & 1) == _interleaved) {
                        return (_rows - 1) - row;
                    }
                    return row;
                }
                else {
                    if ((col & 1) == _interleaved) {
                        return row;
                    }
                    return (_rows - 1) - row;
                }
            }
            else {
                if (_reverseRows) {
                    return (_rows - 1) - row;
                }
                else {
                    return row;
                }
            }
        }

        // reversed columns
        inline __attribute__((__always_inline__))
        CoordinateType _col(CoordinateType row, CoordinateType col) const {
            if (_reverseColumns) {
                return (_cols - 1) - col;
            }
            else {
                return col;
            }

        }

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(CoordinateType row, CoordinateType col) const {
            // return row + col * _rows;
            return _row(getRow(row, col), getCol(row, col)) + (_col(getRow(row, col), getCol(row, col)) * _rows);
        }

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(PixelCoordinatesType coords) const {
            return getAddress(coords.row(), coords.col());
        }

        inline __attribute__((__always_inline__))
        PixelCoordinatesType getPoint(PixelAddressType address) const {
            if (_rows == 1) {
                return PixelCoordinatesType(_row(getRow(0, address), getCol(0, address)), _col(getRow(0, address), getCol(0, address)));
            }
            else {
                CoordinateType row = address % _rows;
                CoordinateType col = address / _rows;
                return PixelCoordinatesType(_row(getRow(row, col), getCol(row, col)), _col(getRow(row, col), getCol(row, col)));
            }
        }
    };

    // generic class for managing pixel mapping and shapes

    template<size_t _PixelOffset, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class PixelDisplayBuffer : public IOT_CLOCK_PIXEL_MAPPING_TYPE<_PixelOffset, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved> {
    public:
        using PixelMappingType = IOT_CLOCK_PIXEL_MAPPING_TYPE<_PixelOffset, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved>;
        using PixelMappingType::getPoint;
        using PixelMappingType::getAddress;
        using PixelMappingType::kRows;
        using PixelMappingType::kCols;
        using PixelMappingType::kPixelOffset;
        using PixelMappingType::kNumPixels;

        #if IOT_LED_MATRIX_CONFIGURABLE
        using PixelMappingType::_reverseRows;
        using PixelMappingType::_reverseColumns;
        using PixelMappingType::_rotate;
        using PixelMappingType::_interleaved;
        using PixelMappingType::_rows;
        using PixelMappingType::_cols;
        #endif

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

        constexpr size_t getNumPixels() const {
            return kNumPixels;
        }

        inline __attribute__((__always_inline__))
        void setPixel(CoordinateType row, CoordinateType col, ColorType color) {
            _set(getAddress(row, col), color);
        }

        // sequential pixel address 0 to kNumPixels - 1, addressing __pixels[kPixelOffset] to __pixels[kTotalPixels - 1]
        inline __attribute__((__always_inline__))
        void setPixel(PixelAddressType numPixel, ColorType color) {
            _set(getAddress(getPoint(numPixel)), color);
        }

        inline __attribute__((__always_inline__))
        void setPixel(PixelCoordinatesType point, ColorType color) {
            _set(getAddress(point), color);
        }

        inline __attribute__((__always_inline__))
        const ColorType &getPixel(CoordinateType row, CoordinateType col) const {
            return _get(getAddress(row, col));
        }

        // sequential pixel address 0 to kNumPixels - 1, addressing __pixels[kPixelOffset] to __pixels[kTotalPixels - 1]
        inline __attribute__((__always_inline__))
        const ColorType &getPixel(PixelAddressType numPixel) const {
            return _get(getAddress(getPoint(numPixel)));
        }

        inline __attribute__((__always_inline__))
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
        inline __attribute__((__always_inline__))
        ColorType &pixels(PixelAddressType address) {
            // __DBG_assert_printf(address >= 0 && address < size(), "address out of bounds: %d", address);
            return __pixels[address];
        }

        inline __attribute__((__always_inline__))
        ColorType pixels(PixelAddressType address) const {
            // __DBG_assert_printf(address >= 0 && address < size(), "address out of bounds: %d", address);
            return __pixels[address];
        }

        // access to LEDs without the pixel offset
        inline __attribute__((__always_inline__))
        ColorType &operator [](PixelAddressType idx) {
            return _get(idx);
        }

        inline __attribute__((__always_inline__))
        const ColorType &operator [](PixelAddressType idx) const {
            return _get(idx);
        }

        inline __attribute__((__always_inline__))
        PixelBufferPtr begin() {
            return &_pixels[0];
        }

        inline __attribute__((__always_inline__))
        const PixelBufferPtr begin() const {
            return &_pixels[0];
        }

        inline __attribute__((__always_inline__))
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
        inline __attribute__((__always_inline__))
        void _set(PixelAddressType idx, ColorType color) {
            if (idx < kNumPixels) {
                _pixels[idx] = color;
            }
        }

        inline __attribute__((__always_inline__))
        ColorType &_get(PixelAddressType idx) {
            if (idx < kNumPixels) {
                return _pixels[idx];
            }
            static ColorType invalid = ColorType(0);
            return invalid;
        }

        inline __attribute__((__always_inline__))
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

        inline __attribute__((__always_inline__))
        void setDither(bool enable) {
            FastLED.setDither(enable ? BINARY_DITHER : DISABLE_DITHER);
        }

        void reset() {
            PixelBufferType::reset();
            show();
        }

        inline __attribute__((__always_inline__))
        void setBrightness(uint8_t brightness) {
            FastLED.setBrightness(brightness);
        }

        inline __attribute__((__always_inline__))
        void show() {
            show(FastLED.getBrightness());
        }

        void showRepeat(uint8_t num, uint32_t delayMillis = 1) {
            while(num--) {
                show(FastLED.getBrightness());
                ::delay(delayMillis);
            }
        }

        void show(uint8_t brightness) {
            switch(getNeopixelShowMethodType()) {
                case Clock::ShowMethodType::FASTLED:
                    #if NEOPIXEL_DEBUG
                        NeoPixelEx::Context::validate(nullptr).getDebugContext().togglePin();
                    #endif
                    FastLED.show(brightness);
                    break;
                case Clock::ShowMethodType::NEOPIXEL:
                    NeoPixelEx::StaticStrip::externalShow<NeoPixelEx::TimingsWS2812, NeoPixelEx::CRGB>(IOT_LED_MATRIX_OUTPUT_PIN, reinterpret_cast<uint8_t *>(_pixels), kNumPixels, brightness, NeoPixelEx::Context::validate(nullptr));
                    break;
                case Clock::ShowMethodType::NEOPIXEL_REPEAT:
                    for(uint8_t i = 0; i  < 5; i++) {
                        if (NeoPixelEx::StaticStrip::externalShow<NeoPixelEx::TimingsWS2812, NeoPixelEx::CRGB>(IOT_LED_MATRIX_OUTPUT_PIN, reinterpret_cast<uint8_t *>(_pixels), kNumPixels, brightness, NeoPixelEx::Context::validate(nullptr))) {
                            break;
                        }
                        ::delay(1);
                    }
                    break;
                default:
                    break;
            }
        }

        void delay(unsigned long ms) {
            if ((getNeopixelShowMethodType() == Clock::ShowMethodType::FASTLED) && (_controller.getDither() != DISABLE_DITHER) && (FastLED.getBrightness() != 0) && (FastLED.getBrightness() != 255)) {
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

#pragma GCC pop_options

#if DEBUG_IOT_CLOCK
#include <debug_helper_disable.h>
#endif
