/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#if DEBUG_GFXCANVASCOMPRESSED_STATS

#include <push_optimize.h>
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasStats.h"

using namespace GFXCanvas;

void Stats::dump(Print &output) const {
#if !DEBUG_GFXCANVASCOMPRESSED_STATS_DETAILS
    output.printf_P(PSTR("drawInto %.3fms\n"),
        drawInto / 1000.0
    );
#else
    output.printf_P(PSTR("de/encode %u/%.2fms %u/%.2fms mops %u cacheR %u F %u D %u max %ub drawInto %.3fms\n"),
        decode_count, decode_time, encode_count, encode_time,
        malloc,
        cache_read, cache_flush, cache_drop, cache_max, drawInto / 1000.0
    );
#endif
}


#include <pop_optimize.h>

#endif
