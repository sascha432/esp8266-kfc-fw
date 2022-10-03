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
        if (x < 0) {
            return 0;
        }
        else if (x >= static_cast<sWidthType>(maxWidth)) {
            return static_cast<uXType>(maxWidth - 1);
        }
        return static_cast<uXType>(x);
    }

    inline uXType getClippedX(sXType x, uWidthType minWidth, uWidthType maxWidth)
    {
        if (x < static_cast<sWidthType>(minWidth)) {
            x = minWidth;
        }
        if (x >= static_cast<sWidthType>(maxWidth)) {
            return static_cast<uXType>(maxWidth - 1);
        }
        return static_cast<uXType>(x);
    }

    inline uYType getClippedY(sXType y, uHeightType maxHeight)
    {
        if (y < 0) {
            return 0;
        }
        else if (y >= static_cast<sHeightType>(maxHeight)) {
            return static_cast<uYType>(maxHeight - 1);
        }
        return static_cast<uYType>(y);
    }

    inline uYType getClippedY(sXType y, uHeightType minHeight, uHeightType maxHeight)
    {
        if (y < static_cast<sHeightType>(minHeight)) {
            y = minHeight;
        }
        if (y >= static_cast<sHeightType>(maxHeight)) {
            return static_cast<uYType>(maxHeight - 1);
        }
        return static_cast<uYType>(y);
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
    // --------------------------------------------------------------------

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

    // --------------------------------------------------------------------
    // typedef struct _Win3xPaletteElement
    // --------------------------------------------------------------------

    struct BitmapPaletteColorType {
        union  {
            struct {
                uint8_t blue;
                uint8_t green;
                uint8_t red;
                uint8_t __reserved;
            };
            uint32_t rawBGR; // dword aligned and zero filled
        };

        BitmapPaletteColorType();
        BitmapPaletteColorType(ColorType rgb565);

        uint32_t getGBRValue() const;
    };

    static_assert(sizeof(BitmapPaletteColorType) == sizeof(uint32_t), "Invalid size");


    inline BitmapPaletteColorType::BitmapPaletteColorType() : rawBGR(0)
    {
    }

    inline BitmapPaletteColorType::BitmapPaletteColorType(ColorType rgb565) : BitmapPaletteColorType()
    {
        convertToRGB(rgb565, red, green, blue);
    }

    inline uint32_t BitmapPaletteColorType::getGBRValue() const
    {
        return rawBGR;
    }

    // --------------------------------------------------------------------
    // BMP Version 3 (Microsoft Windows NT)
    // File header union
    // --------------------------------------------------------------------

    union BitmapFileHeaderType {
        struct _bitmapHeader {
            BITMAPFILEHEADER bfh;
            BITMAPINFOHEADER bih;
        } h;
        uint8_t b[sizeof(struct _bitmapHeader)];

        BitmapFileHeaderType() : b{}
        {}
    };

    static_assert(sizeof(BITMAPFILEHEADER) == 14, "Invalid size");
    static_assert(sizeof(BITMAPINFOHEADER) == 40, "Invalid size");

    // --------------------------------------------------------------------
    // BMP Version 3 (Microsoft Windows NT)
    // Class to access the file header and color palette
    // --------------------------------------------------------------------

    class BitmapHeaderType {
    public:

        static constexpr size_t getHeaderSize() {
            return sizeof(BitmapFileHeaderType);
        }

        uint32_t getHeaderAndPaletteSize() const {
            return _header.bfh.bfSize;
        }

        uint32_t getBfSize() const {
            return _header.bfh.bfSize;
        }

    public:

        BitmapHeaderType() :
            _fileHeader(),
            _header(_fileHeader.h),
            _palette(nullptr)
        {
        }

        BitmapHeaderType(int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors) :
            _fileHeader(),
            _header(_fileHeader.h),
            _palette(nullptr)
        {
            _header.bfh.bfType = 'B' | ('M' << 8);
            _header.bfh.bfReserved2 = sizeof(_header);
            _header.bih.biSize = sizeof(_header.bih);
            _header.bih.biPlanes = 1;
            _header.bih.biWidth = width;
            _header.bih.biHeight = -height; // negative means top to bottom
            _header.bih.biBitCount = bits;
            _header.bih.biClrUsed = numPaletteColors;

            _palette = new BitmapPaletteColorType[_header.bih.biClrUsed]();
            if (!_palette) {
                __LDBG_printf("cannot allocate memory for the ");
                _header.bih.biClrUsed = 0;
            }

            _header.bfh.bfSize = sizeof(_header) + (_header.bih.biClrUsed * sizeof(BitmapPaletteColorType));
        }
        ~BitmapHeaderType();


        BitmapHeaderType &operator=(BitmapHeaderType &&src)
        {
            new(static_cast<void *>(this)) BitmapHeaderType(src._header.bih.biWidth, src._header.bih.biHeight, src._header.bih.biBitCount, src._header.bih.biClrUsed);
            std::swap(_palette, src._palette);
            return *this;
        }

        // get single byte from header or palette
        uint8_t getHeaderAt(uint16_t index) const;
        uint8_t getPaletteAt(uint16_t index) const;

        // number of colors in the palette
        uint16_t getNumPaletteColors() const;
        // data pointer to the palette
        BitmapPaletteColorType *getPalleteColorDataPtr();

        // set single palette color
        void setPaletteColor(uint16_t index, ColorType color);

        // get the bitmap file header
        BitmapFileHeaderType &getBitmapFileHeader();

    private:
        BitmapFileHeaderType _fileHeader;
        decltype(_fileHeader.h) &_header;
        BitmapPaletteColorType *_palette;
    };

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

    inline BitmapPaletteColorType *BitmapHeaderType::getPalleteColorDataPtr()
    {
        return _palette;
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
        return reinterpret_cast<uint8_t *>(_palette)[index - getHeaderSize()];
    }

    inline BitmapFileHeaderType &BitmapHeaderType::getBitmapFileHeader()
    {
        return _fileHeader;
    }

}

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif


#include <pop_pack.h>
