/**
  Author: sascha_lammers@gmx.de
*/

#include "LoopFunctions.h"

#if DEBUG_LOOP_FUNCTIONS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if defined(ESP32) || defined(_MSC_VER)

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
    // __SLDBG_printf("callbackPtr=%p callback=%p", callbackPtr, lambda_target(callback));
    // for(auto &entry: _functions) {
    //     if (entry.callbackPtr == callbackPtr) {
    //         __SLDBG_printf("callbackPtr=%p already exists, deleted state %d", callbackPtr, entry.deleteCallback);
    //         entry.deleteCallback = false; // restore if deleted
    //         return;
    //     }
    // }
    // _functions.emplace_back(callback, callbackPtr, false);
}

// void LoopFunctions::remove(CallbackPtr callbackPtr)
// {
    // __SLDBG_printf("callbackPtr=%p", callbackPtr);
    // for(auto &entry: _functions) {
    //     if (entry.callbackPtr == callbackPtr) {
    //         entry.deleteCallback = true;
    //         return;
    //     }
    // }
    // __SLDBG_printf("cannot find callbackPtr=%p", callbackPtr);
// }

// LoopFunctions::FunctionsVector &LoopFunctions::getVector()
// {
//     return _functions;
// }

// size_t LoopFunctions::size()
// {

//     size_t count = 0;
//     for(auto &entry: _functions) {
//         if (!entry.deleteCallback) {
//             count++;
//         }
//     }
//     return count;
// }
