/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "Event.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Event;

RepeatType::RepeatType(uint32_t repeat) : _repeat(repeat)
{
    // __LDBG_assert_panic(_repeat != kPreset, "invalid value");
#if DEBUG
    assert(_repeat != kPreset);
#endif
}


