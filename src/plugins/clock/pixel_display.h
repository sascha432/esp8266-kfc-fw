/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <type_traits>
#include "color.h"
#include <NeoPixelEx.h>
#include <memory>
#include <new>
#include "matrix_validation.h"
#if IOT_LED_MATRIX_HAVE_PANELS
#    include "segment_panel.h"
#endif
#if HAVE_NEOPIXELBUS
#    include "pixel_output_neobus.h"
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
        #if HAVE_NEOPIXELBUS
            NEOBUS_RMT,
            NEOBUS_I2S,
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

        PixelAddressType getRowOfs() const
        {
            return _rowOfs;
        }

        PixelAddressType getColOfs() const
        {
            return _colOfs;
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
        #if IOT_LED_MATRIX_HAVE_PANELS
            using PixelMappingType::getRowOfs;
            using PixelMappingType::getColOfs;
        #endif

    protected:
        using PixelMappingType::kMaxPixelAddress; // = kPixelOffset + kNumPixels

    public:
        using PixelAddressType = typename PixelMappingType::PixelAddressType;
        using CoordinateType = typename PixelMappingType::CoordinateType;
        using PixelCoordinatesType = typename PixelMappingType::PixelCoordinatesType;
        using ColorType = PixelRGB;
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

        // the brightness is owned by the display (FastLED or NeoPixelBus), see PixelDisplay::dump()
        void dump(Print &output, uint8_t brightness)
        {
            output.printf_P(PSTR("data=%p pixels=%p offset=%u num=%u mode=led_matrix brightness=%u\n"), __pixels.data(), _pixels, getOffset(), size(), brightness);
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
        #if IOT_LED_MATRIX_HAVE_PANELS
            using PixelBufferType::getRowOfs;
            using PixelBufferType::getColOfs;
        #endif
        using PixelBufferType::setParams;
        using PixelBufferType::getRows;
        using PixelBufferType::getCols;
        using PixelBufferType::size;

    public:

        // all segments need to be added, the size can be changed later
        PixelDisplay() :
            #if HAVE_NEOPIXELBUS
                _brightness(255),
                _neoBusMethod(NeoBusMethodType::RMT),
            #else
                _emptyPixel(0),
                _controller(FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN>(&_emptyPixel, 1)),
            #endif
            _segPixels{},
            #if IOT_LED_MATRIX_HAVE_PANELS
                _segPanel{},
                _segLut{},
                #if !HAVE_NEOPIXELBUS
                    _segBuffer{},
                #endif
            #endif
            _numSegments(0)
        {
            setDither(false);
            #if IOT_LED_MATRIX_MULTI_OUTPUT && !HAVE_NEOPIXELBUS
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
                    FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN1>(&_emptyPixel, 1);
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
                    FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN2>(&_emptyPixel, 1);
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
                    FastLED.addLeds<FASTLED_LED_CONTROLLER, IOT_LED_MATRIX_OUTPUT_PIN3>(&_emptyPixel, 1);
                #endif
            #endif
        }

        #if IOT_LED_MATRIX_HAVE_PANELS
            // panel of each segment (see SegmentPanel), applied by the next updateSegments(). The LUTs are
            // built with the pixel mapping (rows/cols, reverse, rotation, interleaved), setParams() has to be
            // called first
            void setSegmentPanels(const SegmentPanel *panels)
            {
                for (uint8_t i = 0; i < MatrixValidation::kMaxSegments; i++) {
                    _segPanel[i] = panels[i];
                }
            }

            const SegmentPanel &getSegmentPanel(uint8_t index) const
            {
                return _segPanel[index];
            }

            // true if the panel is transmitted straight from the pixel buffer (LED i = buffer pixel i)
            bool isSegmentIdentity(uint8_t index) const
            {
                return !_segLut[index];
            }
        #endif

        // LEDs the segment transmits, 0 if it is disabled or does not fit into the buffer
        uint16_t getSegmentPixels(uint8_t index) const
        {
            return _segPixels[index];
        }

        // rebuilds the transport: the panels of the segments (their LUTs depend on the pixel mapping, so
        // this is required after setParams() as well), or the mapped matrix on the first output pin of a
        // static display
        void updateSegments()
        {
            fill(0);
            show(0);
            #if IOT_LED_MATRIX_HAVE_PANELS
                _buildLuts();
            #else
                // the matrix (rows x columns) starting at the pixel offset, inside the buffer
                const uint32_t available = __pixels.size() - getOffset();
                _segPixels[0] = static_cast<uint16_t>(std::min<uint32_t>(size(), available));
            #endif

            #if HAVE_NEOPIXELBUS
                // the strips are rebuilt for the new segment layout, they are also rebuilt by show()
                // when the show method is changed at runtime
                _rebuildStrips();
            #endif

            #if !HAVE_NEOPIXELBUS
            _numSegments = 0;
            #if IOT_LED_MATRIX_HAVE_PANELS
                for (auto &buffer : _segBuffer) {
                    buffer.reset();
                }
            #endif

            auto fastLedPtr = &_controller;
            if (_segPixels[0]) {
                __LDBG_printf("segment 0 num=%u", _segPixels[0]);
                fastLedPtr->setLeds(_getSegmentLeds(0), _segPixels[0]);
                _numSegments++;
            }
            else {
                fastLedPtr->setLeds(&_emptyPixel, 1);
            }
            #if IOT_LED_MATRIX_MULTI_OUTPUT
                for(;;) {
                    if (!(fastLedPtr = fastLedPtr->next())) {
                        break;
                    }
                    if (_segPixels[1] && IOT_LED_MATRIX_OUTPUT_PIN1 != -1) {
                        __LDBG_printf("segment 1 num=%u", _segPixels[1]);
                        fastLedPtr->setLeds(_getSegmentLeds(1), _segPixels[1]);
                        _numSegments++;
                    }
                    else {
                        fastLedPtr->setLeds(&_emptyPixel, 1);
                    }
                    if (!(fastLedPtr = fastLedPtr->next())) {
                        break;
                    }
                    if (_segPixels[2] && IOT_LED_MATRIX_OUTPUT_PIN2 != -1) {
                        __LDBG_printf("segment 2 num=%u", _segPixels[2]);
                        fastLedPtr->setLeds(_getSegmentLeds(2), _segPixels[2]);
                        _numSegments++;
                    }
                    else {
                        fastLedPtr->setLeds(&_emptyPixel, 1);
                    }
                    if (!(fastLedPtr = fastLedPtr->next())) {
                        break;
                    }
                    if (_segPixels[3] && IOT_LED_MATRIX_OUTPUT_PIN3 != -1) {
                        __LDBG_printf("segment 3 num=%u", _segPixels[3]);
                        fastLedPtr->setLeds(_getSegmentLeds(3), _segPixels[3]);
                        _numSegments++;
                    }
                    else {
                        fastLedPtr->setLeds(&_emptyPixel, 1);
                    }
                    break;
                }
            #endif
            #endif // !HAVE_NEOPIXELBUS
        }

        inline __attribute__((__always_inline__))
        void setDither(bool enable)
        {
            #if HAVE_NEOPIXELBUS
                // NeoPixelBus has no temporal dithering
                (void)enable;
            #else
                FastLED.setDither(enable ? BINARY_DITHER : DISABLE_DITHER);
            #endif
        }

        void reset()
        {
            PixelBufferType::reset();
            show();
        }

        inline __attribute__((__always_inline__))
        void setBrightness(uint8_t brightness)
        {
            #if HAVE_NEOPIXELBUS
                _brightness = brightness;
            #else
                FastLED.setBrightness(brightness);
            #endif
        }

        // current brightness, the NeoPixelBus build keeps it in the display
        inline __attribute__((__always_inline__))
        uint8_t getBrightness() const
        {
            #if HAVE_NEOPIXELBUS
                return _brightness;
            #else
                return FastLED.getBrightness();
            #endif
        }

        void dump(Print &output)
        {
            PixelBufferType::dump(output, getBrightness());
        }

        inline __attribute__((__always_inline__))
        void show()
        {
            show(getBrightness());
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
            #if !HAVE_NEOPIXELBUS && IOT_LED_MATRIX_HAVE_PANELS
                _fillSegmentBuffers();
            #endif
            #if HAVE_NEOPIXELBUS
                auto type = getNeopixelShowMethodType();
                auto method = (type == ShowMethodType::NEOBUS_I2S) ? NeoBusMethodType::I2S : NeoBusMethodType::RMT;
                if (method != _neoBusMethod) {
                    _neoBusMethod = method;
                    _rebuildStrips();
                }
                switch(type) {
                    case ShowMethodType::NEOBUS_RMT:
                    case ShowMethodType::NEOBUS_I2S:
                        _neoBus.show(__pixels.data(), brightness);
                        break;
                    default:
                        break;
                }
            #elif IOT_LED_MATRIX_FASTLED_ONLY
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
                    default:
                        break;
                }
            #endif
        }

        bool getDither() const
        {
            #if HAVE_NEOPIXELBUS
                return false;
            #else
                return (_controller.getDither() == BINARY_DITHER);
            #endif
        }

        #if HAVE_NEOPIXELBUS
            // frames per second of the NeoPixelBus transport, measured over one second
            inline __attribute__((__always_inline__))
            float getFps() const
            {
                return _neoBus.getFps();
            }

            // segments of the NeoPixelBus transport that are transmitted by the I2S DMA and by the
            // RMT transport (mixed mode, see NeoBusStrips)
            uint8_t getI2sStrips() const
            {
                return _neoBus.getI2sStrips();
            }

            uint8_t getRmtStrips() const
            {
                return _neoBus.getRmtStrips();
            }

            // longest frame of the RMT transport that took longer than the pixels need, in micro
            // seconds. A value above the tolerance means the RMT memory ran empty and data was
            // transmitted twice - the visible flicker of a delayed refill interrupt
            uint32_t getMaxOverrunMicros() const
            {
                return _neoBus.getMaxOverrunMicros();
            }

            // frames of the RMT transport that never finished (the refill interrupt stopped)
            uint32_t getTxTimeouts() const
            {
                return _neoBus.getTxTimeouts();
            }

            // transmitted segments of the RMT transport and how many of them took longer than the
            // pixels need - a flash write during the transmission masks the refill interrupt
            uint32_t getTransmissions() const
            {
                return _neoBus.getTransmissions();
            }

            uint32_t getTransmissionsOver() const
            {
                return _neoBus.getTransmissionsOver();
            }

            // longest time the driver needed to translate and copy a frame into the RMT memory
            uint32_t getMaxWriteMicros() const
            {
                return _neoBus.getMaxWriteMicros();
            }

            // time the pixels of all segments need on the wire, the shortest frame possible
            uint32_t getWireMicros() const
            {
                return _neoBus.getWireMicros();
            }

            // sum of the channel values and the number of LEDs of all active segments, used by the
            // software power limit (the power constants are mW per 256 LEDs, see the FastLED fork's
            // power management)
            void getPowerSum(uint32_t &red, uint32_t &green, uint32_t &blue, uint32_t &count) const
            {
                red = green = blue = count = 0;
                for (uint8_t i = 0; i < MatrixValidation::kMaxSegments; i++) {
                    const uint16_t num = _segPixels[i];
                    if (!num) {
                        continue;
                    }
                    forEachSegmentPixel(__pixels.data(), getOffset(), _getSegmentLut(i), num, [&](uint16_t, const PixelRGB &color) {
                        red += color.r;
                        green += color.g;
                        blue += color.b;
                    });
                    count += num;
                }
            }
        #endif

        uint32_t getNumSegments() const
        {
            return _numSegments;
        }

        void delay(unsigned long ms)
        {
            #if HAVE_NEOPIXELBUS
                ::delay(ms);
            #else
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
            #endif
        }

    protected:
        // LUT of a segment, nullptr if it is transmitted straight from the buffer
        const uint16_t *_getSegmentLut(uint8_t index) const
        {
            #if IOT_LED_MATRIX_HAVE_PANELS
                return _segLut[index].get();
            #else
                return nullptr;
            #endif
        }

        #if IOT_LED_MATRIX_HAVE_PANELS
            // the LEDs of a panel are wired like the matrix: the matrix mapping applied to the panel's own size
            using LocalMappingType = DynamicPixelMapping<0, 1, 1, false, false, false, false>;

            // buffer address of every LED of the enabled segments, see SegmentPanel. A panel that is the
            // identity (LED i = buffer pixel i, the default single panel) gets no LUT and is transmitted
            // straight from the buffer
            void _buildLuts()
            {
                const uint32_t maxPixels = __pixels.size();
                const uint16_t rows = getRows();
                const uint16_t cols = getCols();
                const uint16_t pixelOffset = getOffset();
                LocalMappingType local;
                for (uint8_t i = 0; i < MatrixValidation::kMaxSegments; i++) {
                    _segLut[i].reset();
                    _segPixels[i] = 0;
                    const auto &panel = _segPanel[i];
                    if (!panel.isEnabled()) {
                        continue;
                    }
                    const uint32_t numPixels = panel.numPixels();
                    if (!local.setParams(panel.wiredRows(), panel.wiredCols(), isRowsReversed(), isColsReversed(), isRotated(), isInterleaved(), getRowOfs(), getColOfs())) {
                        __DBG_printf("segment %u: panel %ux%u does not fit into the pixel buffer", i, panel.rows, panel.cols);
                        continue;
                    }
                    std::unique_ptr<uint16_t[]> lut(new (std::nothrow) uint16_t[numPixels]);
                    if (!lut) {
                        __DBG_printf("segment %u: out of memory", i);
                        continue;
                    }
                    // the local mapping yields the LED index of every point of the unrotated panel, the LED
                    // reads the buffer pixel of the matrix point that it covers
                    std::fill(lut.get(), lut.get() + numPixels, SegmentPanel::kBlack);
                    for (uint16_t row = 0; row < panel.wiredRows(); row++) {
                        for (uint16_t col = 0; col < panel.wiredCols(); col++) {
                            uint16_t matrixRow;
                            uint16_t matrixCol;
                            if (!panel.getMatrixPoint(row, col, rows, cols, matrixRow, matrixCol)) {
                                continue;
                            }
                            const uint32_t address = pixelOffset + this->getAddress(matrixRow, matrixCol);
                            if (address < maxPixels) {
                                lut[local.getAddress(row, col)] = address;
                            }
                        }
                    }
                    bool identity = true;
                    for (uint32_t index = 0; index < numPixels && identity; index++) {
                        identity = (lut[index] == pixelOffset + index);
                    }
                    _segPixels[i] = numPixels;
                    if (!identity) {
                        _segLut[i] = std::move(lut);
                    }
                    __LDBG_printf("segment %u: panel %ux%u at %u/%u pixels=%u identity=%u", i, panel.rows, panel.cols, panel.posRow, panel.posCol, numPixels, identity);
                }
            }
        #endif

        #if HAVE_NEOPIXELBUS
            // create/rebuild one strip per output pin, the pins are compile time constants
            void _rebuildStrips()
            {
                uint8_t pins[NeoBusStrips::kMaxStrips];
                pins[0] = IOT_LED_MATRIX_OUTPUT_PIN;
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN1) && IOT_LED_MATRIX_OUTPUT_PIN1 != -1
                    pins[1] = IOT_LED_MATRIX_OUTPUT_PIN1;
                #else
                    pins[1] = 0xff;
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN2) && IOT_LED_MATRIX_OUTPUT_PIN2 != -1
                    pins[2] = IOT_LED_MATRIX_OUTPUT_PIN2;
                #else
                    pins[2] = 0xff;
                #endif
                #if defined(IOT_LED_MATRIX_OUTPUT_PIN3) && IOT_LED_MATRIX_OUTPUT_PIN3 != -1
                    pins[3] = IOT_LED_MATRIX_OUTPUT_PIN3;
                #else
                    pins[3] = 0xff;
                #endif

                // _buildLuts() only keeps panels that fit into the pixel buffer, a static display transmits
                // the matrix on the first pin
                uint16_t counts[NeoBusStrips::kMaxStrips];
                const uint16_t *luts[NeoBusStrips::kMaxStrips];
                for (uint8_t i = 0; i < NeoBusStrips::kMaxStrips; i++) {
                    counts[i] = _segPixels[i];
                    luts[i] = _getSegmentLut(i);
                }
                _neoBus.update(_neoBusMethod, pins, counts, luts, getOffset());
                _numSegments = _neoBus.getNumStrips();
            }

            NeoBusStrips _neoBus;
            uint8_t _brightness;
            NeoBusMethodType _neoBusMethod;
        #else
            #if IOT_LED_MATRIX_HAVE_PANELS
                // pixels of a segment for FastLED/NeoPixelEx: the pixel buffer itself for an identity panel, or
                // a buffer of the panel's size (filled by show() through its LUT, see _fillSegmentBuffers())
                CRGB *_getSegmentLeds(uint8_t index)
                {
                    if (_segLut[index]) {
                        _segBuffer[index].reset(new (std::nothrow) PixelRGB[_segPixels[index]]());
                        if (_segBuffer[index]) {
                            return reinterpret_cast<CRGB *>(_segBuffer[index].get());
                        }
                        __DBG_printf("segment %u: out of memory, transmitting it as identity", index);
                        _segLut[index].reset();
                    }
                    else {
                        _segBuffer[index].reset();
                    }
                    return reinterpret_cast<CRGB *>(__pixels.data() + getOffset());
                }

                // copy the panels into their buffers before they are transmitted
                void _fillSegmentBuffers()
                {
                    for (uint8_t i = 0; i < MatrixValidation::kMaxSegments; i++) {
                        if (!_segBuffer[i]) {
                            continue;
                        }
                        PixelRGB *dst = _segBuffer[i].get();
                        forEachSegmentPixel(__pixels.data(), getOffset(), _segLut[i].get(), _segPixels[i], [dst](uint16_t index, const PixelRGB &color) {
                            dst[index] = color;
                        });
                    }
                }
            #else
                // the matrix of a static display is transmitted straight from the buffer
                CRGB *_getSegmentLeds(uint8_t index)
                {
                    return reinterpret_cast<CRGB *>(__pixels.data() + getOffset());
                }
            #endif

            // the pixel buffer (PixelRGB) is passed to FastLED as CRGB, see updateSegments()
            static_assert(sizeof(CRGB) == sizeof(PixelRGB), "PixelRGB must have the layout of CRGB");
            CRGB _emptyPixel;
            CLEDController &_controller;
        #endif
        uint16_t _segPixels[MatrixValidation::kMaxSegments];
        #if IOT_LED_MATRIX_HAVE_PANELS
            SegmentPanel _segPanel[MatrixValidation::kMaxSegments];
            std::unique_ptr<uint16_t[]> _segLut[MatrixValidation::kMaxSegments];
            #if !HAVE_NEOPIXELBUS
                std::unique_ptr<PixelRGB[]> _segBuffer[MatrixValidation::kMaxSegments];
            #endif
        #endif
        uint32_t _numSegments;
    };

}

#if DEBUG_IOT_CLOCK
#    include <debug_helper_disable.h>
#endif
