/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Arduino_compat.h"
#include <freertos/semphr.h>

struct SemaphoreMutex
{
    SemaphoreMutex();
    ~SemaphoreMutex();

    void lock();
    void unlock();

    xSemaphoreHandle _lock;
};

inline SemaphoreMutex::SemaphoreMutex() :
    _lock(xSemaphoreCreateMutex())
{
    // __DBG_assert_panic(_lock != nullptr, "xSemaphoreCreateMutex failed");
}

inline SemaphoreMutex::~SemaphoreMutex()
{
    vSemaphoreDelete(_lock);
}

inline void SemaphoreMutex::lock()
{
    do {
    } while (xSemaphoreTake(_lock, portMAX_DELAY) != pdPASS);
}

inline void SemaphoreMutex::unlock()
{
    xSemaphoreGive(_lock);
}

struct SemaphoreMutexRecursive
{
    SemaphoreMutexRecursive();
    ~SemaphoreMutexRecursive();

    void lock();
    void unlock();

    void _create();
    xSemaphoreHandle _lock;
};

inline SemaphoreMutexRecursive::SemaphoreMutexRecursive() :
    _lock(xSemaphoreCreateRecursiveMutex())
{
    // __DBG_assert_panic(_lock != nullptr, "xSemaphoreCreateRecursiveMutex failed");
}

inline SemaphoreMutexRecursive::~SemaphoreMutexRecursive()
{
    vSemaphoreDelete(_lock);
}

inline void SemaphoreMutexRecursive::lock()
{
    do {
    } while (xSemaphoreTakeRecursive(_lock, portMAX_DELAY) != pdPASS);
}

inline void SemaphoreMutexRecursive::unlock()
{
    xSemaphoreGiveRecursive(_lock);
}



