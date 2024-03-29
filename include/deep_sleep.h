/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// collects input PIN state during boot and supports deep sleep without the 3 hour limit
// using deep sleep for longer periods of time causes the system to wake up every 3 hours to check if the time has passed. that takes
// about 60-70ms. the total amount per day sums up to ~0.8 seconds. this can be optimized by turning the wifi modem off during boot unless a key is pressed
// otherwise it will affect quick connect by ~30ms
// TODO: check how to improve that in framework 3.0, since it behaves differently than 2.x

#if ENABLE_DEEP_SLEEP

#ifndef __DEEP_SLEEP_INCLUDED
#define __DEEP_SLEEP_INCLUDED
#define __DEEP_SLEEP_INSIDE_INCLUDE

#include <Arduino_compat.h>
#include <chrono>
#include <array>
#include <PinMonitor.h>
#include <BitsToStr.h>
#include <stl_ext/array.h>
#if IOT_REMOTE_CONTROL
#include "../src/plugins/remote/remote_def.h"
#endif

#    ifndef DEBUG_DEEP_SLEEP
#        define DEBUG_DEEP_SLEEP 0
#    endif

// include methods in source code = 0, or in header as always inline = 1
// use 1 for production, speeds everything up and reduces code size
// use 0 for development to avoid recompiling half the program for every change
#    ifndef DEEP_SLEEP_INCLUDE_HPP_INLINE
#        if DEBUG_DEEP_SLEEP
#            define DEEP_SLEEP_INCLUDE_HPP_INLINE 1
#        else
#            define DEEP_SLEEP_INCLUDE_HPP_INLINE 1
#        endif
#    endif

#    ifndef DEEP_SLEEP_BUTTON_ACTIVE_STATE
#        define DEEP_SLEEP_BUTTON_ACTIVE_STATE PIN_MONITOR_ACTIVE_STATE
#    endif

#    ifndef DEEP_SLEEP_BUTTON_PINS
#        ifdef IOT_REMOTE_CONTROL
#            define DEEP_SLEEP_BUTTON_PINS IOT_REMOTE_CONTROL_BUTTON_PINS
#        else
#            error DEEP_SLEEP_BUTTON_PINS not defined
#        endif
#    endif

namespace DeepSleep {

    using milliseconds = std::chrono::duration<uint32_t, std::milli>;
    using seconds = std::chrono::duration<uint32_t, std::ratio<1>>;
    using minutes = std::chrono::duration<uint32_t, std::ratio<60>>;

    static constexpr auto kPinsToRead = stdex::array_of<const uint8_t>(DEEP_SLEEP_BUTTON_PINS);

    // bitmask to detect buttons pressed
    static constexpr auto kButtonMask = Interrupt::PinAndMask::mask_of(kPinsToRead);


    struct WiFiQuickConnect
    {
        int16_t channel;
        uint8_t wifi_config_num;
        bool use_static_ip;

        struct Uint8Array {
            union {
                uint8_t _data[WL_MAC_ADDR_LENGTH];
                uint32_t _alignedData[(WL_MAC_ADDR_LENGTH + 3) / 4];
            };

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
            Uint8Array() : _alignedData{} {}
        } bssid;

        uint32_t local_ip;
        uint32_t dns1;
        uint32_t dns2;
        uint32_t subnet;
        uint32_t gateway;

        WiFiQuickConnect() :
            channel(0),
            wifi_config_num(0),
            use_static_ip(false),
            // __alignment1(0),
            // __alignment2(0),
            bssid(),
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

        uint32_t _first_pressed[16];
        uint32_t _state;
        #if DEBUG_PIN_STATE
            uint32_t _time;
            uint32_t _states[16];
            uint32_t _times[16];
            uint8_t _count;
        #endif

        PinState();

        static constexpr bool activeLow() {
            return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_LOW;
        }
        static constexpr bool activeHigh() {
            return PIN_MONITOR_ACTIVE_STATE == PinMonitor::ActiveStateType::PRESSED_WHEN_HIGH;
        }

        // initialize and read pin states
        void init();
        // read pin states. this method should be executed in regular intervals to keep the timestamps in order
        void merge();

        // returns true if at least one reading has performed
        bool isValid() const;

        // returns true if any pin is active
        bool anyPressed() const;

        // get the active state for the pin, inverted if activeLow() == true
        bool getState(uint8_t pin) const;
        // get active states for all pins, inverted if activeLow() == true
        uint32_t getStates() const;
        // get the GPIO states for all pins, always non inverted
        uint32_t getGPIOStates() const;
        // get timestamp in micros when the key press has been detected the first time
        uint32_t getFirstPressed(uint8_t pin) const;

        String toString(uint32_t state = ~0U, uint32_t time = 0) const;

        // read all GPIOs and store pin states, states are stored inverted if activeLow() == true
        uint32_t _readStates() const;
        // set GPIO states
        void _setStates(uint32_t state);
        // merge active GPIO states
        void _mergeStates(uint32_t state);
        // update timestamp to detect the order of the key pressed during boot. the merge function is required to be called frequently to poll the states
        void _updateFirstPressed(uint32_t time);

    };


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

        DeepSleepParam();
        DeepSleepParam(WakeupMode wakeupMode);
        DeepSleepParam(minutes deepSleepTime, RFMode mode = RF_NO_CAL);
        DeepSleepParam(seconds deepSleepTime, RFMode mode = RF_NO_CAL);
        DeepSleepParam(milliseconds deepSleepTime, RFMode rfMode = RF_NO_CAL);

        #if DEBUG_DEEP_SLEEP
            void dump();
        #endif

        static void enterDeepSleep(milliseconds time, RFMode rfMode = RF_NO_CAL);
        static void reset();
        bool isValid() const;
        static uint32_t getDeepSleepMaxMillis();
        float getTotalTime() const;
        void updateRemainingTime();
        uint32_t getDeepSleepTimeMillis() const;
        uint64_t getDeepSleepTimeMicros() const;
        uint32_t _updateRemainingTime();
        WakeupMode getWakeupMode() const;
        void setRFMode(RFMode rfMode);

    };

    static constexpr size_t DeepSleepParamSize = sizeof(DeepSleepParam);

    extern void preinit();

    extern PinState &deepSleepPinState;
    extern DeepSleepParam &deepSleepParams;
    extern bool enableWiFiOnBoot;

}

#    if DEEP_SLEEP_INCLUDE_HPP_INLINE
#        include "deep_sleep.hpp"
#    endif

#endif

#undef __DEEP_SLEEP_INSIDE_INCLUDE
#endif
