/**
Author: sascha_lammers@gmx.de
*/

#include <EnumHelper.h>
#include "WiFiCallbacks.h"

#if DEBUG_WIFICALLBACKS
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

WiFiCallbacks::CallbackVector WiFiCallbacks::_callbacks;
bool WiFiCallbacks::_locked = false;

WiFiCallbacks::EventType WiFiCallbacks::add(EventType events, Callback callback, CallbackPtr_t callbackPtr)
{
    _debug_printf_P(PSTR("events=%u, callbackPtr=%p callback=%p\n"), events, resolve_lambda((void *)callbackPtr), resolve_lambda(lambda_target(callback)));

    events = EnumHelper::Bitset::getAnd(events, EventType::ANY);

    for (auto &callback : _callbacks) {
        if (callbackPtr == callback.callbackPtr) {
            _debug_printf_P(PSTR("callbackPtr=%p, changed events from %u to %u\n"), callbackPtr, callback.events, events);
            // callback.events |= events;
            callback.events = EnumHelper::Bitset::getOr(callback.events, events);
            return callback.events;
        }
    }
    _debug_printf_P(PSTR("callbackPtr=%p new entry, events %d\n"), callbackPtr,  events);
    _callbacks.push_back(CallbackEntry_t({events, callback, callbackPtr}));
    return events;
}

WiFiCallbacks::EventType WiFiCallbacks::remove(EventType events, CallbackPtr_t callbackPtr)
{
    _debug_printf_P(PSTR("events=%u, callbackPtr=%p\n"), events, resolve_lambda((void *)callbackPtr));
    for (auto iterator = _callbacks.begin(); iterator != _callbacks.end(); ++iterator) {
        if (callbackPtr == iterator->callbackPtr) {
            _debug_printf_P(PSTR("callbackPtr=%p changed events from %u to %u\n"), callbackPtr, iterator->events, iterator->events & ~events);
            // iterator->events &= ~events;
            iterator->events = EnumHelper::Bitset::getAnd(iterator->events, EnumHelper::Bitset::getInverted(events));
            if (iterator->events == EventType::NONE) {
                if (!_locked) {
                    _callbacks.erase(iterator);
                }
                return EventType::NONE;
            }
            return iterator->events;
        }
    }
    return EventType::CALLBACK_NOT_FOUND;
}

void WiFiCallbacks::callEvent(EventType event, void *payload)
{
    _debug_printf_P(PSTR("event=%u payload=%p\n"), event, payload);
    _locked = true;
    for (const auto &entry : _callbacks) {
        if (EnumHelper::Bitset::hasAny(entry.events, event)) {
            if (entry.callback) {
                _debug_printf_P(PSTR("callback=%p\n"), resolve_lambda(lambda_target(entry.callback)));
                entry.callback(event, payload);
            } else {
                _debug_printf_P(PSTR("callbackPtr=%p\n"), resolve_lambda((void *)entry.callbackPtr));
                entry.callbackPtr(event, payload);
            }
        }
    }
    _callbacks.erase(std::remove_if(_callbacks.begin(), _callbacks.end(), [](const CallbackEntry_t &wc) {
        return wc.events == EventType::NONE;
    }), _callbacks.end());
    _locked = false;
}

WiFiCallbacks::CallbackVector &WiFiCallbacks::getVector()
{
    return _callbacks;
}
