/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

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
    virtual ColorType getPaletteColor(ColorType color) const;
    virtual const ColorPalette *getPalette() const;

    virtual void getDetails(Print &output, bool displayPalette = false) const;

private:
    ColorPalette16 _palette;
};


inline ColorType GFXCanvasCompressedPalette::getPaletteColor(ColorType color) const
{
    return _palette.getColorIndex(color);
}

inline const ColorPalette *GFXCanvasCompressedPalette::getPalette() const
{
    __LDBG_printf("return %p", &_palette);
    return &_palette;
}

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif
