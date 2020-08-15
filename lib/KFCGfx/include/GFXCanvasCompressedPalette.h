/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "GFXCanvasConfig.h"
#include "GFXCanvas.h"
#include "GFXCanvasCompressed.h"
#include "GFXCanvasByteBuffer.h"

using namespace GFXCanvas;

class GFXCanvasCompressedPalette : public GFXCanvasCompressed {
public:
    GFXCanvasCompressedPalette(uWidthType width, uHeightType height);
    GFXCanvasCompressedPalette(const GFXCanvasCompressedPalette &canvas);

    virtual GFXCanvasCompressed *clone();

    virtual void fillScreen(uint16_t color);

    virtual void _RLEdecode(ByteBuffer &buffer, ColorType *output);
    virtual void _RLEencode(ColorType *data, ByteBuffer &buffer);
    virtual ColorType getPaletteColor(ColorType color) const;
    virtual const ColorPalette *getPalette() const;

    virtual String getDetails() const;

private:
    ColorPalette _palette;
};
