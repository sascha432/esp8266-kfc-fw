/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

// #ifndef DEBUG_LOOP_FUNCTIONS
// #define DEBUG_LOOP_FUNCTIONS            0
// #include <debug_helper_enable.h>
// #endif

#include <Arduino_compat.h>
#if ESP8266
#    include <Schedule.h>
#elif ESP32
#    include <EventScheduler.h>
#endif
#include <functional>
#include <vector>

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#if defined(ESP32) || defined(_MSC_VER)
bool IRAM_ATTR schedule_function(const std::function<void(void)> &fn);
void run_scheduled_functions();
#endif

class LoopFunctions {
public:
    class Entry;

    using Callback = std::function<void(void)>;
    using CallbackPtr = void(*)(void);
    using FunctionsVector = std::vector<Entry>;

    struct CallbackId {
        constexpr CallbackId() : _id(nullptr) {}

        CallbackId(const void *id) : _id(id) {}
        CallbackId(const uint32_t id) : _id(reinterpret_cast<const void *>(id)) {}
        CallbackId(const CallbackPtr id) : _id(reinterpret_cast<const void *>(id)) {}

        operator CallbackPtr() const {
            return reinterpret_cast<CallbackPtr>(_id);
        }

        operator uint32_t() const {
            return reinterpret_cast<uint32_t>(_id);
        }

        operator int32_t() const {
            return reinterpret_cast<int32_t>(_id);
        }

        bool operator==(const void *value) const {
            return _id == value;
        }

        bool operator!=(const void *value) const {
            return _id != value;
        }

        bool operator==(const int value) const {
            return _id == reinterpret_cast<const void *>(value);
        }

        bool operator!=(const int value) const {
            return _id != reinterpret_cast<const void *>(value);
        }

    private:
        const void *_id;
    };

    class Entry {
    public:
        Entry(Callback pCallback, CallbackPtr pCallbackPtr, bool pDeleteCallback) : callback(pCallback), callbackPtr(pCallbackPtr), deleteCallback(pDeleteCallback) {}

        bool operator==(CallbackPtr value) const {
            return callbackPtr == value;
        }

        bool operator==(CallbackId value) const {
            return callbackPtr == value;
        }

        Callback callback;
        CallbackPtr callbackPtr;
        bool deleteCallback;
    };


    inline __attribute__((__always_inline__))
    static void clear() {
        _functions.clear();
    }

    static void add(Callback callback, CallbackPtr callbackPtr);  // for lambda functions, use any unique pointer as id

    inline __attribute__((__always_inline__))
    static void add(Callback callback, CallbackId id) {
        add(callback, static_cast<CallbackPtr>(id));
    }

    inline __attribute__((__always_inline__))
    static void add(CallbackPtr callbackPtr) {  // for functon pointers, the address of the function is used as id
        add(callbackPtr, callbackPtr);
    }

    inline __attribute__((__always_inline__))
    static void remove(CallbackPtr callbackPtr) {
        auto iterator = std::find(_functions.begin(), _functions.end(), callbackPtr);
        if (iterator != _functions.end()) {
            iterator->deleteCallback = true;
        }
    }

    inline __attribute__((__always_inline__))
    static void remove(CallbackId id) {
        remove(static_cast<CallbackPtr>(id));
    }

    inline __attribute__((__always_inline__))
    static size_t size() {
        return std::count_if(_functions.begin(), _functions.end(), [](const Entry &entry) { return entry.deleteCallback == false; });
    }

    inline __attribute__((__always_inline__))
    static bool empty() {
        return _functions.empty() || (std::find_if(_functions.begin(), _functions.end(), [](const Entry &entry) { return entry.deleteCallback == false; }) == _functions.end());
    }

    inline __attribute__((__always_inline__))
    static FunctionsVector &getVector() {
        return _functions;
    }

    inline __attribute__((__always_inline__))
    static bool callOnce(Callback callback) {
        return schedule_function(callback);
    }

private:
    static FunctionsVector _functions;
};

inline void LoopFunctions::add(Callback callback, CallbackPtr callbackPtr)
{
    auto iterator = std::find(_functions.begin(), _functions.end(), callbackPtr);
    if (iterator == _functions.end()) {
        _functions.emplace_back(callback, callbackPtr, false);
        return;
    }
    iterator->deleteCallback = false;
}

#if _MSC_VER || ESP32

extern std::vector<std::function<void(void)>> scheduled_functions;

inline void run_scheduled_functions()
{
    for(auto fn: scheduled_functions) {
        fn();
        #if ESP32 && defined(CONFIG_HEAP_POISONING_COMPREHENSIVE)
            heap_caps_check_integrity_all(true);
        #elif _MSC_VER
            _ASSERTE(_CrtCheckMemory());
        #endif
    }
    scheduled_functions.clear();
}

#endif

#include <debug_helper_disable.h>

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif
