/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if ENABLE_DEEP_SLEEP

#include <Arduino_compat.h>
#include <chrono>
#include <array>
#include <PinMonitor.h>
#include <BitsToStr.h>
#include <stl_ext/array.h>
#if IOT_REMOTE_CONTROL
#include "../src/plugins/remote/remote_def.h"
#endif

#define DEBUG_DEEP_SLEEP 1

#ifndef DEBUG_DEEP_SLEEP
    #define DEBUG_DEEP_SLEEP                                0
#endif

#ifndef DEEP_SLEEP_BUTTON_ACTIVE_STATE
    #define DEEP_SLEEP_BUTTON_ACTIVE_STATE                  PIN_MONITOR_ACTIVE_STATE
#endif

#ifndef DEEP_SLEEP_BUTTON_PINS
    #ifdef IOT_REMOTE_CONTROL
        #define DEEP_SLEEP_BUTTON_PINS                      IOT_REMOTE_CONTROL_BUTTON_PINS
    #else
        #error DEEP_SLEEP_BUTTON_PINS not defined
    #endif
#endif

namespace DeepSleep
{
    using milliseconds = std::chrono::duration<uint32_t, std::milli>;
    using seconds = std::chrono::duration<uint32_t, std::ratio<1>>;
    using minutes = std::chrono::duration<uint32_t, std::ratio<60>>;

    static constexpr auto kPinsToRead = stdex::array_of<uint8_t>(DEEP_SLEEP_BUTTON_PINS);

    // bitmask to detect buttons pressed
    static constexpr auto kButtonMask = Interrupt::PinAndMask::mask_of(kPinsToRead);


    struct __attribute__packed__ WiFiQuickConnect
    {
        int16_t channel: 15;                    //  0
        int16_t use_static_ip: 1;               // +2 byte
        struct Uint8Array {
            uint8_t _data[WL_MAC_ADDR_LENGTH];      // +6 byte
            operator uint8_t *() {
                return _data;
            }
            operator const uint8_t *() const {
                return _data;
            }
            Uint8Array &operator=(const uint8_t *data) {
                std::copy_n(data, WL_MAC_ADDR_LENGTH, _data);
                return *this;
            }
            Uint8Array() = default;
        } bssid;

        uint32_t local_ip;
        uint32_t dns1;
        uint32_t dns2;
        uint32_t subnet;
        uint32_t gateway;

        WiFiQuickConnect() :
            channel(0),
            use_static_ip(0),
            bssid({}),
            local_ip(0),
            dns1(0),
            dns2(0),
            subnet(0),
            gateway(0)
        {}

    };
    static constexpr auto kWiFiQuickConnectSize = sizeof(WiFiQuickConnect);
    static_assert(kWiFiQuickConnectSize % 4 == 0, "this is stored in RTC memory and needs to be dword aligned");

    #define DEBUG_PIN_STATE 1

    struct PinState {

        uint32_t _time;
        uint32_t _state;
#if DEBUG_PIN_STATE
        uint32_t _states[16];
        uint32_t _times[16];
        uint8_t _count;
#endif

        PinState() : _time(0), _state(0) {
        }

        static constexpr bool activeLow() {
            return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_LOW;
        }
        static constexpr bool activeHigh() {
            return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_HIGH;
        }

        void init() {
            _time = system_get_time();
#if DEBUG_PIN_STATE
            _states[0] = _readStates();
            _times[0] = micros();
            _count = 1;
            _setStates(_states[0]);
#else
            _setStates(_readStates());
#endif
        }

        void merge() {
#if DEBUG_PIN_STATE
            _states[_count] = _readStates();
            _times[_count] = micros();
            _mergeStates(_states[_count]);
            if (_count < sizeof(_states) / sizeof(_states[0]) - 1) {
                _count++;
            }
#else
            _mergeStates(_readStates());
#endif
        }


        // returns true if at least one reading has performed
        bool isValid() const;

        // returns true if any pin is active
        bool anyPressed() const;

        // get the active state for the pin
        // active high: true for value != 0
        // active low:  true for value == 0
        bool getState(uint8_t pin) const;
        // get value that was read from the pin
        bool getValue(uint8_t pin) const;
        // get active states for all pins
        uint32_t getStates() const;
        // get values (states for actgive high and inverted states for active low)
        uint32_t getValues() const;
        // get values from states
        uint32_t getValues(uint32_t states) const;

        // time of objection creation
        uint32_t getMillis() const;
        uint32_t getMicros() const;

        String toString(uint32_t state = ~0U, uint32_t time = 0) const;

        // read all GPIOs
        uint32_t _readStates() const;
        // set GPIO states
        void _setStates(uint32_t state);
        // merge active GPIO states
        void _mergeStates(uint32_t state);

    };

    inline __attribute__((__always_inline__))
    bool PinState::isValid() const {
        return _time != 0;
    }

    inline __attribute__((__always_inline__))
    bool PinState::anyPressed() const {
        return _state & kButtonMask;
    }

    inline __attribute__((__always_inline__))
    uint32_t PinState::getStates() const {
        return _state;
    }

    inline __attribute__((__always_inline__))
    uint32_t PinState::getValues() const {
        return getValues(getStates());
    }

    inline __attribute__((__always_inline__))
    uint32_t PinState::getValues(uint32_t states) const {
        return activeHigh() ? states : ~states;
    }

    inline __attribute__((__always_inline__))
    bool PinState::getState(uint8_t pin) const {
        return _state & _BV(pin);
    }

