/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <chrono>

#ifndef DEBUG_EVENT_SCHEDULER
#define DEBUG_EVENT_SCHEDULER                           1
#endif

#ifndef DISABLE_GLOBAL_EVENT_SCHEDULER
#define DISABLE_GLOBAL_EVENT_SCHEDULER                  0
#endif

#if DEBUG_EVENT_SCHEDULER
#define __DBG_Event_Timer_store_position()              __DBG_store_position()
#define _Scheduler                                      ((__DBG_store_position()) ? __Scheduler : __Scheduler)
#define _Timer(obj)                                     ((__DBG_store_position()) ? obj : obj)
#else
#define _Scheduler                                      __Scheduler
#define _Timer(obj)                                     obj
#endif

#pragma push_macro("HIGH")
#pragma push_macro("LOW")
#pragma push_macro("DEFAULT")
#undef HIGH
#undef LOW
#undef DEFAULT

namespace Event {

    class Timer;
    class CallbackTimer;
    class Scheduler;

    static constexpr uint32_t kMinDelay = 5;
    static constexpr uint32_t kMaxDelay = 0x68D7A3;                                              // 6870947ms / 6870.947 seconds

    static constexpr uint64_t kMaxDelayMillis = (1ULL << 17) * (uint64_t)kMaxDelay + (kMaxDelay - 1);
    static constexpr uint64_t kMaxDelaySeconds = kMaxDelayMillis / 1000;
    static constexpr uint64_t kMaxDelayInDays  = kMaxDelaySeconds / 86400;

    // ESP8266 for "os_timer_arm"
    // - if system_timer_reinit has been called, the timer value allowed range from 100 to 0x689D0.
    // - if didn’t call system_timer_reinit has NOT been called, the timer value allowed range from 5 to 0x68D7A3.

    using CallbackTimerPtr = CallbackTimer *;
    using TimerVector = std::vector<CallbackTimerPtr>;
    using TimerVectorIterator = TimerVector::iterator;
    using Callback = std::function<void(CallbackTimerPtr timer)>;

    using milliseconds = std::chrono::duration<int64_t, std::ratio<1>>;

    template<typename T>
    constexpr milliseconds seconds(T seconds) {
        return milliseconds((milliseconds::rep)seconds * (milliseconds::rep)1000);
    }

    template<typename T>
    constexpr milliseconds minutes(T minutes) {
        return milliseconds((milliseconds::rep)minutes * (milliseconds::rep)60000);
    }

    enum class PriorityType : int8_t {
        NONE = -127,
        LOWEST = -100,
        LOWER = -75,
        LOW = -50,
        NORMAL = 0,
        HIGH = 50,       // above normal is limited to 15ms
        HIGHER = 75,
        HIGHEST = 100,
        MAX
    };

    class RepeatType {
    public:
        static const uint32_t kUnlimited = ~0;
        static const uint32_t kPreset = 0;

        // use preset
        RepeatType() : _repeat(kPreset) {}
        // repeat timer
        // false = call it one time
        // true = unlimited times
        RepeatType(bool repeat) : _repeat(repeat ? kUnlimited : 1) {}
        // repeat timer "repeat" times
        RepeatType(int repeat) : _repeat(static_cast<uint32_t>(repeat)) {}
        RepeatType(uint32_t repeat) : _repeat(repeat) {}

        RepeatType(const RepeatType &repeat) : _repeat(repeat._repeat) {}

    protected:
        friend Scheduler;
        friend CallbackTimer;

        RepeatType &operator|=(const RepeatType &repeat) {
            if (repeat._repeat != kPreset) {
                _repeat = repeat._repeat;
            }
            if (_repeat == 0) {
                _repeat = 1;
            }
            return *this;
        }

        bool _doRepeat() {
            if (_repeat == kUnlimited) {
                return true;
            }
            if (_repeat > 0) {
                _repeat--;
            }
            return _hasRepeat();
        }

        bool _hasRepeat() const {
            if (_repeat == kUnlimited) {
                return true;
            }
            return _repeat > 0 && (_repeat != kPreset);
        }

        uint32_t _repeat;
    };

}

#pragma pop_macro("DEFAULT")
#pragma pop_macro("HIGH")
#pragma pop_macro("LOW")
