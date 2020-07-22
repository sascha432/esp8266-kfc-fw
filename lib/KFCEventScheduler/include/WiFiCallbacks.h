/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_WIFICALLBACKS
#define DEBUG_WIFICALLBACKS                         0
#include <debug_helper_enable.h>
#endif

#include <Arduino_compat.h>
#include <functional>
#include <vector>

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
    typedef void(* CallbackPtr_t)(EventType event, void *payload);

    typedef struct {
        EventType events;
        Callback callback;
        CallbackPtr_t callbackPtr;
    } CallbackEntry_t;

    typedef std::vector<CallbackEntry_t> CallbackVector;

    static void clear() {
        getVector().clear();
    }

    static EventType add(EventType events, Callback callback, CallbackPtr_t callbackPtr);
    static EventType add(EventType events, Callback callback, void *callbackPtr) {
#if DEBUG_WIFICALLBACKS
        _debug_resolve_lambda(lambda_target(callback));
#endif
        return add(events, callback, reinterpret_cast<CallbackPtr_t>(callbackPtr));
    }
    static EventType add(EventType events, CallbackPtr_t callbackPtr) {
#if DEBUG_WIFICALLBACKS
        _debug_resolve_lambda((void *)callbackPtr);
#endif
        return add(events, nullptr, callbackPtr);
    }
    // returns -1 if not found, 0 if the callback has been removed or EventType of callbacks left for this pointer
    static EventType remove(EventType events, CallbackPtr_t callbackPtr);
    static EventType remove(EventType events, void *callbackPtr) {
        return remove(events, reinterpret_cast<CallbackPtr_t>(callbackPtr));
    }

    static void callEvent(EventType event, void *payload);
    static CallbackVector &getVector();

private:
    static CallbackVector _callbacks;
    static bool _locked;
};

#include <debug_helper_disable.h>
