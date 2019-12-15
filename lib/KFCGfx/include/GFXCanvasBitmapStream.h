/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvas.h"

#include "push_pack.h"

#if !_MSC_VER

typedef struct __attribute__packed__ {
    uint16_t        bfType;
    uint32_t        bfSize;
    uint16_t        bfReserved1;
    uint16_t        bfReserved2;
    uint32_t        bfOffBits;
} BITMAPFILEHEADER;

typedef struct __attribute__packed__ {
    uint32_t        biSize;
    int32_t         biWidth;
    int32_t         biHeight;
    uint16_t        biPlanes;
    uint16_t        biBitCount;
    uint32_t        biCompression;
    uint32_t        biSizeImage;
    int32_t         biXPelsPerMeter;
    int32_t         biYPelsPerMeter;
    uint32_t        biClrUsed;
    uint32_t        biClrImportant;
} BITMAPINFOHEADER;


#endif

typedef union {
    struct _bitmapHeader {
        BITMAPFILEHEADER bfh;
        BITMAPINFOHEADER bih;
    } h;
    uint8_t b[sizeof(struct _bitmapHeader)];
} BitmapFileHeader_t;

#include "pop_pack.h"

class GFXCanvasCompressed;

class GFXCanvasBitmapStream : public Stream {
public:
    GFXCanvasBitmapStream(GFXCanvasCompressed& canvas, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    GFXCanvasBitmapStream(GFXCanvasCompressed& canvas);

    virtual int available();
    virtual int read();

    operator bool() const {
        return _available != 0;
    }

    virtual int peek() {
        return -1;
    }

    virtual size_t write(uint8_t) {
        return 0;
    }

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
