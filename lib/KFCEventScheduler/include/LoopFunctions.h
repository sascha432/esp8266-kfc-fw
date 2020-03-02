/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_LOOP_FUNCTIONS
#define DEBUG_LOOP_FUNCTIONS            1
#include <debug_helper_enable.h>
#endif

#include <Arduino_compat.h>
#if ESP8266
#include <Schedule.h>
#elif ESP32
#include <EventScheduler.h>
#endif
#include <functional>
#include <vector>

class LoopFunctions {
public:
    typedef std::function<void(void)> Callback_t;
    typedef void(*CallbackPtr_t)();

    typedef struct  {
        Callback_t callback;
        CallbackPtr_t callbackPtr;
        bool deleteCallback;
    } FunctionEntry_t;

    typedef std::vector<FunctionEntry_t> FunctionsVector;

    static void clear() {
        getVector().clear();
    }

    static void add(Callback_t callback, CallbackPtr_t callbackPtr);  // for lambda functions, use any unique pointer as callbackPtr
    static void add(CallbackPtr_t callbackPtr) {
        add(nullptr, callbackPtr);
    }
    static bool callOnce(Callback_t callback) {
#if ESP32
#warning TODO find schedule_function()
        Scheduler.addTimer(1, false, [callback](EventScheduler::TimerPtr) {
            callback();
        });
        return true;
#else
        _debug_resolve_lambda(lambda_target(callback));
        return schedule_function(callback);
#endif
    }
    static void remove(CallbackPtr_t callbackPtr);

    inline static void add(Callback_t callback, void *callbackPtr) {
        add(callback, reinterpret_cast<CallbackPtr_t>(callbackPtr));
    }
    inline static void remove(void *callbackPtr) {
        _debug_resolve_lambda(callbackPtr);
        remove(reinterpret_cast<CallbackPtr_t>(callbackPtr));
    }

    static FunctionsVector &getVector();
    static size_t size();
};

#include <debug_helper_disable.h>
