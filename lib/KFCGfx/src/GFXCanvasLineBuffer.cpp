/**
* Author: sascha_lammers@gmx.de
*/


#include <Arduino_compat.h>
#include "GFXCanvasConfig.h"

#pragma GCC push_options
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#pragma GCC optimize ("O3")
#endif

using namespace GFXCanvas;

#include "GFXCanvasLineBuffer.h"

#pragma GCC pop_options
