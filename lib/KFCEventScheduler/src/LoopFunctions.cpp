/**
  Author: sascha_lammers@gmx.de
*/

#include "LoopFunctions.h"

#if DEBUG_LOOP_FUNCTIONS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if _MSC_VER || ESP32

#include <vector>

#define SCHEDULED_FN_MAX_COUNT 32

static std::vector<std::function<void(void)>> scheduled_functions;

bool IRAM_ATTR schedule_function (const std::function<void(void)> &fn)
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
        #if _MSC_VER
            _ASSERTE(_CrtCheckMemory());
        #endif
    }
    scheduled_functions.clear();
}

#endif

LoopFunctions::FunctionsVector LoopFunctions::_functions;


void LoopFunctions::add(Callback callback, CallbackPtr callbackPtr)
{
    auto iterator = std::find(_functions.begin(), _functions.end(), callbackPtr);
    if (iterator == _functions.end()) {
        _functions.emplace_back(callback, callbackPtr, false);
        return;
    }
    iterator->deleteCallback = false;
}
