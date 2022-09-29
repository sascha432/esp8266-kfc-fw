/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#pragma GCC push_options
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasBitmapStream.h"
#include "GFXCanvasCompressed.h"

using namespace GFXCanvas;

GFXCanvasBitmapStream::GFXCanvasBitmapStream(GFXCanvasCompressed& canvas) : GFXCanvasBitmapStream(canvas, 0, 0, canvas.width(), canvas.height())
{
}

GFXCanvasBitmapStream::GFXCanvasBitmapStream(GFXCanvasCompressed& canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h) : _canvas(canvas), _cache(canvas.width(), Cache::kYInvalid)
{
    _x = x;
    _y = y;
    _width = w;
    _height = h;
    _position = 0;
    __LDBG_printf("init x,y,w,h=%d,%d,%d,%d", _x, _y, _width, _height);
    _createHeader();
}

int GFXCanvasBitmapStream::read()
{
    if (_available) {
        _available--;
        if (_position < sizeof(_header)) {
            return _header.b[_position++];
        }
        else {
            uint16_t imagePos = _position++ - sizeof(_header); // maximum length is limited by _available
            //uint16_t perLine = _canvas.width() * 2;
            uint16_t perLine = _width * 2;
            uint16_t y = (imagePos / perLine) + _y;
            uint16_t x = ((imagePos % perLine) / 2) + _x;

            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _canvas.height()), return 0);
            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sx(x, _canvas.width()), return 0);

            if (!_cache.isY(y)) {
                _cache.setY(y);
                _canvas._decodeLine(_cache);
            }

            auto color = _cache.getBuffer()[x];

            // convert rgb565 to rgb555
            if (imagePos % 2 == 0) {    // return low byte
                return (color & 0x1f) | ((color & 0xc0) >> 1) | ((color >> 8) << 7);
            }
            else {  // return high byte
                return color >> 9;
            }
        }
    }
    return -1;
}

void GFXCanvasBitmapStream::_createHeader()
{
    _available = 2UL * _width * _height + sizeof(_header.h);

    _header = {};
    _header.h.bfh.bfType = 'B' | ('M' << 8);
    _header.h.bfh.bfSize = _available;
    _header.h.bfh.bfReserved2 = sizeof(_header.h);

    _header.h.bih.biSize = sizeof(_header.h.bih);
    _header.h.bih.biPlanes = 1;
    _header.h.bih.biBitCount = 16;
    _header.h.bih.biWidth = _width;
    _header.h.bih.biHeight = -_height; // negative means top to bottom
}

#pragma GCC pop_options
