/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

/*

RLE compressed 16 color palette canvas

*/

#include "GFXCanvasConfig.h"
#include "GFXCanvas.h"
#include "GFXCanvasCompressed.h"
#include "GFXCanvasByteBuffer.h"

#if DEBUG_GFXCANVAS
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

using namespace GFXCanvas;

class GFXCanvasCompressedPalette : public GFXCanvasCompressed {
public:
    GFXCanvasCompressedPalette(uWidthType width, uHeightType height);
    GFXCanvasCompressedPalette(const GFXCanvasCompressedPalette &canvas);

    virtual GFXCanvasCompressed *clone();

    virtual void fillScreen(uint16_t color);

    virtual void _RLEdecode(ByteBuffer &buffer, ColorType *output);
    virtual void _RLEencode(ColorType *data, Buffer &buffer);

    ColorIndexType getPaletteSize() const;
    ColorType getPaletteAt(ColorIndexType index) const;

private:
    void _clearPalettte();
    ColorIndexType _addColor(ColorType color);
    ColorType _getColor(ColorIndexType index) const;
    ColorIndexType _getIndex(ColorType color) const;

private:
    static constexpr uint8_t kMaxColors = 16;

private:
    ColorType _palette[kMaxColors];
    ColorIndexType _size;
};

inline ColorIndexType GFXCanvasCompressedPalette::getPaletteSize() const
{
    return _size;
}

inline ColorType GFXCanvasCompressedPalette::getPaletteAt(ColorIndexType index) const
{
    return _palette[index];
}

inline void GFXCanvasCompressedPalette::_clearPalettte()
{
    _size = 0;
}

inline ColorIndexType GFXCanvasCompressedPalette::_addColor(ColorType color)
{
    auto index = _getIndex(color);
    if (index != -1) {
        return index;
    }
    if (_size < kMaxColors) {
        _palette[_size] = color;
        return _size++;
    }
    return 0;
}

inline ColorType GFXCanvasCompressedPalette::_getColor(ColorIndexType index) const
{
    return _palette[index];
}

inline ColorIndexType GFXCanvasCompressedPalette::_getIndex(ColorType color) const
{
    for(uint8_t i = 0; i < _size; i++) {
        if (_palette[i] == color) {
            return i;
        }
    }
    return -1;
}

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif
