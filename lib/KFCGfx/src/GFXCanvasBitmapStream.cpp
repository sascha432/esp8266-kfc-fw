/**
* Author: sascha_lammers@gmx.de
*/

// http://www.daubnet.com/en/file-format-bmp

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvas.h"
#include "GFXCanvasBitmapStream.h"
#include "GFXCanvasCompressed.h"
#include "GFXCanvasCompressedPalette.h"

#pragma GCC push_options
#pragma GCC optimize("O3")

#if DEBUG_GFXCANVAS
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
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
    _createHeader();
    #if DEBUG_GFXCANVAS
    __DBG_printf("init x,y,w,h,p=%d,%d,%d,%d,%d,b=%u", _x, _y, _width, _height, _perLine, _header.getBitCount());
    #endif
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
            return _header.getHeaderAt(_position++);
        }
        #if GFXCANVAS_SUPPORT_4BIT_BMP // only 4 bit palettes are supported, so just skip the code if compiled for 16bit RGB
            else if (_position < _header.getHeaderAndPaletteSize()) {
                return _header.getPaletteAt(_position++ - _header.getHeaderSize());
            }
        #endif
        else {
            // sending line by line, calculating x and y from the position and it's 32bit padding per line

            uint16_t imagePos = _position++ - _header.getHeaderAndPaletteSize();
            uint16_t y = (imagePos / _perLine) + _y;
            uint16_t x = ((imagePos % _perLine) / 2) + _x;

            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _canvas.height()), return 0);
            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sx(x, _canvas.width()), return 0);

            if (x < _width) {
                #if GFXCANVAS_SUPPORT_4BIT_BMP
                    if (!_cache.isY(y)) {
                        __LDBG_printf("y=%u x=%u a=%d", y, x, _available);
                        _cache.setY(y);
                        _canvas.setDecodePalette(false); // return palette index instead of RGB565
                        _canvas._decodeLine(_cache);
                        _canvas.setDecodePalette(true);
                    }
                    // for 4 bit we have to fetch 2 pixels per byte
                    uint8_t byte = _cache.at(x * 4) & 0x0f;
                    if (++x < _width) {
                        return byte | ((_cache.at(x * 4) << 4) & 0xf0);
                    }
                    return byte;
                #else
                    if (!_cache.isY(y)) {
                        __LDBG_printf("y=%u x=%u a=%d", y, x, _available);
                        _cache.setY(y);
                        _canvas._decodeLine(_cache);
                    }
                    // for 16bit, we have to split each pixel into its low and high byte
                    auto color = _cache.at(x);
                    if (imagePos % 2 == 0) {
                        return convertRGB565toRGB555LowByte(color);
                    }
                    return convertRGB565toRGB555HighByte(color);
                #endif
            }
            else {
                // 32bit padding
                #if DEBUG_GFXCANVAS
                    if (y == 0) { // display padding for first line only
                        __DBG_printf("y=0,x=%u,w=%u,padding=0", x, _width);
                    }
                #endif
                return 0;
            }
        }

    }
    return -1;
}

void GFXCanvasBitmapStream::_createHeader()
{
    #if GFXCANVAS_SUPPORT_4BIT_BMP
        #define DEBUG_PERLINE_FACTOR 0.5
        _perLine = (((_width + 3) / sizeof(uint32_t)) * sizeof(uint32_t) / 2);  // 32 bit padding, 4 bit per pixel, padding each line with zeros up to a 32bit boundary will result in up to 28 zeros = 7 'wasted pixels'
        auto &canvas = reinterpret_cast<GFXCanvasCompressedPalette &>(_canvas);
        _header.update(_width, _height, 4, canvas.getPaletteSize());
        // convert palette directly to BGR24
        for(uint8_t i = 0; i < canvas.getPaletteSize(); i++) {
            _header.setPaletteColor(i, canvas.getPaletteAt(i));
        }
        _available = _header.getHeaderAndPaletteSize();
    #else
        #define DEBUG_PERLINE_FACTOR 2
        _perLine = ((_width + 1) / sizeof(uint16_t)) * sizeof(uint16_t) * 2;  // 32 bit padding, 16 bit per pixel, padding each line with zeros up to a 32bit boundary will result in up to 2 zeros = 1 'wasted pixel'
        _header.update(_width, _height, 16, 0);
        _available = _header.getHeaderSize();
    #endif
    _available += _perLine * _height;
    _header.setBfSize(_available);

    #if DEBUG_GFXCANVAS
        __DBG_printf("bits=%u avail=%d hdr=%u pal=%u perln=%u(%u)", _header.getBitCount(), _available, _header.getHeaderSize(), _header.getHeaderAndPaletteSize(), _perLine, static_cast<uint32_t>(_width * DEBUG_PERLINE_FACTOR)/*unpadded size*/);
    #endif
}

#pragma GCC pop_options
