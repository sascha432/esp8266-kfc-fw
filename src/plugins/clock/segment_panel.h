/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

namespace Clock {

    // Panel geometry of one segment (one output pin).
    //
    // Every segment is a panel of rows x cols LEDs (as it is mounted, a strip is a panel with 1 row) placed
    // at posRow/posCol of the matrix (top row / left column). The LEDs of the panel are wired like the matrix
    // options say (reverse rows/columns, rotation, interleaved) - PixelDisplay::_buildLuts() applies the
    // matrix mapping to the panel's own size to get the position of every LED on the unrotated panel.
    // rotation turns the mounted panel clockwise in quarter turns (90/270 degrees swap its axes: an 8x32
    // panel standing upright is entered as 32 rows x 8 columns), mirror swaps left and right afterwards.
    //
    // The transport reads every LED from the matrix point it covers: _buildLuts() translates that once into
    // buffer addresses, the transport only does one indexed load per LED. A panel that covers the whole
    // matrix unrotated yields the identity (LED i = buffer pixel i) and is transmitted without a LUT.
    struct SegmentPanel {
        static constexpr uint16_t kBlack = 0xffff; // LUT entry of an LED outside of the matrix

        SegmentPanel(bool _enabled = false, uint16_t _rows = 1, uint16_t _cols = 1, uint16_t _posRow = 0, uint16_t _posCol = 0, uint8_t _rotation = 0, bool _mirror = false) :
            enabled(_enabled),
            rows(_rows),
            cols(_cols),
            posRow(_posRow),
            posCol(_posCol),
            rotation(_rotation),
            mirror(_mirror)
        {
        }

        bool isEnabled() const
        {
            return enabled && rows && cols;
        }

        uint32_t numPixels() const
        {
            return static_cast<uint32_t>(rows) * cols;
        }

        // size of the panel before it is rotated, the LEDs are wired in that orientation
        uint16_t wiredRows() const
        {
            return (rotation & 1) ? cols : rows;
        }

        uint16_t wiredCols() const
        {
            return (rotation & 1) ? rows : cols;
        }

        // matrix point of the LED at (wiredRow, wiredCol) of the unrotated panel, false if it is outside the matrix
        bool getMatrixPoint(uint16_t wiredRow, uint16_t wiredCol, uint16_t matrixRows, uint16_t matrixCols, uint16_t &matrixRow, uint16_t &matrixCol) const
        {
            uint16_t row;
            uint16_t col;
            switch(rotation) {
                case 1: // 90 degrees clockwise
                    row = wiredCol;
                    col = (wiredRows() - 1) - wiredRow;
                    break;
                case 2: // 180
                    row = (wiredRows() - 1) - wiredRow;
                    col = (wiredCols() - 1) - wiredCol;
                    break;
                case 3: // 270
                    row = (wiredCols() - 1) - wiredCol;
                    col = wiredRow;
                    break;
                default:
                    row = wiredRow;
                    col = wiredCol;
                    break;
            }
            if (mirror) {
                col = (cols - 1) - col;
            }
            const uint32_t mr = static_cast<uint32_t>(posRow) + row;
            const uint32_t mc = static_cast<uint32_t>(posCol) + col;
            if (mr >= matrixRows || mc >= matrixCols) {
                return false;
            }
            matrixRow = mr;
            matrixCol = mc;
            return true;
        }

        bool enabled;
        uint16_t rows;      // size as the panel is mounted
        uint16_t cols;
        uint16_t posRow;    // placement in the matrix: top row / left column
        uint16_t posCol;
        uint8_t rotation;   // quarter turns clockwise (0-3)
        bool mirror;
    };

    // calls fn(index, color) for every LED of a segment: straight from the pixel buffer (identity, lut is
    // nullptr) or through the LUT of the panel (PixelDisplay::_buildLuts())
    template<typename _PixelType, typename _Fn>
    inline __attribute__((__always_inline__))
    void forEachSegmentPixel(const _PixelType *pixels, uint16_t pixelOffset, const uint16_t *lut, uint16_t count, _Fn &&fn)
    {
        if (lut) {
            const _PixelType black = _PixelType();
            for (uint16_t i = 0; i < count; i++) {
                const uint16_t address = lut[i];
                fn(i, (address == SegmentPanel::kBlack) ? black : pixels[address]);
            }
        }
        else {
            pixels += pixelOffset;
            for (uint16_t i = 0; i < count; i++) {
                fn(i, pixels[i]);
            }
        }
    }

}
