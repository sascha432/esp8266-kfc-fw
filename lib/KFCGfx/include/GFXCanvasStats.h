/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#if DEBUG_GFXCANVASCOMPRESSED_STATS

namespace GFXCanvas {

    class Stats {
    public:
        Stats() = default;
        void dump(Print &output) const;

        int decode_count;
        int encode_count;
        double decode_time;
        double encode_time;
        int decode_invalid;
        int encode_invalid;
        int cache_read;
        int cache_flush;
        int cache_drop;
        int cache_max;
        uint32_t drawInto;
        int malloc;
        int modified_skipped;
    };

}

#endif
