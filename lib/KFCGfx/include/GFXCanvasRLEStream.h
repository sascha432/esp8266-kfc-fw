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
    virtual int peek();
    virtual size_t readBytes(char *buffer, size_t length) {
        return readBytes(reinterpret_cast<uint8_t *>(buffer), length);
    }
    size_t readBytes(uint8_t *buffer, size_t length);

    virtual size_t write(uint8_t) {
        return 0;
    }

private:
    int _read(bool peek);
    uint8_t *_writeColor(uint8_t *buffer, uint16_t rle, uint16_t color);
    int _fillBuffer(uint8_t *buffer, size_t length);

private:
    static const int32_t DONE = -1;
    static const int32_t LAST = (1 << 31);

    GFXCanvasCompressed &_canvas;
    GFXCanvas::Cache _cache;
    GFXCanvasRLEStreamHeader_t _header;
    Buffer _buffer;
    int32_t _position;
    ColorType _lastColor;
};
