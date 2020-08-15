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

#include "GFXCanvasCompressedPalette.h"

using namespace GFXCanvas;

GFXCanvasCompressedPalette::GFXCanvasCompressedPalette(uWidthType width, uHeightType height) : GFXCanvasCompressed(width, height)
{
}

GFXCanvasCompressedPalette::GFXCanvasCompressedPalette(const GFXCanvasCompressedPalette &canvas) : GFXCanvasCompressed(canvas.width(), canvas._lines), _palette(canvas._palette)
{
}

GFXCanvasCompressed* GFXCanvasCompressedPalette::clone()
{
    return __DBG_new(GFXCanvasCompressedPalette, *this);
}

void GFXCanvasCompressedPalette::fillScreen(uint16_t color)
{
    _palette.clear();
    GFXCanvasCompressed::fillScreen(color);
}

void GFXCanvasCompressedPalette::_RLEdecode(ByteBuffer &buffer, ColorType *output)
{
    __DBG_BOUNDS(int count = 0);
    auto begin = buffer.begin();
    auto end = buffer.end();
    while(begin < end) {
        auto rle = *begin++;
        uint8_t index = (rle >> 4);
        if ((rle & 0xf) == 0) {
            __DBG_BOUNDS_RETURN(__DBG_BOUNDS_buffer_end(begin, end));
            rle = *begin++;
        }
        else {
            rle &= 0x0f;
        }
        __DBG_BOUNDS(count += rle);
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_buffer_end(&output[rle - 1], &output[_width]));
        output = std::fill_n(output, rle, _palette[index]);
    }
    __DBG_BOUNDS_assert(count == _width);
}

void GFXCanvasCompressedPalette::_RLEencode(ColorType *data, ByteBuffer &buffer)
{
    // 4 bit: length, 0 indicates additional 8 bit
    // 4 bit: palette index
    // if length = 0: extra byte for length

    // _debug_printf("_encodeLine(%u): %p %p %u\n", y, ptr, end, _width);

    uint8_t rle = 0;
    auto begin = data;
    auto end = &data[_width];
    auto lastColor = *data;
    __DBG_BOUNDS(int count = 0);

    while(begin < end) {
        auto color = *begin++;
        if (color == lastColor && rle != 0xff) {
            rle++;
        }
        else {
            __DBG_BOUNDS(count += rle);
            auto index = _palette.addColor(lastColor);
            __DBG_BOUNDS_RETURN(__DBG_BOUNDS_assert(index != -1));
            if (rle > 0xf) {
                buffer.push2(index << 4, rle);
            }
            else {
                buffer.push(rle | (index << 4));
            }
            lastColor = color;
            rle = 1;
        }
    }
    if (begin == end) {
        __DBG_BOUNDS(count += rle);
        auto index = _palette.addColor(lastColor);
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_assert(index != -1));
        if (rle > 0xf) {
            buffer.push2(index << 4, rle);
        }
        else {
            buffer.push(rle | (index << 4));
        }
    }
    __DBG_BOUNDS_assert(count == _width);
}


ColorType GFXCanvasCompressedPalette::getPaletteColor(ColorType color) const
{
    return _palette.getColorIndex(color);
}

const ColorPalette *GFXCanvasCompressedPalette::getPalette() const
{
    return &_palette;
}

String GFXCanvasCompressedPalette::getDetails() const
{
    PrintString str = GFXCanvasCompressed::getDetails();
    str.printf_P(PSTR("palette %u="), _palette.length());

    uint8_t i = 0;
    for(const auto color: _palette) {
        str.printf("%06x", GFXCanvas::convertToRGB(color));
        if (++i < _palette.length()) {
            str.print(',');
        }
    }
    str.print('\n');
    return str;
}

#include <pop_optimize.h>
