/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#if DEBUG_GFXCANVAS_STATS

#pragma GCC push_options
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

#include "GFXCanvasStats.h"

namespace GFXCanvas {

    void Stats::dump(Print &output) const
    {
        #if DEBUG_GFXCANVAS_STATS_DETAILS
            output.printf_P(PSTR("de/encode %u/%.2fms %u/%.2fms mops %u/%u cache hits/miss %u/%u flush %u/%u drawInto avg %.3fms free %u low %u\n"),
                decode_count, decode_time, encode_count, encode_time,
                malloc, free,
                cache_hits, cache_miss, cache_flush, cache_writes,
                drawInto / drawIntoCount / 1000.0,
                ESP.getFreeHeap(),
                heap_low
            );
        #else
            output.printf_P(PSTR("drawInto avg %.3fms heap free %u low %u\n"),
                drawInto / drawIntoCount / 1000.0, ESP.getFreeHeap(), heap_low
            );
        #endif
    }

    Stats stats;

}

#pragma GCC pop_options

#endif
