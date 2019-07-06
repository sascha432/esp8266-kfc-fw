/**
  Author: sascha_lammers@gmx.de
*/

#include "LoopFunctions.h"
#include "debug_helper.h"

static LoopFunctions::FunctionsVector _functions;

void LoopFunctions::add(LoopFunctions::Callback_t callback, CallbackPtr_t callbackPtr) {

    debug_printf_P(PSTR("LoopFunctions::add(%p)\n"), callbackPtr);

    auto result = std::find_if(_functions.begin(), _functions.end(), [&](const FunctionEntry_t &entry) {
        return (entry.callbackPtr == callbackPtr);
    });
    if (result == _functions.end()) {
        _functions.push_back({callback, callbackPtr, false});
    } else {
        debug_printf_P(PSTR("LoopFunctions::add(): callbackPtr already exists, deleted state %d\n"), result->deleteCallback);
        if (result->deleteCallback) {
            result->deleteCallback = 0;
        }
    }
}

void LoopFunctions::remove(CallbackPtr_t callbackPtr) {

    debug_printf_P(PSTR("LoopFunctions::remove(%p)\n"), callbackPtr);

    auto result = std::find_if(_functions.begin(), _functions.end(), [&](const FunctionEntry_t &entry) {
        return (entry.callbackPtr == callbackPtr);
    });
    if (result != _functions.end()) {
        if (!result->deleteCallback) {
            result->deleteCallback = 1;
        }
    } else {
        debug_printf_P(PSTR("LoopFunctions::_remove(): Cannot find callbackPtr\n"));
    }
}

LoopFunctions::FunctionsVector &LoopFunctions::getVector() {
    return _functions;
}
