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

void LoopFunctions::add(LoopFunctions::Callback_t callback, CallbackPtr_t callbackPtr)
{
    _debug_printf_P(PSTR("LoopFunctions::add(%p)\n"), callbackPtr);
    for(auto &entry: _functions) {
        if (entry.callbackPtr == callbackPtr) {
            _debug_printf_P(PSTR("LoopFunctions::add(): callbackPtr=%p already exists, deleted state %d\n"), callbackPtr, entry.deleteCallback);
            entry.deleteCallback = false; // restore if deleted
            return;
        }
    }
    _functions.push_back({callback, callbackPtr, false});
}

void LoopFunctions::remove(CallbackPtr_t callbackPtr)
{
    _debug_printf_P(PSTR("LoopFunctions::remove(%p)\n"), callbackPtr);
    for(auto &entry: _functions) {
        if (entry.callbackPtr == callbackPtr) {
            entry.deleteCallback = true;
            return;
        }
    }
    _debug_printf_P(PSTR("LoopFunctions::_remove(): Cannot find callbackPtr=%p\n"), callbackPtr);
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
