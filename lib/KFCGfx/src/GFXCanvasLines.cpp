/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#pragma GCC push_options
#include <push_optimize.h>
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasLines.h"

using namespace GFXCanvas;

Lines::Lines() : _height(0), _lines(nullptr)
{
}

Lines::Lines(uHeightType height) : _height(height), _lines(new LineBuffer[height])
{
    assert(_lines != nullptr);
}

Lines::Lines(const Lines &lines) : Lines(lines._height)
{
    std::copy(lines.begin(), lines.end(), begin());
}

Lines::~Lines()
{
    if (_lines) {
        delete[]  _lines;
    }
}

void Lines::fill(ColorType color, uYType start, uYType end)
{
    for(uYType i = start; i < end; i++) {
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(i, _height));
        _lines[i].clear(color);
    }
}

#pragma GCC pop_options

