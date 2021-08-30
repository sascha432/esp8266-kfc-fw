/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#if LOGGER
#include <logger.h>
#endif
#include "GFXCanvasConfig.h"

#pragma GCC push_options
#if DEBUG_GFXCANVAS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace GFXCanvas;

#if DEBUG_GFXCANVAS_BOUNDS

bool __debug_GFXCanvasBounds_printf(const DebugContext &p, const char *format, ...)
{
    PrintString message;

    DEBUG_OUTPUT.println(FPSTR(format));

    va_list arg;
    va_start(arg, format);
    message.vprintf_P(format, arg);
    va_end(arg);
    if (p.isActive()) {
        p.prefix();
        p.getOutput().println(message);
    }
#if LOGGER
    Logger_error(message);
#endif
    return true;
}

#endif
