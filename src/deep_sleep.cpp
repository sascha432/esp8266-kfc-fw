/**
 * Author: sascha_lammers@gmx.de
 */

#if ENABLE_DEEP_SLEEP

#include "deep_sleep.h"
#include <user_interface.h>

#if IOT_REMOTE_CONTROL
#include "../src/plugins/remote/remote.h"
#endif


DeepSleep::PinState deepSleepPinState;
DeepSleep::DeepSleepParam deepSleepParams;

using namespace DeepSleep;

extern "C" void preinit (void)
{
#if IOT_REMOTE_CONTROL
    // store states of buttons
    // all pins are reset to input before
    deepSleepPinState.init();

    if (deepSleepPinState.anyPressed()) {
        deepSleepParams = DeepSleepParam();
        return;
    }
#endif

    if (RTCMemoryManager::read(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams))) {
        if (deepSleepParams.isValid()) {
            auto nextSleepTime = deepSleepParams._updateRemainingTime();
            if (nextSleepTime == 0) {
                deepSleepParams._remainingSleepTime = deepSleepParams._totalSleepTime;
                deepSleepParams._currentSleepTime = 0;
            }
            else {
                deepSleepParams._currentSleepTime = nextSleepTime;
            }

            resetDetector.clearCounter();
            deepSleepParams._counter++;
            deepSleepParams._runtime += micros();
            RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams));

            if (nextSleepTime == 0) {
                // RemoteControlPlugin::getInstance()._minAwakeTimeout = 15000;
                //TODO
            }
            else {
                // go back to sleep
                // system_deep_sleep_set_option(static_cast<int>(deepSleepParams._mode));
                // system_deep_sleep_instant(deepSleepParams._currentSleepTime * 1000ULL);
                // esp_yield();
                // system_deep_sleep_instant(0);
                // esp_yield();
            }
        }
    }
}

void DeepSleepParam::enterDeepSleep(milliseconds time)
{
    // create a new object if we do not have a valid one
    if (!deepSleepParams.isValid()) {
        deepSleepParams = DeepSleepParam(time);
    }
    deepSleepParams.updateRemainingTime();
    RTCMemoryManager::write(RTCMemoryManager::RTCMemoryId::DEEP_SLEEP, &deepSleepParams, sizeof(deepSleepParams));

    resetDetector.clearCounter();

#if defined(ESP8266)
    ESP.deepSleep(deepSleepParams.getDeepSleepTimeMicros(), deepSleepParams._mode);
    // try again with half of the max. time if first call fails
    __DBG_printf_E("Deep sleep failed with time=%.0f ms", deepSleepParams.getDeepSleepTimeMicros() / 1000.0);
    ESP.deepSleep(ESP.deepSleepMax() / 2, deepSleepParams._mode);
    ESP.deepSleep(0, deepSleepParams._mode);
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

#endif
