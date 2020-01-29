/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_LOOP_FUNCTIONS
#define DEBUG_LOOP_FUNCTIONS            0
#endif

#include <Arduino_compat.h>
#include <Schedule.h>
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
        return schedule_function(callback);
    }
    static void remove(CallbackPtr_t callbackPtr);

    static FunctionsVector &getVector();
    static size_t size();
};
