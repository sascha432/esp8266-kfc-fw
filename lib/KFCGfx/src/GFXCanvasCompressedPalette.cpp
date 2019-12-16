/**
 * Author: sascha_lammers@gmx.de
 */

#if HAVE_GFX_LIB

#include "GFXCanvasCompressedPalette.h"

#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


GFXCanvasCompressedPalette::GFXCanvasCompressedPalette(uint16_t width, uint16_t height) : GFXCanvasCompressed(width, height) , _paletteCount(0)
{
}

GFXCanvasCompressed* GFXCanvasCompressedPalette::clone()
{
    GFXCanvasCompressedPalette* target = new GFXCanvasCompressedPalette(width(), height());
    for (uint16_t y = 0; y < _height; y++) {
        target->_lineBuffer[y].clone(_lineBuffer[y]);
    }
    memcpy(target->_palette, _palette, sizeof(target->_palette));
    target->_paletteCount = _paletteCount;

    return target;
}

void GFXCanvasCompressedPalette::fillScreen(uint16_t color)
{
    _paletteCount = 0;
    GFXCanvasCompressed::fillScreen(color);
}

void GFXCanvasCompressedPalette::_RLEdecode(Buffer &buffer, uint16_t *output)
{
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    auto outputEndPtr = &output[_width];
    int count = 0;
#endif
    auto data = buffer.get();
    auto endPtr = &data[buffer.length()];
    while(data < endPtr) {
        auto rle = *data++;
        uint8_t index = rle >> 4;
        if ((rle & 0xf) == 0) {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
            assert(data < endPtr);
#endif
            rle = *data++;
        }
        else {
            rle &= 0x0f;
        }
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
        if (!(&output[rle] <= outputEndPtr)) {
            __debugbreak_and_panic();
        }
        count += rle;
#endif
        while(rle--) {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
            if (index >= _paletteCount) {
                __debugbreak_and_panic();
            }
#endif
            *output++ = _palette[index];
        }
    }
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (count != _width) {
        __debugbreak_and_panic();
    }
#endif
}

void GFXCanvasCompressedPalette::_RLEencode(uint16_t *data, Buffer &buffer)
{
    // 4 bit: length, 0 indicates additional 8 bit
    // 4 bit: palette index
    // if length = 0: extra byte for length

    // _debug_printf("_encodeLine(%u): %p %p %u\n", y, ptr, endPtr, _width);

    uint8_t rle = 0;
    auto endPtr = &data[_width];
    auto lastColor = *data;
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    int count = 0;
#endif

    while(data < endPtr) {
        auto color = *data++;
        if (color == lastColor && rle != 0xff) {
            rle++;
        }
        else {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
            count += rle;
#endif
            auto index = getColor(lastColor);
            if (rle > 0xf) {
                buffer.write(index << 4);
                buffer.write(rle);
            }
            else {
                buffer.write(rle | (index << 4));
            }
            lastColor = color;
            rle = 1;
        }
    }
    if (data == endPtr) {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
        count += rle;
#endif
        auto index = getColor(lastColor);
        if (rle > 0xf) {
            buffer.write(index << 4);
            buffer.write(rle);
        }
        else {
            buffer.write(rle | (index << 4));
        }
    }
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
    if (count != _width) {
        __debugbreak_and_panic();
    }
#endif
}

uint16_t GFXCanvasCompressedPalette::getColor(uint16_t color, bool addIfNotExists)
{
    for (uint8_t i = 0; i < _paletteCount; i++) {
        if (_palette[i] == color) {
            return i;
        }
    }
    if (addIfNotExists) {
        if (_paletteCount >= 15) {
#if DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
            __debugbreak_and_panic();
#endif
            return INVALID_COLOR;
        }
        _palette[_paletteCount++] = color;
        return _paletteCount - 1;
    }
    return INVALID_COLOR;
}

uint16_t *GFXCanvasCompressedPalette::getPalette(uint8_t &count)
{
    count = _paletteCount;
    return _palette;
}

void GFXCanvasCompressedPalette::setPalette(uint16_t* palette, uint8_t count)
{
    memcpy(_palette, palette, count * sizeof(uint16_t));
}

String GFXCanvasCompressedPalette::getDetails() const
{
    PrintString str = GFXCanvasCompressed::getDetails();
    str.printf_P(PSTR(" palette %u - "), _paletteCount);

    for (uint8_t i = 0; i < _paletteCount; i++) {
        str.printf("[%u]%06x,", i, GFXCanvas::convertToRGB(_palette[i]));
    }
    str.remove(str.length() - 1);
    str.write('\n');
    return str;
}

#endif
