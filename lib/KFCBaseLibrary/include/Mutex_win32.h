/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <assert.h>
#include <mutex>

struct SemaphoreMutex {
    SemaphoreMutex() : _locked(false) 
    {
    }

    ~SemaphoreMutex() {
        _ASSERT_EXPR(_locked == false, "destroyed locked SemaphoreMutex");
    }

    void lock() {
        _ASSERT_EXPR(_locked == false, "SemaphoreMutex already locked");
        _locked = true;
        _mutex.lock();
    }

    void unlock() {
        _ASSERT_EXPR(_locked == true, "SemaphoreMutex not locked");
        _mutex.unlock();
        _locked = false;
    }

    std::mutex _mutex;
    std::atomic_bool _locked;
};

struct SemaphoreMutexRecursive : SemaphoreMutex {
};
