/**
 * Author: sascha_lammers@gmx.de
 */

#if ENABLE_DEEP_SLEEP

#include "deep_sleep.h"
#include <user_interface.h>
#if DEBUG_DEEP_SLEEP
#include "logger.h"
#endif

#if IOT_REMOTE_CONTROL
#include "../src/plugins/remote/remote.h"
#endif


DeepSleep::PinState deepSleepPinState;
DeepSleep::DeepSleepParam deepSleepParams;

using namespace DeepSleep;

inline static void deep_sleep_preinit()
{
    // store states of all PINs
    deepSleepPinState.init();

#if DEBUG_DEEP_SLEEP
    Serial0.begin(KFC_SERIAL_RATE);
#endif
    if (ESP.getResetInfoPtr()->reason != REASON_DEEP_SLEEP_AWAKE) {
#if DEBUG_DEEP_SLEEP
        ::printf_P(PSTR("reset reason not REASON_DEEP_SLEEP_AWAKE\n"));
#endif
        RTCMemoryManager::remove(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP);
        return;
    }

    // if kButtonMask is set, check if any of the buttons is pressed and exit deep sleep
    if (kButtonMask && deepSleepPinState.anyPressed()) {
#if DEBUG_DEEP_SLEEP
        ::printf_P(PSTR("deep sleep: user awake\n"));
#endif
        deepSleepParams = DeepSleepParam(WakeupMode::USER);
        return;
    }

    if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams))) {
        if (deepSleepParams.isValid()) {

            // clear reset counter
            resetDetector.clearCounter();

            auto nextSleepTime = deepSleepParams._updateRemainingTime();
            if (nextSleepTime == 0) {

#if DEBUG_DEEP_SLEEP
                ::printf_P(PSTR("deep sleep: wakeup (total=%ums remaining=%ums time=%ums wakeup-runtime/counter=%uµs/%u)\n"),
                    deepSleepParams._totalSleepTime, deepSleepParams._remainingSleepTime, deepSleepParams._currentSleepTime, deepSleepParams._runtime, deepSleepParams._counter
                );
#endif

                deepSleepParams._remainingSleepTime = 0;
                deepSleepParams._currentSleepTime = 0;
#if !RTC_SUPPORT
                struct timeval tv = { static_cast<time_t>(deepSleepParams._realTime + ((deepSleepParams._totalSleepTime + (deepSleepParams._runtime / 1000U)) / 1000U)), 0 };
                settimeofday(&tv, nullptr);
                ::printf_P(PSTR("stored.time=%ld total=%.3f runtime=%.6f est.tikme=%ld\n"), deepSleepParams._realTime, deepSleepParams._totalSleepTime / 1000.0, deepSleepParams._runtime / 1000000.0, time(nullptr));
#endif
            }
            else {
                deepSleepParams._currentSleepTime = nextSleepTime;
                deepSleepParams._counter++;
                deepSleepParams._runtime += micros() + 75/* it takes ~75µs system_deep_sleep_instant() call*/;
                RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams));
            }

            if (nextSleepTime == 0) {
                deepSleepParams._wakeupMode = WakeupMode::AUTO;
#if DEBUG_DEEP_SLEEP
                ::printf_P(PSTR("deep sleep: auto awake cycle\n"));
#endif
            }
            else {
#if DEBUG_DEEP_SLEEP
                ::printf_P(PSTR("deep sleep: going back to sleep (total=%ums remaining=%ums time=%ums wakeup-runtime/counter=%uµs/%u)\n"),
                    deepSleepParams._totalSleepTime, deepSleepParams._remainingSleepTime, deepSleepParams._currentSleepTime, deepSleepParams._runtime, deepSleepParams._counter
                );
#endif
                // go back to sleep
                system_deep_sleep_set_option(static_cast<int>(deepSleepParams._rfMode));
                system_deep_sleep_instant(deepSleepParams._currentSleepTime * 1000ULL);
                esp_yield();
                system_deep_sleep_instant(0);
                esp_yield();
            }
        }
#if DEBUG_DEEP_SLEEP
        else {
            ::printf_P(PSTR("deep sleep: invalid parameter\n"));
        }
    }
    else {
        ::printf_P(PSTR("deep sleep: failed to read state from RTC\n"));
#endif
    }
}

void DeepSleepParam::enterDeepSleep(milliseconds sleep_time)
{
    deepSleepParams = DeepSleepParam(sleep_time);

#if DEBUG_DEEP_SLEEP
    Logger_notice(F("going to deep sleep, time=%ld sleep-time=%.3f"), time(nullptr), deepSleepParams.getTotalTime());
#endif

    deepSleepParams.updateRemainingTime();
    deepSleepParams.setRealTime(time(nullptr));
    RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams));

    resetDetector.clearCounter();

#if DEBUG_DEEP_SLEEP
    ::printf_P(PSTR("deep sleep %.2f remaining=%u\n"), (deepSleepParams.getDeepSleepTimeMicros() / 1000.0), deepSleepParams._remainingSleepTime);
#endif
#if defined(ESP8266)
    ESP.deepSleep(deepSleepParams.getDeepSleepTimeMicros(), deepSleepParams._rfMode);
    // try again with half of the max. time if first call fails
#if DEBUG_DEEP_SLEEP
    ::printf_P(PSTR("deep sleep failed with time=%.0f ms\n"), deepSleepParams.getDeepSleepTimeMicros() / 1000.0);
#endif
    ESP.deepSleep(getDeepSleepMaxMillis() / 2, deepSleepParams._rfMode);
    ESP.deepSleep(0, deepSleepParams._rfMode);
#else
    ESP.deepSleep(deepSleepParams.getDeepSleepTimeMicros());
    ESP.deepSleep(0);
#endif

}

String PinState::toString(uint32_t state, uint32_t time) const
{
    if (state == ~0U) {
        state = _state;
        time = _time;
    }
    auto str = PrintString(F("pins="));
    for(auto pin: kPinsToRead) {
        str += state & _BV(pin) ? '1' : '0';
    }
    str.printf_P(PSTR(" state=%s time=%u actve_low=%u"),
        BitsToStr<17, true>(state).c_str(),
        time,
        activeLow()
    );
    return str;
}

#if DEBUG_DEEP_SLEEP

void deep_sleep_setup()
{
    deep_sleep_preinit();
}

#else

extern "C" void preinit(void)
{
    deep_sleep_preinit()
}

#endif


#endif
