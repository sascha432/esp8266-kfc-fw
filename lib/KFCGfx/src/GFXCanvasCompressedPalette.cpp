/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasCompressedPalette.h"

#include <push_optimize.h>
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

using namespace GFXCanvas;

GFXCanvasCompressedPalette::GFXCanvasCompressedPalette(uWidthType width, uHeightType height) : GFXCanvasCompressed(width, height)
{
}

GFXCanvasCompressedPalette::GFXCanvasCompressedPalette(const GFXCanvasCompressedPalette &canvas) : GFXCanvasCompressed(canvas.width(), canvas._lines), _palette(canvas._palette)
{
}

GFXCanvasCompressed* GFXCanvasCompressedPalette::clone()
{
    return new GFXCanvasCompressedPalette(*this);
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
        uint16_t rle = *begin++;
        uint8_t index = (rle >> 4);
        if ((rle & 0xf) == 0) {
            __DBG_BOUNDS_RETURN(__DBG_BOUNDS_buffer_end(begin, end));
            rle = *begin++;
            rle += 0xf;
        }
        else {
            rle &= 0xf;
        }
        __DBG_BOUNDS(count += rle);
        __DBG_BOUNDS_RETURN(__DBG_BOUNDS_buffer_end(&output[rle - 1], &output[_width]));
        output = std::fill_n(output, rle, _palette[index]);
    }
    __DBG_BOUNDS_assertp(count == _width, "count=%u width=%u", count, _width);
}

void GFXCanvasCompressedPalette::_RLEencode(ColorType *data, Buffer &buffer)
{
    // 4 bit: rle = data [0x0-0xe], 0xf indicates that a byte follows
    // 4 bit: palette index
    // 8 bit: rle = data + 0x0f

    // _debug_printf("_encodeLine(%u): %p %p %u\n", y, ptr, end, _width);

    uint16_t rle = 0;
    auto begin = data;
    auto end = &data[_width];
    auto lastColor = *data;
    __DBG_BOUNDS(int count = 0);

    while(begin < end) {
        auto color = *begin++;
        if (color == lastColor && rle != (0xff + 0x0f)) {
            rle++;
        }
        else {
            __DBG_BOUNDS(count += rle);
            auto index = _palette.addColor(lastColor);
            __DBG_BOUNDS_RETURN(__DBG_BOUNDS_assert(index != -1));
            if (rle > 0xf) {
                //buffer.push_bb(index << 4, rle - 0xf);
                buffer.write(index << 4);
                buffer.write(rle - 0xf);
            }
            else {
                buffer.write(rle | (index << 4));
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
            //buffer.push_bb(index << 4, rle - 0xf);
            buffer.write(index << 4);
            buffer.write(rle - 0xf);
        }
        else {
            buffer.write(rle | (index << 4));
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
