/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <assert.h>
#include <MicrosTimer.h>
#include "GFXCanvasConfig.h"
#include <bitmap_header.h>

#include <push_pack.h>

#if DEBUG_GFXCANVAS
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

// --------------------------------------------------------------------
// BMP Version 3 (Microsoft Windows NT)
// File header union
// --------------------------------------------------------------------

#include "bitmap_header.h"

namespace GFXCanvas {

    // --------------------------------------------------------------------
    // Some helper functions
    // --------------------------------------------------------------------

    inline uint16_t getIconWidth(const char *icon)
    {
        return (pgm_read_byte(icon + 2) << 8) | pgm_read_byte(icon + 3);
    }

    inline uint16_t getIconHeight(const char *icon)
    {
        return (pgm_read_byte(icon + 4) << 8) | pgm_read_byte(icon + 5);
    }

    inline uint16_t getBitmapWidth(const char *icon)
    {
        return getIconWidth(icon);
    }

    inline uint16_t getBitmapHeight(const char *icon)
    {
        return getIconHeight(icon);
    }

    inline uXType getClippedX(sXType x, uWidthType maxWidth)
    {
        return std::clamp<sXType>(x, 0, maxWidth - 1);
        // if (x < 0) {
        //     return 0;
        // }
        // else if (x >= static_cast<sWidthType>(maxWidth)) {
        //     return static_cast<uXType>(maxWidth - 1);
        // }
        // return static_cast<uXType>(x);
    }

    inline uXType getClippedX(sXType x, uWidthType minWidth, uWidthType maxWidth)
    {
        return std::clamp<sXType>(x, minWidth, maxWidth - 1);
        // if (x < static_cast<sWidthType>(minWidth)) {
        //     x = minWidth;
        // }
        // if (x >= static_cast<sWidthType>(maxWidth)) {
        //     return static_cast<uXType>(maxWidth - 1);
        // }
        // return static_cast<uXType>(x);
    }

    inline uYType getClippedY(sYType y, uHeightType maxHeight)
    {
        return std::clamp<sYType>(y, 0, maxHeight - 1);
        // if (y < 0) {
        //     return 0;
        // }
        // else if (y >= static_cast<sHeightType>(maxHeight)) {
        //     return static_cast<uYType>(maxHeight - 1);
        // }
        // return static_cast<uYType>(y);
    }

    inline uYType getClippedY(sYType y, uHeightType minHeight, uHeightType maxHeight)
    {
        return std::clamp<sYType>(y, minHeight, maxHeight - 1);
        // if (y < static_cast<sHeightType>(minHeight)) {
        //     y = minHeight;
        // }
        // if (y >= static_cast<sHeightType>(maxHeight)) {
        //     return static_cast<uYType>(maxHeight - 1);
        // }
        // return static_cast<uYType>(y);
    }

    inline uPositionType getClippedXY(sXType x, sYType y, uWidthType maxWidth,uHeightType maxHeight)
    {
        return uPositionType({ getClippedX(x, maxWidth), getClippedY(y, maxHeight) });
    }

    inline uPositionType getClippedXY(sXType x, sYType y, uWidthType minWidth, uWidthType maxWidth, uHeightType minHeight, uHeightType maxHeight)
    {
        return uPositionType({ getClippedX(x, minWidth, maxWidth), getClippedY(y, minHeight, maxHeight) });
    }

    inline bool isValidX(sXType x, uWidthType width)
    {
        return (static_cast<uXType>(x) < width);
    }

    inline bool isValidX(sXType x, sXType startX, sXType endX)
    {
        return (x >= startX) && (x < endX);
    }

    inline bool isValidY(sYType y, uHeightType height)
    {
        return (static_cast<uYType>(y) < height);
    }

    inline bool isValidY(sYType y, sYType startY, sYType endY)
    {
        return (y >= startY) && (y < endY);
    }

    inline bool isValidXY(sXType x, sXType y, uWidthType width, uHeightType height)
    {
        return isValidX(x, width) && isValidY(y, height);
    }

    inline bool isFullLine(sXType startX, sXType endX, uWidthType width)
    {
        return startX <= 0 && endX >= width;
    }

    // --------------------------------------------------------------------
    // Color conversions
    // RGB565 <-> RGB555
    // RGB565 <-> RGB24
    // --------------------------------------------------------------------

    inline void convertRGB565ToRGB(ColorType color, uint8_t& r, uint8_t& g, uint8_t& b)
    {
        r = ((color >> 11) * 527 + 23) >> 6;
        g = (((color >> 5) & 0x3f) * 259 + 33) >> 6;
        b = ((color & 0x1f) * 527 + 23) >> 6;
    }

