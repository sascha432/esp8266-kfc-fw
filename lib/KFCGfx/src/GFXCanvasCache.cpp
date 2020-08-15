/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include <debug_helper.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasCompressed.h"

#include <push_optimize.h>
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasCache.h"

using namespace GFXCanvas;

Cache::Cache(Cache &&cache) : _buffer(cache._buffer), _y(cache._y), _width(cache._width), _flags(cache._flags)
{
    cache.__reset();
}

Cache::Cache(uWidthType width, sYType y) : _buffer(__DBG_new_array(width, ColorType)), _y(y), _width(width), _flags(0)
{
    if (!_buffer) {
        __DBG_panic("alloc failed, width=%u", width);
        __reset();
    }
}

Cache::Cache(uWidthType width, nullptr_t) : _buffer(nullptr), _y(kYInvalid), _width(width), _flags(0)
{
}

Cache::~Cache()
{
    if (_buffer) {
        __DBG_delete_array(_buffer);
    }
}

Cache &Cache::operator=(Cache &&cache)
{
    if (_buffer) {
        __DBG_delete_array(_buffer);
    }
    _buffer = cache._buffer;
    _y = cache._y;
    _width = cache._width;
    _flags = cache._flags;
    cache.__reset();
    return *this;
}

void Cache::allocate()
{
    if (!_buffer) {
        _buffer = __DBG_new_array(_width, ColorType);
    }
    _y = kYInvalid;
    _flags = 0;
}

void Cache::release()
{
    if (_buffer) {
        __DBG_delete_array(_buffer);
    }
    _buffer = nullptr;
    _y = kYInvalid;
    _flags = 0;
}

void Cache::fill(ColorType color)
{
    #undef __DBG_DEFAULT_ACTION
    #define __DBG_DEFAULT_ACTION return;
    __DBG_check_alloc_aligned(_buffer);
    std::fill(_buffer, &_buffer[_width], color);
    // auto begin = _buffer;
    // auto end = &_buffer[_width];
    // while(begin < end) {
    //     *begin++ = color;
    // }
}

void Cache::fill(ColorType color, uWidthType startX, uWidthType endX)
{
    #undef __DBG_DEFAULT_ACTION
    #define __DBG_DEFAULT_ACTION return;
    __DBG_check_sx(startX, _width);
    __DBG_check_ex(endX, startX, _width);
    __DBG_check_alloc_aligned(_buffer);
    __DBG_check_buffer(&_buffer[startX], _buffer, &_buffer[_width]);
    __DBG_check_buffer(&_buffer[endX - 1], _buffer, &_buffer[_width]);
    std::fill(&_buffer[startX], &_buffer[endX], color);
    // auto begin = &_buffer[startX];
    // auto end = &_buffer[endX];
    // while(begin < end) {
    //     *begin++ = color;
    // }
}

void Cache::setPixel(sXType x, ColorType color)
{
    #undef __DBG_DEFAULT_ACTION
    #define __DBG_DEFAULT_ACTION return;
    __DBG_check_sx(x, _width);
    __DBG_check_alloc_aligned(_buffer);
    #undef __DBG_DEFAULT_ACTION
    #define __DBG_DEFAULT_ACTION ;
    _buffer[x] = color;
}

SingleLineCache::SingleLineCache(uWidthType width, uHeightType height, size_t) : Cache(width, kYInvalid)
{
}

void SingleLineCache::flush(GFXCanvasCompressed &canvas)
{
    if (hasWriteFlag()) {
        __DBG_check_alloc_aligned(_buffer);
        canvas._encodeLine(*this);
    }
}

Cache &SingleLineCache::get(GFXCanvasCompressed &canvas, sYType y)
{
    if (!_buffer) {
        allocate();
    }
    __DBG_check_alloc_aligned(_buffer);
    if (!isY(y)) {
        flush(canvas);
        setY(y);
    }
    return *this;
}

void SingleLineCache::invalidateRange(sYType startY, sYType endY)
{
    invalidate();
}

void SingleLineCache::free(GFXCanvasCompressed &canvas)
{
    // __LDBG_printf("buffer=%p", _buffer);
    flush(canvas);
    release();
}

void SingleLineCache::resize(GFXCanvasCompressed &canvas, size_t numLines)
{
    __LDBG_printf("lines=%u", numLines);
    flush(canvas);
}

size_t SingleLineCache::length() const
{
    return 1;
}


#include <pop_optimize.h>
