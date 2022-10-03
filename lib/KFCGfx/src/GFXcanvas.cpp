/**
 * Author: sascha_lammers@gmx.de
 */

#include "GFXCanvasConfig.h"
#include <Arduino_compat.h>

#pragma GCC push_options

#if DEBUG_GFXCANVAS
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#    pragma GCC optimize("O3")
#endif

#include "GFXCanvas.h"

namespace GFXCanvas {

}

#pragma GCC pop_options
