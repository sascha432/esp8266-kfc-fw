/**
 * Author: sascha_lammers@gmx.de
 */

#if ENABLE_DEEP_SLEEP

#include <stl_ext/memory.h>
#include "reset_detector.h"
#include "deep_sleep.h"
#include "save_crash.h"
#if DEBUG_DEEP_SLEEP
#include "logger.h"
#endif
#include "../src/plugins/plugins.h"
#include <plugins_menu.h>

#    undef __LDBG_printf
#    if DEBUG_DEEP_SLEEP
#        define __LDBG_printf(fmt, ...) ::printf_P(PSTR("DS%04u: " fmt "\n"), micros() / 1000, ##__VA_ARGS__)
#    else
#        define __LDBG_printf(...)
#    endif

using DeepSleepPinStateUninitialized = stdex::UninitializedClass<DeepSleep::PinState>;
static DeepSleepPinStateUninitialized deepSleepPinStateNoInit __attribute__((section(".noinit")));
DeepSleep::PinState &deepSleepPinState = deepSleepPinStateNoInit._object;

using namespace DeepSleep;

inline static void deep_sleep_wakeup_wifi()
{
    #if ARDUINO_ESP8266_MAJOR >= 3
    // Starting from arduino core v3: wifi is disabled at boot time
    // WiFi.begin() or WiFi.softAP() will wake WiFi up
    // #error TODO enable wifi or remove disabling via __disableWiFiAtBootTime
        #warning TODO check if this enables wifi when being called or in general
        enableWiFiAtBootTime();
    #endif
}

inline static void deep_sleep_preinit()
{
    uint32_t realTimeOffset = 0;
    // store states of all PINs
    deepSleepPinState.init();

    if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams))) {
        if (deepSleepParams.isValid()) {
            // set offset to the amount of time spent in deep sleep
            // the last deep sleep cycle is not added unless the deep sleep ends
            // if the device has been woken up by the user, the time cannot be determined
            // deepSleepParams._currentSleepTime is not subtracted here... since we cannot
            // know how much of the time has passed already
            realTimeOffset = (deepSleepParams._totalSleepTime - deepSleepParams._remainingSleepTime) + (deepSleepParams._runtime / 1000);
            #if DEBUG_DEEP_SLEEP
                __LDBG_printf("real time offset without the last cycle: %.6f", DeepSleep::_realTimeOffset / 1000000.0);
            #endif
        }
    }

    if (!resetDetector.hasWakeUpDetected()) {
        #if DEBUG_DEEP_SLEEP
            __LDBG_printf("reason: %s", resetDetector.getResetReason());
        #endif
        RTCMemoryManager::remove(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP);
        RTCMemoryManager::updateTimeOffset((realTimeOffset + (micros() / 1000));
        return;
    }

    // if kButtonMask is set, check if any of the buttons is pressed and exit deep sleep
    if (kButtonMask && deepSleepPinState.anyPressed()) {
        #if DEBUG_DEEP_SLEEP
            __LDBG_printf("reason: USER_ACTION");
        #endif
        RTCMemoryManager::remove(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP);
        deepSleepParams = DeepSleepParam(WakeupMode::USER);
        deep_sleep_wakeup_wifi(); // speed up wifi connection
        return;
    }

    #if IOT_REMOTE_CONTROL
        if (digitalRead(IOT_REMOTE_CONTROL_CHARGING_PIN)) {
            #if DEBUG_DEEP_SLEEP
                __LDBG_printf("reason: CHARGING_DETECTED");
            #endif
            RTCMemoryManager::remove(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP);
            deepSleepParams = DeepSleepParam(WakeupMode::USER);
            return;
        }
    #endif

    __LDBG_printf("reason: %s", resetDetector.getResetReason());

    if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams))) {
        #if DEBUG_DEEP_SLEEP
                deepSleepParams.dump();
        #endif

        if (deepSleepParams.isValid()) {

            // clear reset counter
            resetDetector.clearCounter();

            auto nextSleepTime = deepSleepParams._updateRemainingTime();
            if (nextSleepTime == 0) {

                #if DEBUG_DEEP_SLEEP
                    __LDBG_printf("wakeup (total=%ums remaining=%ums time=%ums wakeup-runtime/counter=%uµs/%u)",
                        deepSleepParams._totalSleepTime, deepSleepParams._remainingSleepTime, deepSleepParams._currentSleepTime, deepSleepParams._runtime, deepSleepParams._counter
                    );
                #endif

                deepSleepParams._remainingSleepTime = 0;
                deepSleepParams._currentSleepTime = 0;
                deepSleepParams._wakeupMode = WakeupMode::AUTO;
                // deep_sleep_wakeup_wifi(); // speed up wifi connection

            }
            else {
                deepSleepParams._currentSleepTime = nextSleepTime;
                deepSleepParams._counter++;
                deepSleepParams._runtime += micros() + 75/* it takes ~75µs system_deep_sleep_instant() call*/;
                RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams));
            }

            if (nextSleepTime != 0) {
                #if DEBUG_DEEP_SLEEP
                    __LDBG_printf("going back to sleep (total=%ums remaining=%ums time=%ums wakeup-runtime/counter=%uµs/%u)",
                        deepSleepParams._totalSleepTime, deepSleepParams._remainingSleepTime, deepSleepParams._currentSleepTime, deepSleepParams._runtime, deepSleepParams._counter
                    );
                #endif
                // go back to sleep
                system_deep_sleep_set_option(static_cast<int>(deepSleepParams._rfMode));
                system_deep_sleep_instant(deepSleepParams._currentSleepTime * 1000ULL);
                esp_yield();
                system_deep_sleep_instant(0);
                esp_yield();
                /*  NO RETURN */
                do {} while(true) ;
            }
            else {
                RTCMemoryManager::updateTimeOffset(realTimeOffset);
            }
        }
