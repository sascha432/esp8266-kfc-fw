/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>

#ifndef DEBUG_GFXCANVAS
#define DEBUG_GFXCANVAS                             1
#endif

#ifndef DEBUG_GFXCANVAS_MEM
#define DEBUG_GFXCANVAS_MEM                         1
#endif

#ifndef DEBUG_GFXCANVAS_BOUNDS
#define DEBUG_GFXCANVAS_BOUNDS                      1
#endif

// #define DEBUG_GFXCANVASCOMPRESSED_STATS             1
// #define DEBUG_GFXCANVASCOMPRESSED_STATS_DETAILS     1

#if _MSC_VER
#define DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK      1
#define GFXCANVAS_MAX_CACHED_LINES                  16
#endif

// max. number of cached lines
#ifndef GFXCANVAS_MAX_CACHED_LINES
#define GFXCANVAS_MAX_CACHED_LINES                  1
#endif

// enable for debugging only
#ifndef DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK
#define DEBUG_GFXCANVASCOMPRESSED_BOUNDS_CHECK      1
#endif

#include <push_pack.h>

namespace GFXCanvas {

    using ColorType = uint16_t;
    using ColorPaletteType = ColorType;
    using RGBColorType = uint32_t;

    using uXType = uint16_t;
    using sXType = int16_t;
    using uYType = uint16_t;
    using sYType = int16_t;

    using uWidthType = uint16_t;
    using sWidthType = int16_t;
    using uHeightType = uint16_t;
    using sHeightType = int16_t;

    typedef struct {
        uXType x;
        uXType y;
    } uPositionType;

    typedef struct {
        uWidthType w;
        uHeightType h;
    } uDimensionType;

    typedef struct __attribute__packed__ {
        uint16_t x;
        uint16_t y;
        uint16_t width;
        uint16_t height;
        uint16_t paletteCount;
    } GFXCanvasRLEStreamHeader_t;

}

#include "GFXCanvasDebug.h"

#include <pop_pack.h>
