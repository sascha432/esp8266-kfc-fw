/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

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

namespace GFXCanvas {

    union BitmapFileHeaderType {
        struct _bitmapHeader {
            BITMAPFILEHEADER bfh;
            BITMAPINFOHEADER bih;
        } h;
        uint8_t b[sizeof(struct _bitmapHeader)];

        BitmapFileHeaderType() : b{}
        {}
    };

    // check if structures have the correct size
    static_assert(sizeof(BITMAPFILEHEADER) == 14, "Invalid size");
    static_assert(sizeof(BITMAPINFOHEADER) == 40, "Invalid size");
    static_assert(sizeof(BitmapFileHeaderType) == 54, "Invalid size");

}

#include "pop_pack.h"
