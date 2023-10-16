/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "color.h"
#include <NeoPixelEx.h>
#if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
#    include <Adafruit_NeoPixelEx.h>
#endif

#if __GNUG__ < 8
#    error not supported
#endif

extern "C" uint8_t getNeopixelShowMethodInt();
extern "C" const __FlashStringHelper *getNeopixelShowMethodStr();

#if DEBUG_IOT_CLOCK
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// class to address LED matrix displays by x and y coordinates mapping it to its real coordinates
// the mapping is done during compilation and fixed for best performance
//
// the required memory is total pixels (including pixels that are not display, see _PixelOffset) * 3 byte and 32bit padded, plus an additional pointer
// 118 pixel * 3 / 4 = 88.5 = with padding 89 * 4 + 4 byte = 360 byte

namespace Clock {

    enum class ShowMethodType : uint8_t {
        NONE = 0,
        FASTLED,
        #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
            NEOPIXEL_EX,
        #endif
        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
            AF_NEOPIXEL,
        #endif
        MAX
    };

    inline static ShowMethodType getNeopixelShowMethodType()
    {
        return static_cast<ShowMethodType>(getNeopixelShowMethodInt());
    }

    template<class _CoordinateType, _CoordinateType _Rows, _CoordinateType _Columns>
    class PixelCoordinates {
    public:
        using CoordinateType = _CoordinateType;

        static constexpr CoordinateType kRows = _Rows;
        static constexpr CoordinateType kCols = _Columns;

    public:
        PixelCoordinates(CoordinateType row = 0, CoordinateType col = 0) : _row(row), _col(col)
        {
        }

        CoordinateType row() const
        {
            return _row;
        }

        CoordinateType &row()
        {
            return _row;
        }

        CoordinateType col() const
        {
            return _col;
        }

        CoordinateType &col()
        {
            return _col;
        }

        void invertColumn()
        {
            _col = (kCols - 1) - _col;
        }

        void invertColumn(CoordinateType numCols)
        {
            _col = (numCols - 1) - _col;
        }

        void invertRow()
        {
            _row = (kRows - 1) - _row;
        }

        void invertRow(CoordinateType numRows)
        {
            _row = (numRows - 1) - _row;
        }

