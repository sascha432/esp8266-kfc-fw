/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if HAVE_GFX_LIB

#include <Arduino_compat.h>
#include <Buffer.h>
#include "GFXCanvas.h"

#include "push_pack.h"

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    uint16_t paletteCount;
} GFXCanvasRLEStreamHeader_t;

#include "pop_pack.h"

class GFXCanvasCompressed;

class GFXCanvasRLEStream : public Stream {
public:
    GFXCanvasRLEStream(GFXCanvasCompressed &canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    GFXCanvasRLEStream(GFXCanvasCompressed &canvas);

    virtual int available();
    virtual int read();

    virtual int peek() {
        return -1;
    }

    virtual size_t write(uint8_t) {
        return 0;
    }

private:
    void _writeColor(uint8_t rle, uint16_t color);

    uint8_t _sendBufferedByte();

private:
    static const int16_t DONE = -1;

    GFXCanvasCompressed &_canvas;
    GFXCanvas::Cache _cache;
    GFXCanvasRLEStreamHeader_t _header;
    Buffer _buffer;
    int32_t _position;
    uint16_t _lastColor;
};

#endif
