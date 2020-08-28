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

using namespace GFXCanvas;

Lines::Lines() : _height(0), _lines(nullptr)
{
}

Lines::Lines(uHeightType height) : _height(height), _lines(__LDBG_new_array(height, LineBuffer))
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
        __LDBG_delete_array(_lines);
    }
}

void Lines::fill(ColorType color, uYType start, uYType end)
{
    for(uYType i = start; i < end; i++) {
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_sy(i, _height));
        _lines[i].clear(color);
    }
}

#include <pop_optimize.h>

