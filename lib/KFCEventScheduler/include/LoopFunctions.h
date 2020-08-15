/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

// #ifndef DEBUG_LOOP_FUNCTIONS
// #define DEBUG_LOOP_FUNCTIONS            0
// #include <debug_helper_enable.h>
// #endif

#include <Arduino_compat.h>
#if ESP8266
#include <Schedule.h>
#elif ESP32
#include <EventScheduler.h>
#endif
#include <functional>
#include <vector>

#if ESP32
bool schedule_function (const std::function<void(void)>& fn);
void run_scheduled_functions();
#endif

class LoopFunctions {
public:
    using Callback = std::function<void(void)>;
    typedef void(*CallbackPtr)();

    class Entry {
    public:
        Entry(Callback pCallback, CallbackPtr pCallbackPtr, bool pDeleteCallback) : callback(pCallback), callbackPtr(pCallbackPtr), deleteCallback(pDeleteCallback) {}

        Callback callback;
        CallbackPtr callbackPtr;
        bool deleteCallback;
    };

    using FunctionsVector = std::vector<Entry> ;

    static void clear() {
        getVector().clear();
    }

    static void add(Callback callback, CallbackPtr callbackPtr);  // for lambda functions, use any unique pointer as callbackPtr
    static void add(CallbackPtr callbackPtr) {
        add(nullptr, callbackPtr);
    }
    static bool callOnce(Callback callback) {
        return schedule_function(callback);
    }
    static void remove(CallbackPtr callbackPtr);

    inline static void add(Callback callback, void *callbackPtr) {
        add(callback, reinterpret_cast<CallbackPtr>(callbackPtr));
    }
    inline static void remove(void *callbackPtr) {
        remove(reinterpret_cast<CallbackPtr>(callbackPtr));
    }

    static FunctionsVector &getVector();
    static size_t size();

};

#include <debug_helper_disable.h>
