/**
 * Author: sascha_lammers@gmx.de
 */

#include "GFXCanvasConfig.h"
#include <Arduino_compat.h>

#pragma GCC push_options

#if DEBUG_GFXCANVAS
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#    pragma GCC optimize("O3")
#endif

#include "GFXCanvas.h"

namespace GFXCanvas {

    int ColorPalette16::getColorIndex(ColorType findColor) const
    {
        uint8_t i = 0;
        for (const auto &color : *this) {
            if (findColor == color) {
                __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert(&color - begin() == i), return -1);
                return i;
            }
            i++;
        }
        return kInvalidIndexResult;
    }

    int ColorPalette16::addColor(ColorType color)
    {
        auto index = getColorIndex(color);
        if (index != kInvalidIndexResult) {
            return index;
        }
        if (_count < size()) { // not found and space available?
            index = _count;
            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert(_count < kColorsMax), return -1);
            _palette[_count++] = color;
        }
        return index;
    }

}

#pragma GCC pop_options
