/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>
#include <EnumHelper.h>

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#ifndef DEBUG_WIFICALLBACKS
#    define DEBUG_WIFICALLBACKS 0
#endif

class WiFiCallbacks {
public:
    enum class EventType : int8_t {
        CALLBACK_NOT_FOUND = -1,
        NONE = 0,
        CONNECTED = 0x01,     // connect occurs after a successful connection has been established and an IP address has been assigned. It might occur again if a new IP gets assigned, even without disconnect before
        DISCONNECTED = 0x02,     // disconnect can only occur after connect
        MODE_CHANGE = 0x04,     // currently not supported
        CONNECTION = CONNECTED|DISCONNECTED,
        ANY = CONNECTED|DISCONNECTED|MODE_CHANGE
    };

    using Callback = std::function<void(EventType event, void *payload)>;
    typedef void(* CallbackPtr)(EventType event, void *payload);

    class Entry {
    public:
        Entry(EventType pEvents, Callback pCallback, CallbackPtr pCallbackPtr) : events(pEvents), callback(pCallback), callbackPtr(pCallbackPtr) {}

        bool operator==(EventType type) {
            return events == type;
        }

        EventType events;
        Callback callback;
        CallbackPtr callbackPtr;
    };

    using CallbackVector = std::vector<Entry>;

    inline __attribute__((__always_inline__))
    static void clear() {
        _callbacks.clear();
    }

    static EventType add(EventType events, Callback callback, CallbackPtr callbackPtr);
    static EventType add(EventType events, Callback callback, void *callbackPtr);
    static EventType add(EventType events, CallbackPtr callbackPtr);
    // returns -1 if not found, 0 if the callback has been removed or EventType of callbacks left for this pointer
    static EventType remove(EventType events, CallbackPtr callbackPtr);
    static EventType remove(EventType events, void *callbackPtr);

    static void callEvent(EventType event, void *payload);

    inline __attribute__((__always_inline__))
    static CallbackVector &getVector() {
        return _callbacks;
    }

private:
    static CallbackVector _callbacks;
    static bool _locked;
};

inline WiFiCallbacks::EventType WiFiCallbacks::add(EventType events, Callback callback, CallbackPtr callbackPtr)
{
    __SLDBG_printf("events=%u callbackPtr=%p callback=%p", events, callbackPtr, lambda_target(callback));

    // events &= EventType::ANY;
    events = EnumHelper::Bitset::getAnd(events, EventType::ANY);

    for (auto &callback : _callbacks) {
        if (callbackPtr == callback.callbackPtr) {
            __SLDBG_printf("callbackPtr=%p, changed events from %u to %u", callbackPtr, callback.events, events);
            // callback.events |= events;
            return (callback.events = EnumHelper::Bitset::addBits(callback.events, events));
        }
    }
    __SLDBG_printf("callbackPtr=%p new entry events %d", callbackPtr, events);
    _callbacks.emplace_back(events, callback, callbackPtr);
    return events;
}

inline WiFiCallbacks::EventType WiFiCallbacks::add(EventType events, Callback callback, void *callbackPtr)
{
    return WiFiCallbacks::add(events, callback, reinterpret_cast<CallbackPtr>(callbackPtr));
}

inline WiFiCallbacks::EventType WiFiCallbacks::add(EventType events, CallbackPtr callbackPtr)
{
    return add(events, nullptr, callbackPtr);
}

inline WiFiCallbacks::EventType WiFiCallbacks::remove(EventType events, CallbackPtr callbackPtr)
{
    __SLDBG_printf("events=%u callbackPtr=%p", events, callbackPtr);
    for (auto iterator = _callbacks.begin(); iterator != _callbacks.end(); ++iterator) {
        if (callbackPtr == iterator->callbackPtr) {
            __SLDBG_printf("callbackPtr=%p changed events from %u to %u", callbackPtr, iterator->events, EnumHelper::Bitset::removeBits(iterator->events, events));
            // iterator->events &= ~events;
            iterator->events = EnumHelper::Bitset::removeBits(iterator->events, events);
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

inline WiFiCallbacks::EventType WiFiCallbacks::remove(EventType events, void *callbackPtr)
{
    return remove(events, reinterpret_cast<CallbackPtr>(callbackPtr));
}

inline void WiFiCallbacks::callEvent(EventType event, void *payload)
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

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif
