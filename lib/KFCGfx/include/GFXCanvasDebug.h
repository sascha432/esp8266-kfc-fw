/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if DEBUG_GFXCANVASCOMPRESSED_STATS
    #define __DBG_STATS(...)                                __VA_ARGS__
    #if DEBUG_GFXCANVASCOMPRESSED_STATS_DETAILS
        #define __DBG_ASTATS(...)                           __VA_ARGS__
    #else
        #define __DBG_ASTATS(...)
    #endif
#else
#define __DBG_STATS(...)
#define __DBG_ASTATS(...)
#endif

#if DEBUG_GFXCANVAS_BOUNDS

#define __DBG_BOUNDS(...)                                   __VA_ARGS__
#define __DBG_BOUNDS_ACTION(cond, action)                   { if (cond) { action; } }
#define __DBG_BOUNDS_RETURN(cond)                           { if (cond) { return; } }
#define __DBG_check_x(x, minw, maxw)                        ((x < minw || x >= maxw) ? (DEBUG_GFXCANVAS_BOUNDS_TYPE("out of bounds x=%d [%d:%d]", x, minw, maxw) && true) : false)
#define __DBG_check_y(y, minh, maxh)                        ((y < minh || y >= maxh) ? (DEBUG_GFXCANVAS_BOUNDS_TYPE("out of bounds y=%d [%d:%d]", y, minh, maxh) && true) : false)
#define __DBG_check_sx(x, maxw)                             __DBG_check_x(x, 0, maxw)
#define __DBG_check_sy(y, maxh)                             __DBG_check_y(y, 0, maxh)
#define __DBG_check_ex(x, minw, maxw)                       __DBG_check_x(x, minw, maxw + 1)
#define __DBG_check_ey(y, minh, maxh)                       __DBG_check_y(y, minh, maxh + 1)
#define __DBG_check_xy(x, y, minw, maxw, minh, maxh)        ((x < minw || x >= maxw || y < minh || y >= maxh) ? (DEBUG_GFXCANVAS_BOUNDS_TYPE("out of bounds x=%d [%d:%d] y=%d [%d:%d]", x, minw, maxw, y, minh, maxh) && true) : false)
#define __DBG_check_buffer(ptr, start, end)                 ((ptr < start || ptr >= end) ? (DEBUG_GFXCANVAS_BOUNDS_TYPE("out of bounds ptr=%p begin=%p end=%p", ptr, start, end) && true) : false)
#define __DBG_check_buffer_end(ptr, end)                    ((ptr >= end) ? (DEBUG_GFXCANVAS_BOUNDS_TYPE("out of bounds ptr=%p end=%p", ptr, end) && true) : false)
#define __DBG_check_palette_index(i, p)                     if ((size_t)i >= (p).length() || (size_t)i >= (p).size()) { DEBUG_GFXCANVAS_BOUNDS_TYPE("out of bounds index=%u len=%d size=%d", i, (p).length(), (p).size()); delay(1000); return _palette[0]; }
#define __DBG_check_assert(cond)                            (!(cond) ? (DEBUG_GFXCANVAS_BOUNDS_TYPE("assert failed: %u == %s", cond, PSTR(_STRINGIFY(cond))) && true) : false)

#else

#define __DBG_BOUNDS(...)
#define __DBG_BOUNDS_ACTION(...)
#define __DBG_BOUNDS_RETURN(...)
#define __DBG_check_x(...)
#define __DBG_check_y(...)
#define __DBG_check_sx(...)
#define __DBG_check_sy(...)
#define __DBG_check_ex(...)
#define __DBG_check_ey(...)
#define __DBG_check_xy(...)
#define __DBG_check_buffer(...)
#define __DBG_check_buffer_end(...)
#define __DBG_check_palette_index(...)
#define __DBG_check_assert(...)

#endif

namespace GFXCanvas {

}
