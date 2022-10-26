/**
* Author: sascha_lammers@gmx.de
*/


// http://www.daubnet.com/en/file-format-bmp

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
    _height(h),
    #if GFXCANVAS_SUPPORT_4BIT_BMP
        _perLine(((_width + 3) / 4) * 4 / 2)  // 32 bit padding, 4 bit per pixel
    #else
        _perLine(((_width + 1) / 2) * 2 * 2)  // 32 bit padding, 16 bit per pixel
    #endif

{
    _createHeader();
    __LDBG_printf("init x,y,w,h,p=%d,%d,%d,%d,%d", _x, _y, _width, _height, _perLine);
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
            __LDBG_printf("hdr %d/%d a=%d", _position, _header.getHeaderSize(), _available);
            return _header.getHeaderAt(_position++);
        }
        #if GFXCANVAS_SUPPORT_4BIT_BMP
            else if (_position < _header.getHeaderAndPaletteSize()) {
                __LDBG_printf("pal %d/%d a=%d pal=%u", _position - _header.getHeaderSize(), _header.getHeaderAndPaletteSize() - _header.getHeaderSize(), _available, _header.getPaletteAt(_position));
                return _header.getPaletteAt(_position++);
            }
        #endif
        else {
            // sending line by line, calculating x and y from the position and it's 32bit padding per line

            uint16_t imagePos = _position++ - _header.getHeaderAndPaletteSize();
            uint16_t y = (imagePos / _perLine) + _y;
            uint16_t x = ((imagePos % _perLine) / 2) + _x;

            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _canvas.height()), return 0);
            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sx(x, _canvas.width()), return 0);

            if (!_cache.isY(y)) {
                __LDBG_printf("y=%u x=%u avail=%d", y, x, _available);
                _cache.setY(y);
                _canvas._decodeLine(_cache);
            }

            #if GFXCANVAS_SUPPORT_4BIT_BMP
                if (x < _width) {
                    auto color = _cache.at(x);
                    auto &canvas4bit = *(_canvas.getColorPalette());
                    uint8_t byte = *canvas4bit.find(color) << 4;
                    if (++x < _width) {
                        color = _cache.at(x);
                        byte |= static_cast<uint8_t>(*canvas4bit.find(color));
                    }
                    return byte;
                }
                else {
                    return 0; // 32bit padding
                }
            #else
                if (x < _width) {
                    auto color = _cache.at(x);
                    // convert rgb565 to rgb555
                    if (imagePos % 2 == 0) {    // return low byte
                        return (color & 0x1f) | ((color & 0xc0) >> 1) | ((color >> 8) << 7);
                    }
                    else {  // return high byte
                        return color >> 9;
                    }
                }
                else {
                    return 0; // 32bit padding
                }
            #endif
        }

    }
    return -1;
}

void GFXCanvasBitmapStream::_createHeader()
{
    #if GFXCANVAS_SUPPORT_4BIT_BMP
        _header = BitmapHeaderType(_width, _height, _canvas._palette->bits(), _canvas._palette->size());
        _available = _header.getHeaderAndPaletteSize();
        _available += ((_width + 3) * 4) / 4 / 2 * _height;
    #else
        _header = BitmapHeaderType(_width, _height, 16, 0);
        _available = _header.getHeaderSize();
        _available += (2UL * _width * _height);
    #endif
    _header.setBfSize(_available);

    __LDBG_printf("bits=%u available=%d hdr_sz=%u pal_sz=%u", _header.getBitCount(), _available, _header.getHeaderSize(), _header.getHeaderAndPaletteSize());
}

#pragma GCC pop_options
