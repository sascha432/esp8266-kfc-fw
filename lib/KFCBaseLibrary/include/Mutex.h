/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if ESP32
#    include "Mutex_esp32.h"
#elif ESP8266
#    include "Mutex_esp8266.h"
#elif _MSC_VER
#    include "Mutex_win32.h"
#endif

struct MutexLock {
    MutexLock(MutexSemaphore &lock) : _lock(lock), _locked(true) {
        _lock.lock();
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
            _lock.unlock();
            _locked = false;
        }
        return _locked;
    }
    MutexSemaphore &_lock;
    bool _locked;
};

#define MUTEX_LOCK_BLOCK(lock) \
    for(auto __lock = MutexLock(lock); __lock._locked; __lock.unlock())