#if DEBUG_DEEP_SLEEP
        else {
            __LDBG_printf("invalid parameter");
        }
    }
    else {
        __LDBG_printf("failed to read state from RTC memory");
#endif
    }
}

void DeepSleepParam::reset()
{
    deepSleepParams = DeepSleepParam();
    RTCMemoryManager::remove(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP);
}

#if DEBUG_DEEP_SLEEP
void DeepSleepParam::dump() {
    __LDBG_printf("_totalSleepTime=%u", _totalSleepTime);
    __LDBG_printf("_currentSleepTime=%u", _currentSleepTime);
    __LDBG_printf("_remainingSleepTime=%u", _remainingSleepTime);
    __LDBG_printf("_runtime=%u", _runtime);
    __LDBG_printf("_counter=%u", _counter);
    __LDBG_printf("_rfMode=%u", _rfMode);
    __LDBG_printf("_wakeupMode=%u", _wakeupMode);
}
#endif

void DeepSleepParam::enterDeepSleep(milliseconds sleep_time, RFMode rfMode)
{
    deepSleepParams = DeepSleepParam(sleep_time, rfMode);

    #if DEBUG_DEEP_SLEEP
        Logger_notice(F("entering deep sleep: time=" TIME_T_FMT " sleep-time=%.0f mode=%u"), time(nullptr), deepSleepParams.getTotalTime(), rfMode);
        delay(250);
    #endif

    deepSleepParams.updateRemainingTime();
    RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams));

    resetDetector.clearCounter();
    SaveCrash::removeCrashCounter();

    #if DEBUG_DEEP_SLEEP
        __LDBG_printf("deep sleep %.2f remaining=%u mode=%u", (deepSleepParams.getDeepSleepTimeMicros() / 1000.0), deepSleepParams._remainingSleepTime, deepSleepParams._rfMode);
    #endif

    #if defined(ESP8266)
        system_deep_sleep_set_option(static_cast<int>(deepSleepParams._rfMode));
        system_deep_sleep_instant(deepSleepParams.getDeepSleepTimeMicros());
        esp_yield();
        system_deep_sleep_instant(0);
        esp_yield();
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

extern "C" void preinit(void)
{
    resetDetectorNoInit.init();
    deepSleepPinStateNoInit.init();
    componentRegisterNoInit.init();
    resetDetector.begin();

    deep_sleep_preinit();
}

#if !DEEP_SLEEP_INCLUDE_HPP_INLINE
#include "deep_sleep.hpp"
#endif

#endif
