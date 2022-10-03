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

    virtual void getDetails(Print &output, bool displayPalette = false) const;
};

#if DEBUG_GFXCANVAS
#    include "debug_helper_disable.h"
#endif
