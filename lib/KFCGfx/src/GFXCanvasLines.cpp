/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#include <push_optimize.h>
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasLines.h"

#if DEBUG_GFXCANVAS_MEM
#include <debug_helper_enable_mem.h>
#else
#include <debug_helper_disable_mem.h>
#endif

#if SAVE_CRASH_HAVE_ALLOC_POINTERS
#include <EspSaveCrash.h>
#endif

using namespace GFXCanvas;

Lines::Lines() : _height(0), _lines(nullptr)
{
}

Lines::Lines(uHeightType height, const LineBuffer *lines) : _height(height), _lines(__LDBG_new_array(height, LineBuffer))
{
    if (!_lines) {
        __DBG_panic("failed to allocate height=%u", _height);
    }
#if SAVE_CRASH_HAVE_ALLOC_POINTERS
    EspSaveCrash::addPointer(_lines);
#endif
    auto begin = lines;
    auto end = &lines[_height];
    auto dst = _lines;
    while(begin < end) {
        *dst++ = *begin++;
    }
}

Lines::Lines(uHeightType height) : _height(height), _lines(__LDBG_new_array(height, LineBuffer))
{
    if (!_lines) {
        __DBG_panic("failed to allocate height=%u", height);
    }
#if SAVE_CRASH_HAVE_ALLOC_POINTERS
    EspSaveCrash::addPointer(_lines);
#endif
}

Lines::~Lines()
{
    if (_lines) {
#if SAVE_CRASH_HAVE_ALLOC_POINTERS
        EspSaveCrash::removePointer(_lines);
#endif
        __LDBG_delete_array(_lines);
    }
}

Lines &Lines::operator=(const Lines &lines)
{
    if (_lines) {
#if SAVE_CRASH_HAVE_ALLOC_POINTERS
        EspSaveCrash::removePointer(_lines);
#endif
        __LDBG_delete_array(_lines);
    }
    _height = lines.height();
    _lines = __LDBG_new_array(_height, LineBuffer);
    for(uYType i = 0; i < _height; i++) {
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(i, _height), break);
        _lines[i] = lines.getLine(i);
    }
    return *this;
}

void Lines::clear(ColorType color)
{
    fill(color, 0, _height);
}

void Lines::fill(ColorType color, uYType start, uYType end)
{
    for(uYType i = start; i < end; i++) {
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(i, _height));
        _lines[i].clear(color);
    }
}

void Lines::fill(ColorType color, uYType y)
{
    getLine(y).clear(color);
}

uHeightType Lines::height() const
{
    return _height;
}

ColorType Lines::getFillColor(sYType y) const
{
    return getLine(y).getFillColor();
}

ByteBuffer &Lines::getBuffer(sYType y)
{
    return getLine(y).getBuffer();
}

const ByteBuffer &Lines::getBuffer(sYType y) const
{
    return getLine(y).getBuffer();
}

LineBuffer &Lines::getLine(sYType y)
{
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _height), y = 0);
    return _lines[y];
}

const LineBuffer &Lines::getLine(sYType y) const
{
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_sy(y, _height), y = 0);
    return _lines[y];
}

uWidthType Lines::length(sYType y) const
{
    return getLine(y).length();
}

#include <pop_optimize.h>

