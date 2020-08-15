/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <assert.h>
#include <MicrosTimer.h>
#include "GFXCanvasConfig.h"

#include <push_pack.h>

namespace GFXCanvas {

    inline uXType getClippedX(sXType x, uWidthType maxWidth)
    {
        if (x < 0) {
            return 0;
        }
        else if (x >= (sWidthType)maxWidth) {
            return maxWidth - 1;
        }
        return x;
    }

    inline uXType getClippedX(sXType x, uWidthType minWidth, uWidthType maxWidth)
    {
        if (x < (sWidthType)minWidth) {
            x = minWidth;
        }
        if (x >= (sWidthType)maxWidth) {
            return (uXType)(maxWidth - 1);
        }
        return (uXType)x;
    }

    inline uYType getClippedY(sXType y, uHeightType maxHeight)
    {
        if (y < 0) {
            return 0;
        }
        else if (y >= (sHeightType)maxHeight) {
            return (uYType)(maxHeight - 1);
        }
        return (uYType)y;
    }

    inline uYType getClippedY(sXType y, uHeightType minHeight, uHeightType maxHeight)
    {
        if (y < (sHeightType)minHeight) {
            y = minHeight;
        }
        if (y >= (sHeightType)maxHeight) {
            return (uYType)(maxHeight - 1);
        }
        return (uYType)y;
    }

    inline uPositionType getClippedXY(sXType x, sYType y, uWidthType maxWidth,uHeightType maxHeight)
    {
        return uPositionType({ getClippedX(x, maxWidth), getClippedY(y, maxHeight) });
    }

    inline uPositionType getClippedXY(sXType x, sYType y, uWidthType minWidth, uWidthType maxWidth, uHeightType minHeight, uHeightType maxHeight)
    {
        return uPositionType({ getClippedX(x, minWidth, maxWidth), getClippedY(y, minHeight, maxHeight) });
    }

    inline bool isValidX(sXType x, uWidthType width) {
        return ((uXType)x < width);
    }
    inline bool isValidX(sXType x, sXType startX, sXType endX) {
        return (x >= startX) && (x < endX);
    }

    inline bool isValidY(sYType y, uHeightType height) {
        return ((uYType)y < height);
    }
    inline bool isValidY(sYType y, sYType startY, sYType endY) {
        return (y >= startY) && (y < endY);
    }

    inline bool isValidXY(sXType x, sXType y, uWidthType width, uHeightType height) {
        return isValidX(x, width) && isValidY(y, height);
    }

    inline bool isFullLine(sXType startX, sXType endX, uWidthType width) {
        return startX <= 0 && endX >= width;
    }


    inline void convertToRGB(ColorType color, uint8_t& r, uint8_t& g, uint8_t& b)
    {
        r = ((color >> 11) * 527 + 23) >> 6;
        g = (((color >> 5) & 0x3f) * 259 + 33) >> 6;
        b = ((color & 0x1f) * 527 + 23) >> 6;
    }

    inline RGBColorType convertToRGB(ColorType color)
    {
        uint8_t r, g, b;
        convertToRGB(color, r, g, b);
        return (r << 16) | (g << 8) | b;
    }

    inline ColorType convertRGBtoRGB565(uint8_t r, uint8_t g, uint8_t b)
    {
        return ((b >> 3) & 0x1f) | (((g >> 2) & 0x3f) << 5) | (((r >> 3) & 0x1f) << 11);
    }

    inline ColorType convertRGBtoRGB565(RGBColorType rgb)
    {
        return convertRGBtoRGB565(rgb, rgb >> 8, rgb >> 16);
    }

    class ColorPalette
    {
    public:
        static constexpr size_t kColorsMax = 15;
        static constexpr ColorType kInvalid = ~0;

        ColorPalette();

        ColorType &at(int index);
        ColorType at(ColorType index) const;
        ColorType &operator[](int index);
        ColorType operator[](ColorType index) const;

        bool empty() const;
        size_t length() const;

        void clear();
        ColorType *begin();
        ColorType *end();
        const ColorType *begin() const;
        const ColorType *end() const;

        // returns index or -1
        int getColorIndex(ColorType color) const;
        // add color if it does not exist and return index
        // returns index of existing color
        // returns -1 if there is no space to add more
        int addColor(ColorType color);

        const uint8_t *getBytes() const;
        size_t getBytesLength() const;

    private:
        size_t size() const;

    private:
        uint16_t _count;
        ColorType _palette[kColorsMax];
    };

};

#include <pop_pack.h>
