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

WiFiCallbacks::EventType WiFiCallbacks::add(EventType events, Callback callback, CallbackPtr callbackPtr)
{
    __SLDBG_printf("events=%u callbackPtr=%p callback=%p", events, callbackPtr, lambda_target(callback));

    // events &= EventType::ANY;
    events = EnumHelper::Bitset::getAnd(events, EventType::ANY);

    for (auto &callback : _callbacks) {
        if (callbackPtr == callback.callbackPtr) {
            __SLDBG_printf("callbackPtr=%p, changed events from %u to %u", callbackPtr, callback.events, events);
            // callback.events |= events;
            EnumHelper::Bitset::addBits(callback.events, events);
            return callback.events;
        }
    }
    __SLDBG_printf("callbackPtr=%p new entry events %d", callbackPtr, events);
    _callbacks.emplace_back(events, callback, callbackPtr);
    return events;
}

WiFiCallbacks::EventType WiFiCallbacks::add(EventType events, Callback callback, void *callbackPtr)
{
    return WiFiCallbacks::add(events, callback, reinterpret_cast<CallbackPtr>(callbackPtr));
}

WiFiCallbacks::EventType WiFiCallbacks::add(EventType events, CallbackPtr callbackPtr)
{
    return add(events, nullptr, callbackPtr);
}

WiFiCallbacks::EventType WiFiCallbacks::remove(EventType events, CallbackPtr callbackPtr)
{
    __SLDBG_printf("events=%u callbackPtr=%p", events, callbackPtr);
    for (auto iterator = _callbacks.begin(); iterator != _callbacks.end(); ++iterator) {
        if (callbackPtr == iterator->callbackPtr) {
            __SLDBG_printf("callbackPtr=%p changed events from %u to %u", callbackPtr, iterator->events, iterator->events & ~events);
            // iterator->events &= ~events;
            EnumHelper::Bitset::removeBits(iterator->events, events);
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

WiFiCallbacks::EventType WiFiCallbacks::remove(EventType events, void *callbackPtr)
{
    return remove(events, reinterpret_cast<CallbackPtr>(callbackPtr));
}

void WiFiCallbacks::callEvent(EventType event, void *payload)
{
    __SLDBG_printf("event=%u payload=%p", event, payload);
    _locked = true;
    for (const auto &entry : _callbacks) {
        if (EnumHelper::Bitset::hasAny(entry.events, event)) {
            if (entry.callback) {
                __SLDBG_printf("callback=%p", lambda_target(entry.callback));
                entry.callback(event, payload);
            } else {
                __SLDBG_printf("callbackPtr=%p", entry.callbackPtr);
                entry.callbackPtr(event, payload);
            }
        }
    }
    _callbacks.erase(std::remove(_callbacks.begin(), _callbacks.end(), EventType::NONE), _callbacks.end());
    _callbacks.shrink_to_fit();
    _locked = false;
}
