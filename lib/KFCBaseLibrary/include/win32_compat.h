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
    bool enter() {
        return true;
    }
    bool exit() {
        return false;
    }
    bool enterISR() {
        return true;
    }
    bool exitISR() {
        return false;
    }
};

#endif
