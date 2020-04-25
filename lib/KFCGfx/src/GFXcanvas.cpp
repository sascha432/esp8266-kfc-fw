/**
* Author: sascha_lammers@gmx.de
*/

#include <push_optimize.h>
#pragma GCC optimize ("O3")

#include "GFXcanvas.h"

#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace GFXCanvas;

// Stats

#if DEBUG_GFXCANVASCOMPRESSED_STATS

void Stats::dump(Print &output) const {
#if !DEBUG_GFXCANVASCOMPRESSED_STATS_DETAILS
    output.printf_P(PSTR("drawInto %.3fms\n"),
        drawInto / 1000.0
    );
#else
    output.printf_P(PSTR("de/encode %u/%.2fms %u/%.2fms mops %u cacheR %u F %u D %u max %ub drawInto %.3fms\n"),
        decode_count, decode_time, encode_count, encode_time,
        malloc,
        cache_read, cache_flush, cache_drop, cache_max, drawInto / 1000.0
    );
#endif
}

#endif

// Cache

Cache::Cache(Cache &&cache)
{
    *this = std::move(cache);
}

Cache::Cache(coord_x_t width, scoord_y_t y) : _buffer(nullptr), _y(y), _width(width), _read(0), _write(0)
{
#if GFXCANVAS_MAX_CACHED_LINES > 1
    allocBuffer();
#else
    _buffer = (color_t *)malloc(_width * sizeof(*_buffer));
#endif
}

Cache::~Cache()
{
    if (_buffer) {
        free(_buffer);
    }
}

Cache &Cache::operator =(Cache &&cache)
{
    _buffer = cache._buffer;
    _read = cache._read;
    _write = cache._write;
    _y = cache._y;
    _width = cache._width;
    cache._buffer = nullptr;
    cache.setY(INVALID);
    return *this;
}

#if GFXCANVAS_MAX_CACHED_LINES > 1

void GFXCanvas::Cache::allocBuffer()
{
    if (!_buffer) {
        size_t size = _width * sizeof(*_buffer);
        _buffer = (color_t *)malloc(size);
        if (!_buffer) {
            debug_printf("Cache(): malloc %u failed\n", size);
            __debugbreak_and_panic();
        }
    }
}

void GFXCanvas::Cache::freeBuffer()
{
    setY(Cache::INVALID);
    if (_buffer) {
        free(_buffer);
        _buffer = nullptr;
    }
}

#endif

// LineBuffer

LineBuffer::LineBuffer() : _fillColor(0)
{
}

void GFXCanvas::LineBuffer::clone(LineBuffer &source)
{
    _buffer.write(source._buffer.get(), source._buffer.length());
    _fillColor = source._fillColor;
}

// functions

void GFXCanvas::convertToRGB(color_t color, uint8_t& r, uint8_t& g, uint8_t& b)
{
    r = ((color >> 11) * 527 + 23) >> 6;
    g = (((color >> 5) & 0x3f) * 259 + 33) >> 6;
    b = ((color & 0x1f) * 527 + 23) >> 6;
}

uint32_t GFXCanvas::convertToRGB(color_t color)
{
    uint8_t r, g, b;
    convertToRGB(color, r, g, b);
    return (r << 16) | (g << 8) | b;
}

color_t GFXCanvas::convertRGBtoRGB565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((b >> 3) & 0x1f) | (((g >> 2) & 0x3f) << 5) | (((r >> 3) & 0x1f) << 11);
}

color_t GFXCanvas::convertRGBtoRGB565(uint32_t rgb)
{
    return convertRGBtoRGB565(rgb, rgb >> 8, rgb >> 16);
}

#include <pop_optimize.h>
