/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <global.h>

#if _MSC_VER

#ifndef HAVE_LPWSTR_WSTRING
#define HAVE_LPWSTR_WSTRING 1

class LPWStr {
public:
    LPWStr();
    LPWStr(const String &str);
    ~LPWStr();

    LPWSTR lpw_str(const String &str);
    LPWSTR lpw_str();

private:
    LPWSTR _str;
};

#endif

extern "C"  {
    extern void optimistic_yield(uint32_t);
    extern bool can_yield();
    extern void yield();
}

struct portMuxType {
    portMuxType() {}
    void enter() {
    }
    void exit() {
    }
    void enterISR() {
    }
    void exitISR() {
    }
};

// scope level auto enter/exit
struct portMuxLock {
    portMuxLock(portMuxType &mux) : _mux(mux) {
        _mux.enter();
    }
    ~portMuxLock() {
        _mux.exit();
    }
    portMuxType &_mux;
};

// scope level auto enter/exit
struct portMuxLockISR {
    portMuxLockISR(portMuxType &mux) : _mux(mux) {
        _mux.enterISR();
    }
    ~portMuxLockISR() {
        _mux.exitISR();
    }
    portMuxType &_mux;
};

#endif
