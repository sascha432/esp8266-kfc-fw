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
    virtual int read() {
        return _read(false);
    }
    virtual int peek() {
        if (_buffer.length()) {
            return _buffer[0];
        }
        return _read(true);
    }
    virtual size_t readBytes(char *buffer, size_t length);

    virtual size_t write(uint8_t) {
        return 0;
    }

private:
    int _read(bool peek);
    void _writeColor(uint8_t rle, uint16_t color);
    uint8_t _sendBufferedByte(bool peek);

private:
    static const int16_t DONE = -1;

    GFXCanvasCompressed &_canvas;
    GFXCanvas::Cache _cache;
    GFXCanvasRLEStreamHeader_t _header;
    Buffer _buffer;
    int32_t _position;
    ColorType _lastColor;
};
