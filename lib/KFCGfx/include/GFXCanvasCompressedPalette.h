/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "GFXCanvasCompressed.h"

class GFXCanvasCompressedPalette : public GFXCanvasCompressed {
public:
    using ByteBuffer = GFXCanvas::ByteBuffer;

    GFXCanvasCompressedPalette(coord_x_t width, coord_y_t height);

    virtual GFXCanvasCompressed *clone();

    virtual void fillScreen(uint16_t color);

    virtual void _RLEdecode(ByteBuffer &buffer, color_t *output);
    virtual void _RLEencode(color_t *data, ByteBuffer &buffer);
    virtual uint16_t getColor(color_t color, bool addIfNotExists = true);
    virtual uint16_t *getPalette(uint8_t &count);
    virtual void setPalette(color_t *palette, uint8_t count);

    virtual String getDetails() const;

private:
    static const color_t INVALID_COLOR = 0;

    color_t _palette[15];
    uint8_t _paletteCount;
};