        void rotate()
        {
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
    protected:
        using TypesType::kRows;
        using TypesType::kCols;
        using TypesType::kNumPixels;
        using TypesType::kPixelOffset;
        using TypesType::kMaxPixelAddress; // kPixelOffset + kNumPixels

    private:
        static constexpr uint8_t kMappingTypeId = (_ReverseRows ? 0x01 : 0x00) | (_ReverseColumns ? 0x02 : 0x00) | (_Rotate ? 0x04 : 0) | (_Interleaved ? 0x08 : 0);

    public:
        constexpr uint8_t getMappingTypeId() const
        {
            return kMappingTypeId;
        }

        constexpr PixelAddressType getOffset() const
        {
            return kPixelOffset;
        }

        bool setParams(CoordinateType rows, CoordinateType cols, bool rowsReversed, bool colsReversed, bool rotated, bool interleaved, PixelAddressType rowOfs = 0, PixelAddressType colOfs = 0)
        {
            return false;
        }

        template<typename _Ta>
        bool setParams(const _Ta &display)
        {
            return false;
        }

        constexpr size_t size() const
        {
            return kRows * kCols;
        }

        constexpr CoordinateType getRows() const
        {
            return kRows;
        }

        constexpr CoordinateType getCols() const
        {
            return kCols;
        }

        constexpr bool isRowsReversed() const
        {
            return _ReverseRows;
        }

        constexpr bool isColsReversed() const
        {
            return _ReverseColumns;
        }

        constexpr bool isRotated() const
        {
            return _Rotate;
        }

        constexpr bool isInterleaved() const
        {
            return _Interleaved;
        }

        struct CoordinateHelperType {
            using type = CoordinateType;

            static constexpr bool kRotate = _Rotate;
            static constexpr bool kReverseRows = _ReverseRows;
            static constexpr bool kReverseColumns = _ReverseColumns;
            static constexpr int kInterleaved = _Interleaved;
        };

        // rotation

        template<typename _Ta = CoordinateHelperType>
        inline __attribute__((__always_inline__))
        typename _Ta::type getRow(typename _Ta::type row, typename _Ta::type col) const
        {
            if __CONSTEXPR17 (_Ta::kRotate) {
                return col;
            }
            else {
                return row;
            }
        }

        template<typename _Ta = CoordinateHelperType>
        inline __attribute__((__always_inline__))
        typename _Ta::type getCol(typename _Ta::type row, typename _Ta::type col) const
        {
            if __CONSTEXPR17 (_Ta::kRotate) {
                return row;
            }
            else {
                return col;
            }
        }

        // reversed and interleaved rows

        template<typename _Ta = CoordinateHelperType>
        inline __attribute__((__always_inline__))
        typename _Ta::type _row(typename _Ta::type row, typename _Ta::type col) const
        {
            if __CONSTEXPR17 (_Ta::kInterleaved) {
                if __CONSTEXPR17 (_Ta::kReverseRows) {
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
                if __CONSTEXPR17 (_Ta::kReverseRows) {
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
        typename _Ta::type _col(typename _Ta::type row, typename _Ta::type col) const
        {
            if __CONSTEXPR17 (_Ta::kReverseColumns) {
                return (kCols - 1) - col;
            }
            else {
                return col;
            }

        }

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(CoordinateType row, CoordinateType col) const
        {
            // return row + col * kRows;
            return _row(getRow(row, col), getCol(row, col)) + (_col(getRow(row, col), getCol(row, col)) * kRows);
        }

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(PixelCoordinatesType coords) const
        {
            return getAddress(coords.row(), coords.col());
        }

        inline __attribute__((__always_inline__))
        PixelCoordinatesType getPoint(PixelAddressType address) const
        {
            if __CONSTEXPR17 (kRows == 1) {
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
    protected:
        using TypesType::kRows;
        using TypesType::kCols;
        using TypesType::kNumPixels;
        using TypesType::kPixelOffset;
        using TypesType::kMaxPixelAddress; // kPixelOffset + kNumPixels

        // the arguments can be changed during runtime
        // it is 10-30% slower than the static version
        bool _reverseRows{_ReverseRows};
        bool _reverseColumns{_ReverseColumns};
        bool _rotate{_Rotate};
        bool _interleaved{_Interleaved};
        PixelAddressType _rowOfs{0};
        PixelAddressType _colOfs{0};
        CoordinateType _rows{kRows};
        CoordinateType _cols{kCols};
        PixelAddressType _offset{_PixelOffset};

    public:
        uint8_t getMappingTypeId() const
        {
            return (_reverseRows ? 0x01 : 0x00) | (_reverseColumns ? 0x02 : 0x00) | (_rotate ? 0x04 : 0) | (_interleaved ? 0x08 : 0) | (_rowOfs ? 0x10 : 0) | (_colOfs ? 0x20 : 0);
        }

        PixelAddressType getOffset() const
        {
            return _offset;
        }

        bool setParams(CoordinateType rows, CoordinateType cols, bool rowsReversed, bool colsReversed, bool rotated, bool interleaved, PixelAddressType rowOfs = 0, PixelAddressType colOfs = 0)
        {
            if (static_cast<size_t>((rows * cols) + _PixelOffset) > kMaxPixelAddress) {
                return false;
            }
            _rows = rows;
            _cols = cols;
            _reverseRows = rowsReversed;
            _reverseColumns = colsReversed;
            _rotate = rotated;
            _interleaved = interleaved;
            _rowOfs = rowOfs % _rows;
            _colOfs = colOfs % _cols;
            __LDBG_printf("size=%u ofs=%u max=%u method=%s", size(), _PixelOffset, kMaxPixelAddress, (PGM_P)getNeopixelShowMethodStr());
            return true;
        }

        template<typename _Ta>
        bool setParams(const _Ta &display)
        {
            _rows = display._rows;
            _cols = display._cols;
            _reverseRows = display._reverseRows;
            _reverseColumns = display._reverseColumns;
            _rotate = display._rotate;
            _interleaved = display._interleaved;
            _rowOfs = display._rowOfs;
            _colOfs = display._colOfs;
            return true;
        }

        constexpr size_t size() const
        {
            return getRows() * getCols();
        }

        constexpr CoordinateType getRows() const
        {
            return _rows;
        }

        constexpr CoordinateType getCols() const
        {
            return _cols;
        }

        bool isRowsReversed() const
        {
            return _reverseRows;

        }

        bool isColsReversed() const
        {
            return _reverseColumns;
        }

        bool isRotated() const
        {
            return _rotate;
        }

        bool isInterleaved() const
        {
            return _interleaved;
        }

        // row offset
        inline __attribute__((__always_inline__))
        void _applyRowOffset(CoordinateType &row) const
        {
            if (_rowOfs) {
                row = (row + _rowOfs) % _rows;
            }
        }

        // col offset
        inline __attribute__((__always_inline__))
        void _applyColOffset(CoordinateType &col) const
        {
            if (_colOfs) {
                col = (col + _colOfs) % _cols;
            }
        }

        // rotation
        inline __attribute__((__always_inline__))
        CoordinateType getRow(CoordinateType row, CoordinateType col) const
        {
            if (_rotate) {
                return col;
            }
            else {
                return row;
            }
        }

        inline __attribute__((__always_inline__))
        CoordinateType getCol(CoordinateType row, CoordinateType col) const
        {
            if (_rotate) {
                return row;
            }
            else {
                return col;
            }
        }

        // reversed and interleaved rows
        inline __attribute__((__always_inline__))
        CoordinateType _row(CoordinateType row, CoordinateType col) const
        {
            if (_interleaved) {
                _applyRowOffset(row);
                _applyColOffset(col);
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
                _applyRowOffset(row);
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
        CoordinateType _col(CoordinateType row, CoordinateType col) const
        {
            _applyColOffset(col);
            if (_reverseColumns) {
                return (_cols - 1) - col;
            }
            else {
                return col;
            }
        }

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(CoordinateType row, CoordinateType col) const
        {
            return _row(getRow(row, col), getCol(row, col)) + (_col(getRow(row, col), getCol(row, col)) * _rows);
        }

        inline __attribute__((__always_inline__))
        PixelAddressType getAddress(PixelCoordinatesType coords) const
        {
            return getAddress(coords.row(), coords.col());
        }

        inline __attribute__((__always_inline__))
        PixelCoordinatesType getPoint(PixelAddressType address) const
        {
            CoordinateType col = address / _rows;
            CoordinateType row = address % _rows;
            return PixelCoordinatesType(_row(getRow(row, col), getCol(row, col)), _col(getRow(row, col), getCol(row, col)));
        }
    };

    // generic class for managing pixel mapping and shapes

    template<size_t _PixelOffset, size_t _Rows, size_t _Columns, bool _ReverseRows, bool _ReverseColumns, bool _Rotate, bool _Interleaved>
    class PixelDisplayBuffer : public IOT_CLOCK_PIXEL_MAPPING_TYPE<_PixelOffset, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved> {
    public:
        using PixelMappingType = IOT_CLOCK_PIXEL_MAPPING_TYPE<_PixelOffset, _Rows, _Columns, _ReverseRows, _ReverseColumns, _Rotate, _Interleaved>;
        using PixelMappingType::getPoint;
        using PixelMappingType::getAddress;
        using PixelMappingType::isRowsReversed;
        using PixelMappingType::isColsReversed;
        using PixelMappingType::isRotated;
        using PixelMappingType::isInterleaved;
        using PixelMappingType::getOffset;
        using PixelMappingType::size;
        using PixelMappingType::getRows;
        using PixelMappingType::getCols;

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

        PixelDisplayBuffer() :
            _pixels(__pixels.data() + getOffset())
        {
        }

        bool setParams(CoordinateType rows, CoordinateType cols, bool rowsReversed, bool colsReversed, bool rotated, bool interleaved, PixelAddressType rowOfs = 0, PixelAddressType colOfs = 0)
        {
            if (!PixelMappingType::setParams(rows, cols, rowsReversed, colsReversed, rotated, interleaved, rowOfs, colOfs)) {
                return false;
            }
            _pixels = __pixels.data() + getOffset();
            return true;
        }

        template<typename _Ta>
        bool setParams(const _Ta &display)
        {
            if (!PixelMappingType::setParams(display)) {
                return false;
            }
            _pixels = __pixels.data() + getOffset();
            return true;
        }

        void setSize(CoordinateType rows, CoordinateType cols)
        {
            PixelMappingType::_rows = rows;
            PixelMappingType::_cols = cols;
        }

        size_t getMaxNumPixels() const
        {
            return PixelMappingType::_rows * PixelMappingType::_cols;
        }

        void reset()
        {
            __pixels.fill(ColorType());
        }

        void clear()
        {
            fill(ColorType());
        }

        void fill(ColorType color)
        {
            std::fill(begin(), end(), color);
        }

        constexpr size_t getNumPixels() const
        {
            return size();
        }

        inline __attribute__((__always_inline__))
        void setPixel(CoordinateType row, CoordinateType col, ColorType color)
        {
            _set(getAddress(row, col), color);
        }

        // sequential pixel address 0 to kNumPixels - 1, addressing __pixels[kPixelOffset] to __pixels[kTotalPixels - 1]
        inline __attribute__((__always_inline__))
        void setPixel(PixelAddressType numPixel, ColorType color)
        {
            _set(getAddress(getPoint(numPixel)), color);
        }

        inline __attribute__((__always_inline__))
        void setPixel(PixelCoordinatesType point, ColorType color)
        {
            _set(getAddress(point), color);
        }

        inline __attribute__((__always_inline__))
        const ColorType &getPixel(CoordinateType row, CoordinateType col) const
        {
            return _get(getAddress(row, col));
        }

        // sequential pixel address 0 to kNumPixels - 1, addressing __pixels[kPixelOffset] to __pixels[kTotalPixels - 1]
        inline __attribute__((__always_inline__))
        const ColorType &getPixel(PixelAddressType numPixel) const
        {
            return _get(getAddress(getPoint(numPixel)));
        }

        inline __attribute__((__always_inline__))
        const ColorType &getPixel(PixelCoordinatesType point) const
        {
            return _get(point);
        }

        template<typename _Ta, typename _Tb>
        static void copy(_Ta src, _Tb dst, PixelAddressType numPixel)
        {
            if (src.getMappingTypeId() == dst.getMappingTypeId()) {
                std::copy(src.begin(), src.begin() + numPixel, dst.begin());
            }
            else {
                // if the mapping is different translate the address of each pixel
                for (typename _Ta::PixelAddressType i = 0; i < numPixel; i++) {
                    dst.pixels(dst.getAddress(src.getPoint(i))) = src.pixels(i);
                }
            }
        }

        // access to all pixels, starting with first LED
        inline __attribute__((__always_inline__))
        ColorType &pixels(PixelAddressType address)
        {
            // __DBG_assertf(address >= 0 && address < size(), "address out of bounds: %d", address);
            return __pixels[address];
        }

        inline __attribute__((__always_inline__))
        ColorType pixels(PixelAddressType address) const
        {
            // __DBG_assertf(address >= 0 && address < size(), "address out of bounds: %d", address);
            return __pixels[address];
        }

        // access to LEDs without the pixel offset
        inline __attribute__((__always_inline__))
        ColorType &operator [](PixelAddressType idx)
        {
            return _get(idx);
        }

        inline __attribute__((__always_inline__))
        const ColorType &operator [](PixelAddressType idx) const
        {
            return _get(idx);
        }

        inline __attribute__((__always_inline__))
        PixelBufferPtr begin()
        {
            return &_pixels[0];
        }

        inline __attribute__((__always_inline__))
        const PixelBufferPtr begin() const
        {
            return &_pixels[0];
        }

        inline __attribute__((__always_inline__))
        PixelBufferPtr end()
        {
            return &_pixels[size()];
        }

        constexpr const PixelBufferPtr end() const
        {
            return &_pixels[size()];
        }

        // debug and dummy methods
        //

        void print(const String &text)
        {
        }

        void print(const char *text)
        {
        }

        void setPixelState(PixelAddressType, bool)
        {
        }

        bool getPixelState(PixelAddressType) const
        {
            return true;
        }

        void showAll()
        {
        }

        void hideAll()
        {
        }

        void dump(Print &output)
        {
            output.printf_P(PSTR("data=%p pixels=%p offset=%u num=%u mode=led_matrix brightness=%u\n"), __pixels.data(), _pixels, getOffset(), size(), FastLED.getBrightness());
        }

    protected:
        inline __attribute__((__always_inline__))
        void _set(PixelAddressType idx, ColorType color)
        {
            if (idx < size()) {
                _pixels[idx] = color;
            }
        }

        inline __attribute__((__always_inline__))
        ColorType &_get(PixelAddressType idx)
        {
            if (idx < size()) {
                return _pixels[idx];
            }
            static ColorType invalid = ColorType(0);
            return invalid;
        }

        inline __attribute__((__always_inline__))
        const ColorType &_get(PixelAddressType idx) const
        {
            if (idx < size()) {
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

    #ifndef FASTLED_LED_CONTROLLER
    #    define FASTLED_LED_CONTROLLER NEOPIXEL
    #endif

    #ifndef NEOPIXEL_LED_TYPE
    #    define NEOPIXEL_LED_TYPE (NEO_GRB + NEO_KHZ800)
    #endif

    #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) || defined(IOT_LED_MATRIX_OUTPUT_PIN2) || defined(IOT_LED_MATRIX_OUTPUT_PIN3)
    #    define IOT_LED_MATRIX_MULTI_OUTPUT 1
    #    ifndef IOT_LED_MATRIX_OUTPUT_PIN1
    #        define IOT_LED_MATRIX_OUTPUT_PIN1 -1
    #    endif
    #    ifndef IOT_LED_MATRIX_OUTPUT_PIN2
    #        define IOT_LED_MATRIX_OUTPUT_PIN2 -1
    #    endif
    #    ifndef IOT_LED_MATRIX_OUTPUT_PIN3
    #        define IOT_LED_MATRIX_OUTPUT_PIN3 -1
    #    endif
    #    if IOT_LED_MATRIX_OUTPUT_PIN1 == -1 && IOT_LED_MATRIX_OUTPUT_PIN2 == -1 && IOT_LED_MATRIX_OUTPUT_PIN3 == -1
    #        error either of the PINs must be valid
    #    endif
    #    if IOT_LED_MATRIX_OUTPUT_PIN1 == -1 && (IOT_LED_MATRIX_OUTPUT_PIN2 != -1 || IOT_LED_MATRIX_OUTPUT_PIN3 != -1)
    #        error if PIN2 or PIN3 are defined, PIN1 must be defined as well
    #    endif
    #    if IOT_LED_MATRIX_OUTPUT_PIN2 == -1 && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
    #        error if PIN3 is defined, PIN2 must be defined as well
    #    endif
    #endif

    // FastLED display

    template<typename _PixelDisplayBufferType>
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
        using PixelBufferType::isRowsReversed;
        using PixelBufferType::isColsReversed;
        using PixelBufferType::isRotated;
        using PixelBufferType::isInterleaved;
        using PixelBufferType::getOffset;
        using PixelBufferType::setParams;
        using PixelBufferType::getRows;
        using PixelBufferType::getCols;
        using PixelBufferType::size;

    public:

        // all segments need to be added, the size can be changed later
        PixelDisplay() :
            _emptyPixel(0),
            _controller(FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN>(&_emptyPixel, 1))
            #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                , _neoPixels{nullptr}
            #endif
        {
            setDither(false);
            #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                auto neoPixelPtr = _neoPixels;
                *neoPixelPtr++ = new Adafruit_NeoPixelEx(0, nullptr, IOT_LED_MATRIX_OUTPUT_PIN, NEOPIXEL_LED_TYPE);
            #endif
            #if IOT_LED_MATRIX_MULTI_OUTPUT
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
                    FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN1>(&_emptyPixel, 1);
                    #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                        *neoPixelPtr++ = new Adafruit_NeoPixelEx(0, nullptr, IOT_LED_MATRIX_OUTPUT_PIN1, NEOPIXEL_LED_TYPE);
                    #endif
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
                    FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN2>(&_emptyPixel, 1);
                    #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                        *neoPixelPtr++ = new Adafruit_NeoPixelEx(0, nullptr, IOT_LED_MATRIX_OUTPUT_PIN2, NEOPIXEL_LED_TYPE);
                    #endif
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
                    FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN3>(&_emptyPixel, 1);
                    #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                        *neoPixelPtr++ = new Adafruit_NeoPixelEx(0, nullptr, IOT_LED_MATRIX_OUTPUT_PIN3, NEOPIXEL_LED_TYPE);
                    #endif
                #endif
            #endif
        }

        ~PixelDisplay()
        {
            #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                auto neoPixelPtr = _neoPixels;
                while(*neoPixelPtr) {
                    delete *neoPixelPtr;
                    neoPixelPtr++;
                }
            #endif
        }

        // updates the size of the segments
        void updateSegments(uint16_t num0, uint16_t ofs0, uint16_t num1, uint16_t ofs1, uint16_t num2, uint16_t ofs2, uint16_t num3, uint16_t ofs3)
        {
            fill(0);
            show(0);
            _numSegments = 0;

            #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                auto neoPixelPtr = _neoPixels;
                while(*neoPixelPtr) {
                    (*neoPixelPtr++)->updateLength(0, nullptr);
                }
            #endif

            auto fastLedPtr = &_controller;
            #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                neoPixelPtr = _neoPixels;
            #endif
            if (num0) {
                __LDBG_printf("segment 0 pixels=%p ofs=%u num=%u", __pixels.data() + ofs0, ofs0, num0);
                fastLedPtr->setLeds(__pixels.data() + ofs0, (num0));
                #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                    (*neoPixelPtr++)->updateLength(num0, reinterpret_cast<uint8_t *>(__pixels.data() + ofs0));
                #endif
                _numSegments++;
            }
            else {
                __LDBG_printf("segment 0 pixels=%p ofs=%u num=%u", _emptyPixel, ofs0, num0);
                fastLedPtr->setLeds(&_emptyPixel, 1);
                #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                    (*neoPixelPtr++)->updateLength(0, nullptr);
                #endif
            }
            #if IOT_LED_MATRIX_MULTI_OUTPUT
                for(;;) {
                    if (!(fastLedPtr = fastLedPtr->next())) {
                        break;
                    }
                    if (num1 && IOT_LED_MATRIX_OUTPUT_PIN1 != -1) {
                        __LDBG_printf("segment 1 pixels=%p ofs=%u num=%u", __pixels.data() + ofs1, ofs1, num1);
                        fastLedPtr->setLeds(__pixels.data() + ofs1, (num1));
                        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                            (*neoPixelPtr++)->updateLength(num1, reinterpret_cast<uint8_t *>(__pixels.data() + ofs1));
                        #endif
                        _numSegments++;
                    }
                    else {
                        __LDBG_printf("segment 1 pixels=%p ofs=%u num=%u", _emptyPixel, ofs1, num1);
                        fastLedPtr->setLeds(&_emptyPixel, 1);
                        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                            (*neoPixelPtr++)->updateLength(0, nullptr);
                        #endif
                    }
                    if (!(fastLedPtr = fastLedPtr->next())) {
                        break;
                    }
                    if (num2 && IOT_LED_MATRIX_OUTPUT_PIN2 != -1) {
                        __LDBG_printf("segment 2 pixels=%p ofs=%u num=%u", __pixels.data() + ofs2, ofs2, num2);
                        fastLedPtr->setLeds(__pixels.data() + ofs2, (num2));
                        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                            (*neoPixelPtr++)->updateLength(num2, reinterpret_cast<uint8_t *>(__pixels.data() + ofs2));
                        #endif
                        _numSegments++;
                    }
                    else {
                        __LDBG_printf("segment 2 pixels=%p ofs=%u num=%u", _emptyPixel, ofs2, num2);
                        fastLedPtr->setLeds(&_emptyPixel, 1);
                        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                            (*neoPixelPtr++)->updateLength(0, nullptr);
                        #endif
                    }
                    if (!(fastLedPtr = fastLedPtr->next())) {
                        break;
                    }
                    if (num3 && IOT_LED_MATRIX_OUTPUT_PIN3 != -1) {
                        __LDBG_printf("segment 3 pixels=%p ofs=%u num=%u", __pixels.data() + ofs3, ofs3, num3);
                        fastLedPtr->setLeds(__pixels.data() + ofs3, (num3));
                        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                            (*neoPixelPtr++)->updateLength(num3, reinterpret_cast<uint8_t *>(__pixels.data() + ofs3));
                        #endif
                        _numSegments++;
                    }
                    else {
                        __LDBG_printf("segment 3 pixels=%p ofs=%u num=%u", _emptyPixel, ofs3, num3);
                        fastLedPtr->setLeds(&_emptyPixel, 1);
                        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                            (*neoPixelPtr++)->updateLength(0, nullptr);
                        #endif
                    }
                    break;
                }
            #endif
        }

        inline __attribute__((__always_inline__))
        void setDither(bool enable)
        {
            FastLED.setDither(enable ? BINARY_DITHER : DISABLE_DITHER);
        }

        void reset()
        {
            PixelBufferType::reset();
            show();
        }

        inline __attribute__((__always_inline__))
        void setBrightness(uint8_t brightness)
        {
            FastLED.setBrightness(brightness);
        }

        inline __attribute__((__always_inline__))
        void show()
        {
            show(FastLED.getBrightness());
        }

        #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT

            void _showNeoPixelEx(uint8_t brightness)
            {
                auto ptr = &_controller;
                uint8_t i = 0;
                while(ptr) {
                    if (ptr->leds() != &_emptyPixel) {
                        // we need a switch since the output pin is hardcoded for performance reasons
                        using PixelType = NeoPixelEx::CRGB;
                        using DefaultTimings = NeoPixelEx::DefaultTimings;
                        using StripType = NeoPixelEx::Strip<0, 0, PixelType, DefaultTimings>;
                        switch(i) {
                            case 0:
                                StripType::externalShow<IOT_LED_MATRIX_OUTPUT_PIN, DefaultTimings, PixelType>(reinterpret_cast<uint8_t *>(ptr->leds()), ptr->size() * sizeof(PixelType), brightness, NeoPixelEx::Context::validate(nullptr));
                                break;
                            #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
                                case 1:
                                    StripType::externalShow<IOT_LED_MATRIX_OUTPUT_PIN1, DefaultTimings, PixelType>(reinterpret_cast<uint8_t *>(ptr->leds()), ptr->size() * sizeof(PixelType), brightness, NeoPixelEx::Context::validate(nullptr));
                                    break;
                            #endif
                            #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
                                case 2:
                                    StripType::externalShow<IOT_LED_MATRIX_OUTPUT_PIN2, DefaultTimings, PixelType>(reinterpret_cast<uint8_t *>(ptr->leds()), ptr->size() * sizeof(PixelType), brightness, NeoPixelEx::Context::validate(nullptr));
                                    break;
                            #endif
                            #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
                                case 3:
                                    StripType::externalShow<IOT_LED_MATRIX_OUTPUT_PIN3, DefaultTimings, PixelType>(reinterpret_cast<uint8_t *>(ptr->leds()), ptr->size() * sizeof(PixelType), brightness, NeoPixelEx::Context::validate(nullptr));
                                    break;
                            #endif
                        }
                    }
                    ptr = ptr->next();
                    i++;
                }
            }

        #endif

        void show(uint8_t brightness)
        {
            #if IOT_LED_MATRIX_FASTLED_ONLY
                FastLED.show(brightness);
            #else
                switch(getNeopixelShowMethodType()) {
                    case Clock::ShowMethodType::FASTLED: {
                        FastLED.show(brightness);
                    }
                    break;
                    #if IOT_LED_MATRIX_NEOPIXEL_EX_SUPPORT
                        case Clock::ShowMethodType::NEOPIXEL_EX: {
                            _showNeoPixelEx(brightness);
                            #if ESP32
                                // release some cpu time
                                ::delay(1);
                            #endif
                        }
                        break;
                    #endif
                    #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
                        case Clock::ShowMethodType::AF_NEOPIXEL: {
                            auto ptr = _neoPixels;
                            while(*ptr) {
                                (*ptr)->setBrightness(brightness);
                                (*ptr)->show();
                                ptr++;
                            }
                            #if ESP32
                                // release some cpu time
                                ::delay(1);
                            #endif
                        }
                        break;
                    #endif
                    default:
                        break;
                }
            #endif
        }

        bool getDither() const
        {
            return (_controller.getDither() == BINARY_DITHER);
        }

        uint32_t getNumSegments() const
        {
            return _numSegments;
        }

        void delay(unsigned long ms)
        {
            if (
            #if IOT_LED_MATRIX_FASTLED_ONLY == 0
                (getNeopixelShowMethodType() == Clock::ShowMethodType::FASTLED) &&
            #endif
                getDither() && (FastLED.getBrightness() != 0) && (FastLED.getBrightness() != 255)) {
                // use FastLED.delay for dithering
                FastLED.delay(ms);
            }
            else {
                ::delay(ms);
            }
        }

    protected:
        CRGB _emptyPixel;
        CLEDController &_controller;
        #if IOT_LED_MATRIX_NEOPIXEL_SUPPORT
            Adafruit_NeoPixelEx *_neoPixels[5];
        #endif
        uint32_t _numSegments;
    };

}

#if DEBUG_IOT_CLOCK
#    include <debug_helper_disable.h>
#endif
