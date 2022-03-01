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

#include "GFXCanvasLines.h"
#include "GFXCanvasStats.h"

using namespace GFXCanvas;

Lines::Lines() : _height(0), _lines(nullptr)
{
}

Lines::Lines(uHeightType height) : _height(height), _lines(new LineBuffer[height]())
{
    assert(_lines != nullptr);
    #if DEBUG_GFXCANVAS_STATS_DETAILS
        if (_lines != nullptr) {
            stats.malloc++;
        }
    #endif
}

Lines::Lines(const Lines &lines) : Lines(lines._height)
{
    std::copy(lines.begin(), lines.end(), begin());
}

Lines::~Lines()
{
    if (_lines) {
        __DBG_ASTATS(
            stats.free++;
        );
        delete[]  _lines;
    }
}

#pragma GCC pop_options

