/**
  Author: sascha_lammers@gmx.de
*/

#include "LoopFunctions.h"

#if DEBUG_LOOP_FUNCTIONS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if ESP32

#define SCHEDULED_FN_MAX_COUNT 32

static std::vector<LoopFunctions::Callback_t> scheduled_functions;


bool ICACHE_RAM_ATTR schedule_function (const std::function<void(void)>& fn)
{
    if (fn) {
        if (scheduled_functions.size() < SCHEDULED_FN_MAX_COUNT) {
            scheduled_functions.emplace_back(fn);
            return true;
        }
    }
    return false;
}

void run_scheduled_functions()
{
    for(auto fn: scheduled_functions) {
        fn();
    }
    scheduled_functions.clear();
}

#endif

static LoopFunctions::FunctionsVector _functions;

void LoopFunctions::add(LoopFunctions::Callback_t callback, CallbackPtr_t callbackPtr)
{
    _debug_printf_P(PSTR("callbackPtr=%p\n"), resolve_lambda((void *)callbackPtr));
    for(auto &entry: _functions) {
        if (entry.callbackPtr == callbackPtr) {
            _debug_printf_P(PSTR("callbackPtr=%p already exists, deleted state %d\n"), callbackPtr, entry.deleteCallback);
            entry.deleteCallback = false; // restore if deleted
            return;
        }
    }
    _functions.push_back(FunctionEntry_t({callback, callbackPtr, false}));
}

void LoopFunctions::remove(CallbackPtr_t callbackPtr)
{
    _debug_printf_P(PSTR("callbackPtr=%p\n"), resolve_lambda((void *)callbackPtr));
    for(auto &entry: _functions) {
        if (entry.callbackPtr == callbackPtr) {
            entry.deleteCallback = true;
            return;
        }
    }
    _debug_printf_P(PSTR("cannot find callbackPtr=%p\n"), callbackPtr);
}

LoopFunctions::FunctionsVector &LoopFunctions::getVector()
{
    return _functions;
}

size_t LoopFunctions::size()
{
    size_t count = 0;
    for(auto &entry: _functions) {
        if (!entry.deleteCallback) {
            count++;
        }
    }
    return count;
}
