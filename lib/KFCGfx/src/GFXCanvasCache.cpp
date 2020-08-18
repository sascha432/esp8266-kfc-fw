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

#if DEBUG_GFXCANVAS_MEM
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable_mem.h>
#endif

using namespace GFXCanvas;

Cache::Cache(Cache &&cache) : _buffer(cache._buffer), _y(cache._y), _width(cache._width), _flags(cache._flags)
{
    cache.__reset();
}

Cache::Cache(uWidthType width, sYType y) : _buffer(__LDBG_new_array(width, ColorType)), _y(y), _width(width), _flags(0)
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
        __LDBG_delete_array(_buffer);
    }
}

Cache &Cache::operator=(Cache &&cache)
{
    if (_buffer) {
        __LDBG_delete_array(_buffer);
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
        _buffer = __LDBG_new_array(_width, ColorType);
    }
    _y = kYInvalid;
    _flags = 0;
}

void Cache::release()
{
    if (_buffer) {
        __LDBG_delete_array(_buffer);
    }
    _buffer = nullptr;
    _y = kYInvalid;
    _flags = 0;
}

void Cache::fill(ColorType color)
{
    __DBG_BOUNDS_RETURN(__DBG_check_alloc_aligned(_buffer));
    std::fill(_buffer, &_buffer[_width], color);
}

void Cache::fill(ColorType color, uWidthType startX, uWidthType endX)
{
    __DBG_BOUNDS_sx(startX, _width);
    __DBG_BOUNDS_ex(endX, startX, _width);
    if (startX < 0) {
        startX = 0;
    }
    if (endX <= startX) {
        return;
    }
    else if (endX >= _width) {
        endX = _width - 1;
    }
    __DBG_BOUNDS_RETURN(
        __DBG_check_alloc_aligned(_buffer) ||
        __DBG_BOUNDS_buffer(&_buffer[startX], _buffer, &_buffer[_width]) ||
        __DBG_BOUNDS_buffer(&_buffer[endX - 1], _buffer, &_buffer[_width])
    );
    std::fill(&_buffer[startX], &_buffer[endX], color);
}

void Cache::setPixel(sXType x, ColorType color)
{
    __DBG_BOUNDS_RETURN(
        __DBG_BOUNDS_sx(x, _width) ||
        __DBG_check_alloc_aligned(_buffer)
    );
    _buffer[x] = color;
}

SingleLineCache::SingleLineCache(uWidthType width, uHeightType height, size_t) : Cache(width, kYInvalid)
{
}

void SingleLineCache::flush(GFXCanvasCompressed &canvas)
{
    if (hasWriteFlag()) {
        __DBG_BOUNDS_RETURN(__DBG_check_alloc_aligned(_buffer));
        canvas._encodeLine(*this);
    }
}

Cache &SingleLineCache::get(GFXCanvasCompressed &canvas, sYType y)
{
    if (!_buffer) {
        allocate();
    }
    __DBG_BOUNDS_ACTION(__DBG_check_alloc_aligned(_buffer), return *this);
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
#include <debug_helper_disable_mem.h>
