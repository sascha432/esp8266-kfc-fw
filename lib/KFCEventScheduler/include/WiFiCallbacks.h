/**
  Author: sascha_lammers@gmx.de
*/


#pragma once

#ifndef DEBUG_WIFICALLBACKS
#define DEBUG_WIFICALLBACKS 0
#endif

#include <Arduino_compat.h>
#include <functional>
#include <vector>

class WiFiCallbacks {
public:
    typedef enum {
        CONNECTED           = 0x01,
        DISCONNECTED        = 0x02,
        MODE_CHANGE         = 0x04,
        ANY                 = CONNECTED|DISCONNECTED|MODE_CHANGE
    } EventEnum_t;

    typedef std::function<void(uint8_t event, void *payload)> Callback_t;
    typedef void(*CallbackPtr_t)(uint8_t event, void *payload);

    typedef struct {
        uint8_t events;
        Callback_t callback;
        CallbackPtr_t callbackPtr;
    } CallbackEntry_t;

    typedef std::vector<CallbackEntry_t> CallbackVector;

    static uint8_t add(uint8_t events, Callback_t callback, CallbackPtr_t callbackPtr);
    static uint8_t add(uint8_t events, CallbackPtr_t callbackPtr) {
        return add(events, nullptr, callbackPtr);
    }
    static uint8_t remove(uint8_t events, CallbackPtr_t callbackPtr);

    static void callEvent(EventEnum_t event, void *payload);
    static CallbackVector &getVector();
};