    inline RGBColorType convertRGB565ToRGB(ColorType color)
    {
        uint8_t r, g, b;
        convertRGB565ToRGB(color, r, g, b);
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

    inline uint8_t convertRGB565toRGB555LowByte(ColorType color)
    {
        return (color & 0x1f) | ((color & 0xc0) >> 1) | ((color >> 8) << 7);
    }

    inline uint8_t convertRGB565toRGB555HighByte(ColorType color)
    {
        return color >> 9;
    }

    // --------------------------------------------------------------------
    // typedef struct _Win3xPaletteElement
    // --------------------------------------------------------------------

    struct BitmapPaletteColorType {
        union {
            struct {
                uint8_t blue;
                uint8_t green;
                uint8_t red;
                uint8_t __reserved;
            };
            uint32_t rgb; // dword aligned and zero filled
        };

        BitmapPaletteColorType();
        BitmapPaletteColorType(ColorType rgb565);
    };

    // check if structures have the correct size
    static_assert(sizeof(BitmapPaletteColorType) == sizeof(uint32_t), "Invalid size");


    inline BitmapPaletteColorType::BitmapPaletteColorType() :
        rgb(0)
    {
    }

    inline BitmapPaletteColorType::BitmapPaletteColorType(ColorType rgb565) :
        rgb(convertRGB565ToRGB(rgb565))
    {
    }

    // --------------------------------------------------------------------
    // BMP Version 3 (Microsoft Windows NT)
    // Class to access the file header and RGB color palette as byte stream
    // --------------------------------------------------------------------

    class BitmapHeaderType {
    public:
        static constexpr size_t getHeaderSize() {
            return sizeof(BitmapFileHeaderType);
        }

    public:
        BitmapHeaderType();
        BitmapHeaderType(int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors);
        ~BitmapHeaderType();

        // update is faster than using operators
        void update(int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors);

        BitmapHeaderType &operator=(BitmapHeaderType &&hdr) = delete;
        BitmapHeaderType &operator=(const BitmapHeaderType &hdr) = delete;

        // get single byte from header or palette
        uint8_t getHeaderAt(uint16_t index) const;
        uint8_t getPaletteAt(uint16_t index) const;

        // number of colors in the palette
        uint16_t getNumPaletteColors() const;

        // set single palette color
        void setPaletteColor(uint16_t index, ColorType color);

        // get the bitmap file header
        BitmapFileHeaderType &getBitmapFileHeader();

        uint32_t getHeaderAndPaletteSize() const;
        uint32_t getBitCount() const;
        void setBfSize(uint32_t size);

    private:
        BitmapFileHeaderType _fileHeader;
        decltype(_fileHeader.h) &_header;
        BitmapPaletteColorType *_palette;
    };

    inline BitmapHeaderType::BitmapHeaderType() :
        _fileHeader(),
        _header(_fileHeader.h),
        _palette(nullptr)
    {
    }

    inline void BitmapHeaderType::update(int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors)
    {
        this->~BitmapHeaderType();
        new(static_cast<void *>(this)) BitmapHeaderType(width, height, bits, numPaletteColors); // place new header
    }

    inline BitmapHeaderType::BitmapHeaderType(int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors) :
        BitmapHeaderType()
    {
        _header.bfh.bfType = 'B' | ('M' << 8);
        _header.bfh.bfReserved2 = sizeof(_header);
        _header.bih.biSize = sizeof(_header.bih);
        _header.bih.biPlanes = 1;
        _header.bih.biWidth = width;
        _header.bih.biHeight = -height; // negative means top to bottom
        _header.bih.biBitCount = bits;
        _header.bih.biClrUsed = numPaletteColors;

        #if DEBUG_GFXCANVAS
            if (bits == 4) {
                if (numPaletteColors < 1 || numPaletteColors > 16) {
                    __DBG_panic("bits=4 palette=%u", numPaletteColors);
                }
            }
            else if (bits == 16) {
                if (numPaletteColors) {
                    __DBG_panic("bits=16 palette=%u", numPaletteColors);
                }
            }
            else {
                __DBG_panic("bits=%u not supported", bits);
            }
        #endif

        if (_header.bih.biClrUsed) {
            _palette = new BitmapPaletteColorType[_header.bih.biClrUsed]();
            if (!_palette) {
                __LDBG_printf("cannot allocate palette memory=%u", _header.bih.biClrUsed);
                // send invalid image if there is not enough memory
                _header.bih.biClrUsed = 0;
                _header.bih.biWidth = 0;
                _header.bih.biHeight = 0;
            }
        }
    }

    inline BitmapHeaderType::~BitmapHeaderType()
    {
        if (_palette) {
            delete[] _palette;
            _palette = nullptr;
        }
    }

    inline uint16_t BitmapHeaderType::getNumPaletteColors() const
    {
        return _header.bih.biClrUsed;
    }

    inline void BitmapHeaderType::setPaletteColor(uint16_t index, ColorType color)
    {
        _palette[index] = BitmapPaletteColorType(color);
    }

    inline uint8_t BitmapHeaderType::getHeaderAt(uint16_t index) const
    {
        return _fileHeader.b[index];
    }

    inline uint8_t BitmapHeaderType::getPaletteAt(uint16_t index) const
    {
        #if DEBUG_GFXCANVAS
            const uint8_t byteIndex = index % sizeof(BitmapPaletteColorType);
            const uint16_t paletteIndex = index / sizeof(BitmapPaletteColorType);
            if (byteIndex == 0) {
                __DBG_printf("i=%u n=%u(%u) #%02x%02x%02x", index, paletteIndex, byteIndex, _palette[paletteIndex].red, _palette[paletteIndex].green, _palette[paletteIndex].blue);
            }
            return reinterpret_cast<uint8_t *>(&_palette[paletteIndex])[byteIndex];
        #else
            return reinterpret_cast<uint8_t *>(_palette)[index];
        #endif
    }

    inline BitmapFileHeaderType &BitmapHeaderType::getBitmapFileHeader()
    {
        return _fileHeader;
    }

    inline uint32_t BitmapHeaderType::getHeaderAndPaletteSize() const
    {
        return getHeaderSize() + (_header.bih.biClrUsed * sizeof(BitmapPaletteColorType));
    }

    inline uint32_t BitmapHeaderType::getBitCount() const
    {
        return _header.bih.biBitCount;
    }

    inline void BitmapHeaderType::setBfSize(uint32_t size)
    {
        _header.bfh.bfSize = size;
    }

}

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif

#include <pop_pack.h>
