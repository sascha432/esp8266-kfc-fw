/**
  Author: sascha_lammers@gmx.de
*/

#include "LoopFunctions.h"

#if DEBUG_LOOP_FUNCTIONS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

static LoopFunctions::FunctionsVector _functions;

void LoopFunctions::add(LoopFunctions::Callback_t callback, CallbackPtr_t callbackPtr) {

    _debug_printf_P(PSTR("LoopFunctions::add(%p)\n"), callbackPtr);

    auto result = std::find_if(_functions.begin(), _functions.end(), [&](const FunctionEntry_t &entry) {
        return (entry.callbackPtr == callbackPtr);
    });
    if (result == _functions.end()) {
        _functions.push_back({callback, callbackPtr, false});
    } else {
        _debug_printf_P(PSTR("LoopFunctions::add(): callbackPtr already exists, deleted state %d\n"), result->deleteCallback);
        if (result->deleteCallback) {
            result->deleteCallback = 0;
        }
    }
}

void LoopFunctions::remove(CallbackPtr_t callbackPtr) {

    _debug_printf_P(PSTR("LoopFunctions::remove(%p)\n"), callbackPtr);

    auto result = std::find_if(_functions.begin(), _functions.end(), [&](const FunctionEntry_t &entry) {
        return (entry.callbackPtr == callbackPtr);
    });
    if (result != _functions.end()) {
        if (!result->deleteCallback) {
            result->deleteCallback = 1;
        }
    } else {
        _debug_printf_P(PSTR("LoopFunctions::_remove(): Cannot find callbackPtr\n"));
    }
}

LoopFunctions::FunctionsVector &LoopFunctions::getVector() {
    return _functions;
}
