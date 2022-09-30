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

        void update(BitmapHeaderType &header, int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors)
        {
            header.~BitmapHeaderType();
            new(static_cast<void *>(&header)) BitmapHeaderType(width, height, bits,numPaletteColors);
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


    // --------------------------------------------------------------------
    // ColorPalette for canvas objects with out a palette
    // --------------------------------------------------------------------

    class ColorPalette {
    public:
        static constexpr size_t kColorsMax = 65536;

    public:
        ColorPalette() : _palette(nullptr), _numColors(0) {}
        virtual ~ColorPalette() {}

        virtual size_t length() const;
        virtual size_t size() const;

        virtual void fillPaletteColors(BitmapHeaderType &_fileHeader) {}

    protected:
        BitmapPaletteColorType *_palette;
        uint16_t _numColors;
    };

    inline size_t ColorPalette::size() const
    {
        return kColorsMax;
    }

    inline size_t ColorPalette::length() const
    {
        return 0;
    }

    // --------------------------------------------------------------------
    // ColorPalette for canvas objects with 16 colors
    // --------------------------------------------------------------------

    class ColorPalette16 : public ColorPalette
    {
    public:
        static constexpr size_t kColorsMax = 16;
        static constexpr ColorType kInvalid = ~0;
        static constexpr int kInvalidIndexResult = -1;

    public:
        ColorPalette16();

        ColorType &at(int index);
        ColorType at(ColorType index) const;
        ColorType &operator[](int index);
        ColorType operator[](ColorType index) const;

        virtual size_t length() const override;
        virtual size_t size() const override;

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

        virtual void fillPaletteColors(BitmapHeaderType &_fileHeader) override;

    private:
        ColorType _palette[kColorsMax];
        size_t _count;
    };

    inline ColorPalette16::ColorPalette16() :
        _palette{},
        _count(0)
    {
        __LDBG_printf("new");
    }

    inline ColorType &ColorPalette16::at(int index)
    {
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assertp((unsigned)index < (unsigned)_count, "index=%d count=%d", index, _count), return _palette[0]);
        index = (unsigned)index % _count;
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert((unsigned)index < (unsigned)_count), return _palette[0]);
        return _palette[index];
    }

    inline ColorType ColorPalette16::at(ColorType index) const
    {
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assertp((unsigned)index < (unsigned)_count, "index=%d count=%d", index, _count), return _palette[0]);
        if (_count == 0) {
            return 0;
        }
        index = (unsigned)index % _count;
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert((unsigned)index < (unsigned)_count), return _palette[0]);
        return _palette[index];
    }

    inline ColorType &ColorPalette16::operator[](int index)
    {
        return at(index);
    }

    inline ColorType ColorPalette16::operator[](ColorType index) const
    {
        return at(index);
    }

    inline void ColorPalette16::clear()
    {
        // set _count to 0 as well
        _count = 0;
        std::fill_n(begin(), size(), 0);
    }

    inline ColorType *ColorPalette16::begin()
    {
        return &_palette[0];
    }

    inline ColorType *ColorPalette16::end()
    {
        return &_palette[_count];
    }

    inline const ColorType *ColorPalette16::begin() const
    {
        return &_palette[0];
    }

    inline const ColorType *ColorPalette16::end() const
    {
        return &_palette[_count];
    }

    inline size_t ColorPalette16::length() const
    {
        return _count;
    }

    inline size_t ColorPalette16::size() const
    {
        return kColorsMax;
    }

    inline void ColorPalette16::fillPaletteColors(BitmapHeaderType &_fileHeader)
    {
        auto dst = _fileHeader.getPalleteColorDataPtr();
        for(const auto color: *this) {
            *dst++ = BitmapPaletteColorType(color);
        }
    }

}

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif


#include <pop_pack.h>
