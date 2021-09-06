/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Arduino_compat.h"

#if ESP32
#    include "Mutex_esp32.h"
#elif ESP8266
#    include "Mutex_esp8266.h"
#elif _MSC_VER
#    include "Mutex_win32.h"
#endif

struct MutexLock
{
    MutexLock(SemaphoreMutex &lock, bool performInitialLock = true) : _lock(lock), _locked(performInitialLock) {
        if (performInitialLock) {
            _lock.lock();
        }
    }
    ~MutexLock() {
        if (_locked) {
            _lock.unlock();
        }
    }
    bool lock() {
        if (!_locked) {
            _lock.lock();
            _locked = true;
        }
        return _locked;
    }
    bool unlock() {
        if (_locked) {
            _locked = false;
            _lock.unlock();
        }
        return _locked;
    }
    SemaphoreMutex &_lock;
    bool _locked;
};

struct MutexLockRecursive
{
    MutexLockRecursive(SemaphoreMutexRecursive &lock, bool performInitialLock = true) : _lock(lock), _locked(performInitialLock ? 1 : 0) {
        if (performInitialLock) {
            _lock.lock();
        }
    }
    ~MutexLockRecursive() {
        unlockAll();
    }
    bool lock() {
        _lock.lock();
        _locked++;
        return _locked;
    }
    bool unlock() {
        if (_locked) {
            _locked--;
            _lock.unlock();
        }
        return _locked;
    }
    void unlockAll() {
        while(_locked--) {
            _lock.unlock();
        }
    }
    SemaphoreMutexRecursive &_lock;
    uint32_t _locked;
};

#define MUTEX_LOCK_BLOCK(lock) \
    for(auto __lock = MutexLock(lock); __lock._locked; __lock.unlock())


#define MUTEX_LOCK_RECURSIVE_BLOCK(lock) \
    for(auto __lock = MutexLockRecursive(lock); __lock._locked; __lock.unlockAll())
