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

#include "GFXCanvas.h"

using namespace GFXCanvas;

ColorPalette::ColorPalette() : _count(0), _palette{}
{
}

ColorType &ColorPalette::at(int index)
{
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assertp((unsigned)index < (unsigned)_count, "index=%d count=%d", index, _count), return _palette[0]);
    index = (unsigned)index % _count;
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert((unsigned)index < (unsigned)_count), return _palette[0]);
    return _palette[index];
}

ColorType ColorPalette::at(ColorType index) const
{
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assertp((unsigned)index < (unsigned)_count, "index=%d count=%d", index, _count), return _palette[0]);
    if (_count == 0) {
        return 0;
    }
    index = (unsigned)index % _count;
    __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert((unsigned)index < (unsigned)_count), return _palette[0]);
    return _palette[index];
}

ColorType &ColorPalette::operator[](int index)
{
    return at(index);
}

ColorType ColorPalette::operator[](ColorType index) const
{
    return at(index);
}

bool ColorPalette::empty() const
{
    return _count == 0;
}

size_t ColorPalette::length() const
{
    return _count;
}

size_t ColorPalette::size() const
{
    return kColorsMax;
}

void ColorPalette::clear()
{
    // set _count to 0 as well
    _count = 0;
    std::fill(_palette, &_palette[kColorsMax], 0);
}

ColorType *ColorPalette::begin()
{
    return &_palette[0];
}

ColorType *ColorPalette::end()
{
    return &_palette[_count];
}

const ColorType *ColorPalette::begin() const
{
    return &_palette[0];
}

const ColorType *ColorPalette::end() const
{
    return &_palette[_count];
}

int ColorPalette::getColorIndex(ColorType findColor) const
{
    uint8_t i = 0;
    for(const auto &color: *this) {
        if (findColor == color) {
            __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert(&color - begin() == i), return -1);
            return i;
        }
        i++;
    }
    return -1;
}

int ColorPalette::addColor(ColorType color)
{
    auto index = getColorIndex(color);
    if (index == -1 && _count < size()) { // not found and space available?
        index = _count;
        __DBG_BOUNDS_ACTION(__DBG_BOUNDS_assert(_count < kColorsMax), return -1);
        _palette[_count++] = color;
    }
    return index;
}

const uint8_t *ColorPalette::getBytes() const
{
    return reinterpret_cast<const uint8_t *>(&_palette[0]);
}

size_t ColorPalette::getBytesLength() const
{
    return sizeof(_palette[0]) * _count;
}

#pragma GCC pop_options
