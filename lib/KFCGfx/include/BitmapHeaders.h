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
            uint8_t bytes[4];
        };

        BitmapPaletteColorType();
        BitmapPaletteColorType(ColorType rgb565);

        uint32_t getBGRValue() const;
        void set(uint8_t pos, uint8_t value);
    };

    static_assert(sizeof(BitmapPaletteColorType) == sizeof(uint32_t), "Invalid size");


    inline BitmapPaletteColorType::BitmapPaletteColorType() : rawBGR(0)
    {
    }

    inline BitmapPaletteColorType::BitmapPaletteColorType(ColorType rgb565) : BitmapPaletteColorType()
    {
        convertToRGB(rgb565, red, green, blue);
    }

    inline uint32_t BitmapPaletteColorType::getBGRValue() const
    {
        return rawBGR;
    }

    inline void BitmapPaletteColorType::set(uint8_t pos, uint8_t value)
    {
        bytes[pos] = value;
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
        BitmapHeaderType(int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors);
        ~BitmapHeaderType();

        BitmapHeaderType &operator=(BitmapHeaderType &&bm)
        {
            new(static_cast<void *>(this)) BitmapHeaderType(
                bm.getBitmapFileHeader().h.bih.biWidth,
                bm.getBitmapFileHeader().h.bih.biHeight,
                bm.getBitmapFileHeader().h.bih.biBitCount,
                bm.getBitmapFileHeader().h.bih.biClrUsed
            );
            bm.~BitmapHeaderType();
            return *this;
        }

        static constexpr size_t getHeaderSize() {
            return sizeof(BitmapFileHeaderType);
        }

        size_t getPaletteSize() const {
            return _header.bih.biClrUsed * sizeof(BitmapPaletteColorType);
        }

        size_t getHeaderAndPaletteSize() const {
            return getHeaderSize() + getPaletteSize();
        }

        size_t getBitmapDataOffset() const {
            return getHeaderAndPaletteSize();
        }

        size_t getBfSize() const {
            return _header.bfh.bfSize;
        }

        uint8_t getBits() const {
            return _header.bih.biBitCount;
        }

        BitmapHeaderType() :
            _bitmapFileHeader(),
            _header(_bitmapFileHeader.h),
            _palette(nullptr)
        {
        }


        // // get single byte from header or palette
        uint8_t getHeaderAt(uint16_t index) const;
        uint8_t getPaletteAt(uint16_t index) const;

        // get the bitmap file header
        BitmapFileHeaderType &getBitmapFileHeader();
        BitmapPaletteColorType *getColorPalette();

    private:
        BitmapFileHeaderType _bitmapFileHeader;
        decltype(_bitmapFileHeader.h) &_header;
        BitmapPaletteColorType *_palette;
    };


    inline BitmapHeaderType::BitmapHeaderType(int32_t width, int32_t height, uint8_t bits, uint16_t numPaletteColors) :
        _bitmapFileHeader(),
        _header(_bitmapFileHeader.h),
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
            _header.bih.biClrUsed = 0;
        }
        if (height < 0) {
            height = -height;
        }

        _header.bfh.bfSize = getHeaderSize() + getPaletteSize();
        if (bits == 4) {
            _header.bfh.bfSize += (width * 2) * height; // 116vit alignment
        }
        else {
            //TODO
            _header.bfh.bfSize += ((((width / 2) + 1) * 2) / 2) * height; // 8 bit alignment
            // _header.bfh.bfSize += ((((width / 2) + 7) * 8) / 8) * height; // 32 bit alignment
        }
    }

    inline BitmapHeaderType::~BitmapHeaderType()
    {
        if (_palette) {
            delete[] _palette;
            _palette = nullptr;
        }
    }

    inline uint8_t BitmapHeaderType::getHeaderAt(uint16_t index) const
    {
        return _bitmapFileHeader.b[index];
    }

    inline uint8_t BitmapHeaderType::getPaletteAt(uint16_t index) const
    {
        return reinterpret_cast<uint8_t *>(_palette)[index];
    }

    inline BitmapFileHeaderType &BitmapHeaderType::getBitmapFileHeader()
    {
        return _bitmapFileHeader;
    }

}


#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif

#include <pop_pack.h>
