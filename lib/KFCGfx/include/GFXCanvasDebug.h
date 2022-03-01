/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#if DEBUG_GFXCANVAS_STATS
#    define __DBG_STATS(...)                                __VA_ARGS__
#    if DEBUG_GFXCANVAS_STATS_DETAILS
#        define __DBG_ASTATS(...)                           __VA_ARGS__
#    else
#        define __DBG_ASTATS(...)
#    endif
#else
#    define __DBG_STATS(...)
#    define __DBG_ASTATS(...)
#endif

#if DEBUG_GFXCANVAS_BOUNDS

bool __debug_GFXCanvasBounds_printf(const DebugContext &p, const char *format, ...);

#define __DBG_GFXCANVAS_PRINTF(format, ...)                 __debug_GFXCanvasBounds_printf(DebugContext_ctor(), PSTR(format), ##__VA_ARGS__)

#define __DBG_BOUNDS(...)                                   __VA_ARGS__
#define __DBG_BOUNDS_ACTION(cond, action)                   { if (cond) { action; } }
#define __DBG_BOUNDS_RETURN(cond)                           { if (cond) { return; } }
#define __DBG_BOUNDS_x(x, minw, maxw)                       ((x < minw || x >= maxw) ? __DBG_GFXCANVAS_PRINTF("out of bounds x=%d [%d:%d]", x, minw, maxw) : false)
#define __DBG_BOUNDS_y(y, minh, maxh)                       ((y < minh || y >= maxh) ? __DBG_GFXCANVAS_PRINTF("out of bounds y=%d [%d:%d]", y, minh, maxh) : false)
#define __DBG_BOUNDS_sx(x, maxw)                            __DBG_BOUNDS_x(x, 0, maxw)
#define __DBG_BOUNDS_sy(y, maxh)                            __DBG_BOUNDS_y(y, 0, maxh)
#define __DBG_BOUNDS_ex(x, minw, maxw)                      __DBG_BOUNDS_x(x, minw, maxw + 1)
#define __DBG_BOUNDS_ey(y, minh, maxh)                      __DBG_BOUNDS_y(y, minh, maxh + 1)
#define __DBG_BOUNDS_xy(x, y, minw, maxw, minh, maxh)       ((x < minw || x >= maxw || y < minh || y >= maxh) ? __DBG_GFXCANVAS_PRINTF("out of bounds x=%d [%d:%d] y=%d [%d:%d]", x, minw, maxw, y, minh, maxh) : false)
#define __DBG_BOUNDS_buffer(ptr, start, end)                ((ptr < start || ptr >= end) ? __DBG_GFXCANVAS_PRINTF("out of bounds ptr=%p begin=%p end=%p", ptr, start, end) : false)
#define __DBG_BOUNDS_buffer_end(ptr, end)                   ((ptr >= end) ? __DBG_GFXCANVAS_PRINTF("out of bounds ptr=%p end=%p", ptr, end) : false)
#define __DBG_BOUNDS_assert(cond)                           __DBG_assert_printf(cond, "assert failed")
#define __DBG_BOUNDS_assertp(cond, fmt, ...)                __DBG_assert_printf(cond, fmt, ##__VA_ARGS__)

#else

#define __DBG_BOUNDS(...)
#define __DBG_BOUNDS_ACTION(...)
#define __DBG_BOUNDS_RETURN(...)
#define __DBG_BOUNDS_x(...)
#define __DBG_BOUNDS_y(...)
#define __DBG_BOUNDS_sx(...)
#define __DBG_BOUNDS_sy(...)
#define __DBG_BOUNDS_ex(...)
#define __DBG_BOUNDS_ey(...)
#define __DBG_BOUNDS_xy(...)
#define __DBG_BOUNDS_buffer(...)
#define __DBG_BOUNDS_buffer_end(...)
#define __DBG_BOUNDS_assert(...)
#define __DBG_BOUNDS_assertp(...)

#endif
