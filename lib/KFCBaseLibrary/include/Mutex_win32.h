/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

struct SemaphoreMutex {
    void lock() {
    }
    void unlock() {
    }
};

struct SemaphoreMutexRecursive : SemaphoreMutex {
};
