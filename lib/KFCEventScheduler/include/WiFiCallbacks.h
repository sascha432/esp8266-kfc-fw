/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <functional>
#include <vector>

#ifndef DEBUG_WIFICALLBACKS
#define DEBUG_WIFICALLBACKS                         0
#include <debug_helper_enable.h>
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

    typedef std::vector<Entry> CallbackVector;

    static void clear() {
        getVector().clear();
    }

    static EventType add(EventType events, Callback callback, CallbackPtr callbackPtr);
    static EventType add(EventType events, Callback callback, void *callbackPtr);
    static EventType add(EventType events, CallbackPtr callbackPtr);
    // returns -1 if not found, 0 if the callback has been removed or EventType of callbacks left for this pointer
    static EventType remove(EventType events, CallbackPtr callbackPtr);
    static EventType remove(EventType events, void *callbackPtr);

    static void callEvent(EventType event, void *payload);
    static CallbackVector &getVector();

private:
    static CallbackVector _callbacks;
    static bool _locked;
};

#include <debug_helper_disable.h>
