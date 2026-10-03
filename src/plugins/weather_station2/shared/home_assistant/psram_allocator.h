/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// STL allocator that serves a container from the PSRAM, with the internal RAM as fallback.
//
// The internal DRAM is the scarce resource of this board (8 MB of PSRAM are idle while ~100 KB of
// DRAM have to carry the network stack, the LVGL buffers, the task stacks and everything else). The
// big, cold data of the Home Assistant dashboard - the parsed /hass.yaml and the reported state of
// every tile - lives in a std::vector, and this allocator moves that buffer into the PSRAM.
//
// On a platform without a PSRAM the allocation falls back to the normal heap, so the header stays
// portable (the plugin only builds for the ESP32, the ESP8266 envs do not add the folder).
//
// allocate() throws std::bad_alloc when the PSRAM and the internal RAM are both exhausted (exceptions
// are enabled in this project), the caller is expected to handle it.
//

#include <Arduino_compat.h>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <vector>

#if ESP32
#    include <esp32-hal-psram.h>
#endif

namespace WeatherStation2 {

template<typename T>
struct PsramAllocator {
    using value_type = T;

    PsramAllocator() = default;
    template<typename U>
    PsramAllocator(const PsramAllocator<U> &)
    {
    }

    template<typename U>
    struct rebind {
        using other = PsramAllocator<U>;
    };

    T *allocate(size_t count)
    {
        if (!count) {
            return nullptr;
        }
        const size_t size = count * sizeof(T);
        void *ptr = nullptr;
#if ESP32
        // The plugin instance is a static object and some of its members are constructed before the
        // Arduino core reaches initArduino() -> psramInit(): ps_malloc() returns nullptr until the
        // PSRAM is initialized (it checks the spiramDetected flag). psramInit() is idempotent, the
        // later call of the core does nothing, and it runs the same sequence a few milliseconds later
        psramInit();
        ptr = ps_malloc(size);
#endif
        if (!ptr) {
            // a failed PSRAM allocation must not break the dashboard - the internal RAM is the next
            // best thing (that is where this data lived before it was moved)
            ptr = malloc(size);
        }
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T *>(ptr);
    }

    void deallocate(T *ptr, size_t)
    {
        free(ptr);
    }

    template<typename U>
    bool operator==(const PsramAllocator<U> &) const
    {
        return true;
    }
    template<typename U>
    bool operator!=(const PsramAllocator<U> &) const
    {
        return false;
    }
};

// a std::vector whose buffer lives in the PSRAM
template<typename T>
using PsramVector = std::vector<T, PsramAllocator<T>>;

} // namespace WeatherStation2
