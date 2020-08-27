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
    EVENT_SCHEDULER_ASSERT(_repeat != kPreset);
}