    inline __attribute__((__always_inline__))
    bool PinState::getValue(uint8_t pin) const {
        return (_state & _BV(pin)) == activeHigh();
    }

    inline __attribute__((__always_inline__))
    uint32_t PinState::_readStates() const {
        uint32_t state = GPI;
        if (GP16I & 0x01) {
            state |= _BV(16);
        }
        return activeHigh() ? state : ~state;
    }

    inline __attribute__((__always_inline__))
    void PinState::_setStates(uint32_t state) {
        _state = state;
    }

    inline __attribute__((__always_inline__))
    void PinState::_mergeStates(uint32_t state) {
        _state |= state;
    }

    inline __attribute__((__always_inline__))
    uint32_t PinState::getMillis() const {
        return _time / 1000;
    }

    inline __attribute__((__always_inline__))
    uint32_t PinState::getMicros() const {
        return _time;
    }

    enum class WakeupMode {
        NONE,
        AUTO,
        USER
    };

    struct __attribute__packed__ DeepSleepParam {

        uint32_t _totalSleepTime;                   // total amount in milliseconds
        uint32_t _remainingSleepTime;               // time left in millis
        uint32_t _currentSleepTime;                 // the current cycle just woken up from
        uint32_t _runtime;                          // total runtime in microseconds during wakeups
        uint16_t _counter;                          // total count of deep sleep cycles for the total amount
        RFMode _rfMode;
        WakeupMode _wakeupMode;
        time_t _realTime;

        DeepSleepParam() : _totalSleepTime(0) {}
        DeepSleepParam(WakeupMode wakeupMode) : _totalSleepTime(0), _wakeupMode(wakeupMode) {}
        DeepSleepParam(minutes deepSleepTime, RFMode mode = RF_NO_CAL) : DeepSleepParam(std::chrono::duration_cast<milliseconds>(deepSleepTime), mode) {}
        DeepSleepParam(seconds deepSleepTime, RFMode mode = RF_NO_CAL) : DeepSleepParam(std::chrono::duration_cast<milliseconds>(deepSleepTime), mode) {}
        DeepSleepParam(milliseconds deepSleepTime, RFMode rfMode = RF_NO_CAL) :
            _totalSleepTime(deepSleepTime.count()),
            _remainingSleepTime(deepSleepTime.count()),
            _currentSleepTime(0),
            _runtime(0),
            _counter(0),
            _rfMode(rfMode),
            _wakeupMode(WakeupMode::NONE),
            _realTime(0)
        {
#if DEBUG_DEEP_SLEEP
            dump();
#endif
        }

#if DEBUG_DEEP_SLEEP
        void dump() {
            ::printf_P(PSTR("_totalSleepTime=%u\n"), _totalSleepTime);
            ::printf_P(PSTR("_currentSleepTime=%u\n"), _currentSleepTime);
            ::printf_P(PSTR("_remainingSleepTime=%u\n"), _remainingSleepTime);
            ::printf_P(PSTR("_runtime=%u\n"), _runtime);
            ::printf_P(PSTR("_counter=%u\n"), _counter);
            ::printf_P(PSTR("_rfMode=%u\n"), _rfMode);
            ::printf_P(PSTR("_wakeupMode=%u\n"), _wakeupMode);
            ::printf_P(PSTR("_realTime=%u\n"), _realTime);
        }
#endif

        static void enterDeepSleep(milliseconds time);
        static void reset();

        inline __attribute__((__always_inline__))
        bool isValid() const {
            return _totalSleepTime != 0 && _totalSleepTime != 0U;
        }

        inline __attribute__((__always_inline__))
        static uint32_t getDeepSleepMaxMillis() {
            return ESP.deepSleepMax() / (1000 + 150/*some extra margin*/);
        }

        // total sleep time in seconds
        inline __attribute__((__always_inline__))
        float getTotalTime() const {
            return _totalSleepTime / 1000.0;
        }

        inline __attribute__((__always_inline__))
        void setRealTime(time_t time) {
            if (IS_TIME_VALID(time)) {
                _realTime = time;
            }
            else {
                _realTime = 0;
            }
        }

        inline __attribute__((__always_inline__))
        time_t getRealTime() const {
            return _realTime;
        }

        inline __attribute__((__always_inline__))
        void updateRemainingTime() {
            _currentSleepTime = _updateRemainingTime();
        }

        inline __attribute__((__always_inline__))
        uint64_t getDeepSleepTimeMicros() const {
            return _currentSleepTime * 1000ULL;
        }

        uint32_t _updateRemainingTime() {
            auto maxSleep = getDeepSleepMaxMillis();
            if (_remainingSleepTime / 2 >= maxSleep) {
                _remainingSleepTime -= maxSleep;
                return maxSleep;
            }
            else if (_remainingSleepTime >= maxSleep) {
                _remainingSleepTime /= 2;
                return _remainingSleepTime;
            }
            else {
                maxSleep = _remainingSleepTime;
                _remainingSleepTime = 0;
                return maxSleep;
            }
        }

        inline __attribute__((__always_inline__))
        WakeupMode getWakeupMode() const {
            return _wakeupMode;
        }

        inline __attribute__((__always_inline__))
        void setRFMode(RFMode rfMode) {
            _rfMode = rfMode;
        }
    };

};

extern "C" DeepSleep::PinState deepSleepPinState;
extern "C" DeepSleep::DeepSleepParam deepSleepParams;

#if DEBUG_DEEP_SLEEP
void deep_sleep_setup();
#endif

#endif
