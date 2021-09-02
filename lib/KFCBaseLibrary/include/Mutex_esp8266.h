/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Arduino_compat.h"

struct SemaphoreMutex {
    SemaphoreMutex() {}
    void lock() {
        ets_intr_lock();
    }
    void unlock() {
        ets_intr_unlock();
    }
};

struct SemaphoreMutexRecursive : SemaphoreMutex {
};
