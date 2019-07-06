/**
Author: sascha_lammers@gmx.de
*/

#include "WiFiCallbacks.h"
#include <debug_helper.h>

static WiFiCallbacks::CallbackVector _callbacks;

uint8_t WiFiCallbacks::add(uint8_t events, WiFiCallbacks::Callback_t callback, WiFiCallbacks::CallbackPtr_t callbackPtr) {
    debug_printf_P(PSTR("WiFiCallbacks::add(%u, %p)\n"), events, callbackPtr);

    events &= EventEnum_t::ANY;

    for (auto &callback : _callbacks) {
        if (callbackPtr == callback.callbackPtr) {
            debug_printf_P(PSTR("WiFiCallbacks::add(): %p, changed events from %u to %u\n"), callbackPtr, callback.events, events);
            callback.events |= events;
            return callback.events;
        }
    }
    debug_printf_P(PSTR("WiFiCallbacks::add(): callbackPtr %p new entry, events %d\n"), callbackPtr,  events);
    _callbacks.push_back({events, callback, callbackPtr});
    return events;
}

uint8_t WiFiCallbacks::remove(uint8_t events, WiFiCallbacks::CallbackPtr_t callbackPtr) {
    debug_printf_P(PSTR("WiFiCallbacks::remove(%u, %p)\n"), events, callbackPtr);
    _callbacks.erase(std::remove_if(_callbacks.begin(), _callbacks.end(), [&](WiFiCallbacks::CallbackEntry_t &entry) {
        if (entry.callbackPtr == callbackPtr) {
            debug_printf_P(PSTR("WiFiCallbacks::remove(): callbackPtr %p changed events from %u to %u\n"), callbackPtr, entry.events, events);
            entry.events = events;
            return entry.events == 0;
        }
        return false;
    }), _callbacks.end());
    return events;
}

void WiFiCallbacks::callEvent(WiFiCallbacks::EventEnum_t event, void *payload) {
    debug_printf_P(PSTR("WiFiCallbacks::callEvent(%u, %p)\n"), event, payload);
    for (const auto entry : _callbacks) {
        if (entry.events & event) {
            if (entry.callback) {
                entry.callback(event, payload);
            } else {
                entry.callbackPtr(event, payload);
            }
        }
    }
}

WiFiCallbacks::CallbackVector &WiFiCallbacks::getVector() {
    return _callbacks;
}
