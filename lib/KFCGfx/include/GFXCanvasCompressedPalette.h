/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if HAVE_GFX_LIB

#include "GFXCanvasCompressed.h"

class GFXCanvasCompressedPalette : public GFXCanvasCompressed {
public:
    GFXCanvasCompressedPalette(uint16_t width, uint16_t height);

    virtual GFXCanvasCompressed *clone();

    virtual void fillScreen(uint16_t color);

    virtual void _RLEdecode(Buffer &buffer, uint16_t *output);
    virtual void _RLEencode(uint16_t *data, Buffer &buffer);
    virtual uint16_t getColor(uint16_t color, bool addIfNotExists = true);
    virtual uint16_t *getPalette(uint8_t &count);
    virtual void setPalette(uint16_t* palette, uint8_t count);

    virtual String getDetails() const;

private:
    static const uint16_t INVALID_COLOR = 0;

    uint16_t _palette[15];
    uint8_t _paletteCount;
};

#endif
