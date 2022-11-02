/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <Buffer.h>

#ifndef DEBUG_GFXCANVAS
#    define DEBUG_GFXCANVAS 0
#endif

#ifndef DEBUG_GFXCANVAS_BOUNDS
#    define DEBUG_GFXCANVAS_BOUNDS 0
#endif

#ifndef DEBUG_GFXCANVAS_STATS
#    define DEBUG_GFXCANVAS_STATS 0
#endif

#ifndef DEBUG_GFXCANVAS_STATS_DETAILS
#    define DEBUG_GFXCANVAS_STATS_DETAILS DEBUG_GFXCANVAS_STATS
#endif

#if _MSC_VER
#    define DEBUG_GFXCANVAS_BOUNDS_CHECK 1
#    define GFXCANVAS_MAX_CACHED_LINES 1
// #    define GFXCANVAS_MAX_CACHED_LINES 16
#endif

// max. number of cached lines
#ifndef GFXCANVAS_MAX_CACHED_LINES
#    define GFXCANVAS_MAX_CACHED_LINES 1
#endif

// enable 4bit / 16 color .bmp files
#ifndef GFXCANVAS_SUPPORT_4BIT_BMP
#    define GFXCANVAS_SUPPORT_4BIT_BMP 1
#endif

// enable for debugging only
#ifndef DEBUG_GFXCANVAS_BOUNDS_CHECK
#    define DEBUG_GFXCANVAS_BOUNDS_CHECK DEBUG_GFXCANVAS_BOUNDS
#endif

#include <push_pack.h>

namespace GFXCanvas {

    using ColorType = uint16_t;
    using ColorIndexType = int8_t;
    using ColorPaletteType = ColorType;
    using RGBColorType = uint32_t;
    using ByteBufferSizeType = int16_t;
    using ByteBufferDataType = uint8_t;
    using ByteBuffer = Buffer;

    using uXType = uint16_t;
    using sXType = int16_t;
    using uYType = uint16_t;
    using sYType = int16_t;

    using uWidthType = uint16_t;
    using sWidthType = int16_t;
    using uHeightType = uint16_t;
    using sHeightType = int16_t;

    struct uPositionType {
        uXType x;
        uXType y;
    };

    struct uDimensionType {
        uWidthType w;
        uHeightType h;
    };

}

#include "GFXCanvasDebug.h"

#include <pop_pack.h>
