/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasCache.h"

using namespace GFXCanvas;

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
    ColorType _lastColor;
};
