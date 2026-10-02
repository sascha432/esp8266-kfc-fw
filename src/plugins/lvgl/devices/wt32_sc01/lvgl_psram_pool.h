/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// Pool of LVGL's own allocator (LV_MEM_POOL_ALLOC in lv_conf.h).
//
// The WT32-SC01 has 8 MB PSRAM but only ~180 KB of internal DRAM, shared with the WiFi/lwIP
// stack, the AsyncWebServer and the panel driver. The 64 KB pool of LVGL does not have to be
// in the fast RAM (the widget trees are small and the render buffers are allocated by the
// application), so it is taken from PSRAM and LVGL keeps its own TLSF allocator.
//
// Falls back to internal RAM if there is no PSRAM, LVGL cannot run without a pool.
//

#include <stddef.h>

#include <esp_heap_caps.h>

static inline void *lvgl_psram_pool_alloc(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == NULL) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return ptr;
}
