/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvas.h"
#include "GFXCanvasBitmapStream.h"
#include "GFXCanvasCompressed.h"

#pragma GCC push_options
#if DEBUG_GFXCANVAS
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#    pragma GCC optimize("O3")
#endif

using namespace GFXCanvas;

GFXCanvasBitmapStream::GFXCanvasBitmapStream(GFXCanvasCompressed &canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h) :
    _canvas(canvas),
    _cache(canvas.width(), Cache::kYInvalid),
    _position(0),
    _x(x),
    _y(y),
    _width(w),
    _height(h)
{
    __LDBG_printf("init x,y,w,h=%d,%d,%d,%d", _x, _y, _width, _height);
    _createHeader();
}

GFXCanvasBitmapStream::GFXCanvasBitmapStream(GFXCanvasCompressed &canvas) :
    GFXCanvasBitmapStream(canvas, 0, 0, canvas.width(), canvas.height())
{
}

int GFXCanvasBitmapStream::read()
{
    if (_available) {
        _available--;
        if (_position < _header.getHeaderSize()) {
            __LDBG_printf("header %d/%d avail=$d", _position, _header.getHeaderSize(), _available);
            return _header.getHeaderAt(_position++);
        }
        #if GFXCANVAS_SUPPORT_4BIT_BMP
            else if (_position < _header.getHeaderAndPaletteSize()) {
                __LDBG_printf("palette %d/%d avail=%d", _position - _header.getHeaderSize(), _header.getHeaderAndPaletteSize() - _header.getHeaderSize(), _available);
                return _header.getPaletteAt(_position++);
            }
        #endif
        else {
            // sending line by line

            uint16_t imagePos = _position++ - _header.getHeaderAndPaletteSize();
            uint16_t perLine = _width * 2;
            uint16_t y = (imagePos / perLine) + _y;
            uint16_t x = ((imagePos % perLine) / 2) + _x;

            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _canvas.height()), return 0);
            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sx(x, _canvas.width()), return 0);

            if (!_cache.isY(y)) {
                _cache.setY(y);
                _canvas._decodeLine(_cache);
            }

            auto color = _cache.at(x);
            #if GFXCANVAS_SUPPORT_4BIT_BMP
                if (_header.h.bih.biBitCount == 4) {
                    //TODO this is not working

                    auto &canvas4bit = reinterpret_cast<ColorPalette16 &>(_canvas);
                    uint8_t byte = canvas4bit.getColorIndex(x) << 4;

                    imagePos = _position++ - (sizeof(_header) + bitmapColorPaletteSizeInBytes()); // maximum length is limited by _available
                    perLine = _width * 2;
                    y = (imagePos / perLine) + _y;
                    x = ((imagePos % perLine) / 2) + _x;

                    if (!_cache.isY(y)) {
                        _cache.setY(y);
                        _canvas._decodeLine(_cache);
                    }

                    color = _cache.at(x);
                    byte |= canvas4bit.getColorIndex(x);
                    return byte;

                }
                else
            #endif
            {
                // convert rgb565 to rgb555
                if (imagePos % 2 == 0) {    // return low byte
                    return (color & 0x1f) | ((color & 0xc0) >> 1) | ((color >> 8) << 7);
                }
                else {  // return high byte
                    return color >> 9;
                }
            }
        }

    }
    return -1;
}

void GFXCanvasBitmapStream::_createHeader()
{
    uint8_t bits = 16; // RGB555
    uint16_t numPaletteColors = 0;

    #if GFXCANVAS_SUPPORT_4BIT_BMP
        const auto *palette = _canvas.getPalette();
        __LDBG_printf("palette=%p size=%d", palette, palette ? palette->length() : -1);
        if (palette) {
            if (palette->size() == ColorPalette16::kColorsMax) {
                bits = 4;
                numPaletteColors = palette->length();
                _windowsPalette = GFXCanvas::WindowsColorPalette(*palette);
            }
            else {
                __DBG_panic("only 4bit palettes are supported");
            }
        }
    #endif

    _header.update(_header, _width, _height, bits, numPaletteColors);

    // _available = (((_width * _header.h.bih.biBitCount) + 31) / 32) * sizeof(uint32_t); // align to 32bit
    // _available *= _height; // multiply with height
    // _available += sizeof(_header.h); // add header size
    // _available += bitmapColorPaletteSizeInBytes();

    // __LDBG_printf("bits=%p available=%u p_size=%u", _header.h.bih.biBitCount, _available, bitmapColorPaletteSizeInBytes());

    // _header.bfh.bfSize = _available;

}

#pragma GCC pop_options
