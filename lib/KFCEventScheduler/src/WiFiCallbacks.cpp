/**
Author: sascha_lammers@gmx.de
*/

#include "WiFiCallbacks.h"

#if DEBUG_WIFICALLBACKS
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

WiFiCallbacks::CallbackVector WiFiCallbacks::_callbacks;
bool WiFiCallbacks::_locked = false;

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif
