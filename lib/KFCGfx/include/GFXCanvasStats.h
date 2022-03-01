/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#if DEBUG_GFXCANVAS_STATS

namespace GFXCanvas {

    class Stats {
    public:
        Stats() {
            memset(this, 0, offsetof(Stats, heap_low));
            heap_low = 0x7fffffff;
        }

        void clear() {
            memset(this, 0, offsetof(Stats, heap_low));
        }

        void dump(Print &output) const;

        int decode_count;
        int encode_count;
        double decode_time;
        double encode_time;
        int decode_invalid;
        int encode_invalid;
        #if DEBUG_GFXCANVAS_STATS_DETAILS
            int cache_hits;
            int cache_miss;
            int cache_writes;
            int cache_flush;
            uint32_t drawInto;
            uint32_t drawIntoCount;
            int malloc;
            int free;
        #endif
        uint32_t heap_low;
    };

    extern Stats stats;

}

#endif
