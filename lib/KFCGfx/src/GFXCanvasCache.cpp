/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include <debug_helper.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasCompressed.h"

#pragma GCC push_options
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasCache.h"

using namespace GFXCanvas;

Cache::Cache(Cache &&cache) noexcept : _buffer(std::exchange(cache._buffer, nullptr)), _y(std::exchange(cache._y, kYInvalid)), _width(std::exchange(cache._width, 0)), _flags(std::exchange(cache._flags, 0))
{
}

Cache::Cache(uWidthType width, sYType y) : _buffer(new ColorType[width]()), _y(y), _width(width), _flags(0)
{
    if (!_buffer) {
        __DBG_panic("alloc failed, width=%u", width);
        invalidate();
    }
}

Cache::Cache(uWidthType width, nullptr_t) : _buffer(nullptr), _y(kYInvalid), _width(width), _flags(0)
{
}

Cache::~Cache()
{
    if (_buffer) {
        delete[] _buffer;
    }
}

Cache &Cache::operator=(Cache &&cache) noexcept
{
    if (_buffer) {
        delete[] _buffer;
    }
    _buffer = std::exchange(cache._buffer, nullptr);
    _y = std::exchange(cache._y, kYInvalid);
    _width = std::exchange(cache._width, 0);
    _flags = std::exchange(cache._flags, 0);
    // cache.__reset();
    return *this;
}

void Cache::allocate()
{
    if (!_buffer) {
        _buffer = new ColorType[_width];
    }
    _y = kYInvalid;
    _flags = 0;
}

void Cache::release()
{
    if (_buffer) {
        delete[]  _buffer;
    }
    _buffer = nullptr;
    _y = kYInvalid;
    _flags = 0;
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
#pragma GCC pop_options
