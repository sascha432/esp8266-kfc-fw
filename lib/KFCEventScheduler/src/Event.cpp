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

Event::RepeatType::RepeatType() : _repeat(kPreset)
{
}

Event::RepeatType::RepeatType(bool repeat) : _repeat(repeat ? kUnlimited : kNoRepeat)
{
}

Event::RepeatType::RepeatType(int repeat) : _repeat(static_cast<uint32_t>(repeat))
{
    // __LDBG_assert_panic(_repeat != kPreset, "invalid value");
#if DEBUG
    assert(_repeat != kPreset);
#endif
}

Event::RepeatType::RepeatType(uint32_t repeat) : _repeat(repeat)
{
    // __LDBG_assert_panic(_repeat != kPreset, "invalid value");
#if DEBUG
    assert(_repeat != kPreset);
#endif
}

Event::RepeatType::RepeatType(const RepeatType &repeat) : _repeat(repeat._repeat)
{
}

bool Event::RepeatType::_doRepeat()
{
    static_assert(kUnlimited > kNoRepeat, "check kUnlimited");
    if (_repeat == kUnlimited) {
        return true;
    }
    else if (_repeat > kNoRepeat) {
        _repeat--;
        return true;
    }
    return false;
}

bool Event::RepeatType::_hasRepeat() const
{
    static_assert(kNoRepeat + 1 > kNoRepeat && kNoRepeat + 1 != kPreset, "check kNoRepeat and kPreset");
    return _repeat > kNoRepeat; // && (_repeat != kPreset);
}

