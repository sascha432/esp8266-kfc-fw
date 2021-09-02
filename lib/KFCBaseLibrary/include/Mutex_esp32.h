/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Arduino_compat.h"
#include "freertos/semphr.h"
#include "debug_helper.h"

struct MutexSemaphore {
    MutexSemaphore();
    ~MutexSemaphore();
    void lock();
    void unlock();
    xSemaphoreHandle _lock;
};

inline MutexSemaphore::MutexSemaphore() :
    _lock(xSemaphoreCreateMutex())
{
    if (!_lock) {
        //  __DBG_panic("xSemaphoreCreateMutex failed");
    }
}

inline MutexSemaphore::~MutexSemaphore()
{
    vSemaphoreDelete(_lock);
}

inline void MutexSemaphore::lock()
{
    do {
    } while (xSemaphoreTake(_lock, portMAX_DELAY) != pdPASS);
}

inline void MutexSemaphore::unlock()
{
    xSemaphoreGive(_lock);
}
