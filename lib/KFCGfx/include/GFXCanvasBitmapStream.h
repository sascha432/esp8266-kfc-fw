/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <bitmap_header.h>
#include "GFXCanvasConfig.h"
#include "GFXCanvasCache.h"

class GFXCanvasCompressed;

class GFXCanvasBitmapStream : public Stream {
public:
    GFXCanvasBitmapStream(GFXCanvasCompressed &canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    GFXCanvasBitmapStream(GFXCanvasCompressed &canvas);

    virtual int available();
    virtual int read();
    virtual void flush();

    operator bool() const;
    virtual int peek();
    virtual size_t write(uint8_t);

    size_t size() const;

private:
    void _createHeader();

private:
    GFXCanvasCompressed &_canvas;
    GFXCanvas::Cache _cache;
    BitmapFileHeader_t _header;
    uint32_t _position;
    uint32_t _available;
    uint16_t _x;
    uint16_t _y;
    uint16_t _width;
    uint16_t _height;
};

inline GFXCanvasBitmapStream::operator bool() const 
{
    return _available != 0;
}

inline void GFXCanvasBitmapStream::flush() 
{
}

inline int GFXCanvasBitmapStream::peek() 
{
    return -1;
}

inline size_t GFXCanvasBitmapStream::write(uint8_t) 
{
    return 0;
}

inline int GFXCanvasBitmapStream::available()
{
    return _available;
}

inline size_t GFXCanvasBitmapStream::size() const
{
    return _header.h.bfh.bfSize;
}

