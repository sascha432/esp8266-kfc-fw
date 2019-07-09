/**
  Author: sascha_lammers@gmx.de
*/


#pragma once

#include <Arduino_compat.h>
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

    static void add(Callback_t callback, CallbackPtr_t callbackPtr);  // for lambda functions, use any unique pointer as callbackPtr
    static void add(CallbackPtr_t callbackPtr) {
      add(nullptr, callbackPtr);
    }
    static void remove(CallbackPtr_t callbackPtr);

    static FunctionsVector &getVector();
};
