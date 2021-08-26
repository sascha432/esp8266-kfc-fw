/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifdef new
#pragma push_macro("new")
#undef new
#define pop_macro_new
#endif

class StaticString {
public:

    StaticString() noexcept :
        _ptr(nullptr)
        #if ESP32
            , _length(0), _progmem(false)
        #endif
    {
    }


    StaticString(StaticString &&str) noexcept :
        _ptr(std::exchange(str._ptr, nullptr))
        #if ESP32
            , _length(str._length), _progmem(str._progmem)
        #endif
    {
    }

    StaticString(const String &str) noexcept :
        StaticString(str.c_str(), static_cast<uint16_t>(str.length()))
    {
    }

    StaticString(const char *str) noexcept :
        StaticString(str, static_cast<uint16_t>(strlen(str)))
    {
    }

    ~StaticString() {
        __free();
    }

    String toString() const {
        return _ptr ? (isProgMem() ? String(FPSTR(_ptr)) : String(reinterpret_cast<const char *>(_ptr))) : String();
    }

    const char *c_str() const {
        return reinterpret_cast<const char *>(_ptr);
    }

    PGM_P p_str() const {
        return reinterpret_cast<PGM_P>(_ptr);
    }

    operator bool() const {
        return _ptr != nullptr;
    }

    bool equalsIgnoreCase(const String &str) const {
        if (_ptr) {
            if (isProgMem()) {
                return !strcasecmp_P(str.c_str(), p_str());
            }
            else {
                return str.equalsIgnoreCase(c_str());
            }
        }
        return false;
    }

    bool equalsIgnoreCase(const StaticString &str) const {
        if (_ptr) {
            if (isProgMem() && str.isProgMem()) {
                return !strcasecmp_P_P(p_str(), str.p_str());
            }
            else if (isProgMem()) {
                return !strcasecmp_P(str.c_str(), p_str());
            }
            else if (str.isProgMem()) {
                return !strcasecmp_P(c_str(), str.p_str());
            }
        }
        return false;
    }

    void __free() {
        if (!isProgMem() && _ptr) {
            free(_ptr);
        }
        _ptr = nullptr;
        #if ESP32
            _length = 0;
            _progmem = false;
        #endif
    }

#if ESP8266

    StaticString(PGM_P str, uint16_t length) : _ptr(length ? malloc(length + 1) : nullptr) {
        if (_ptr) {
            memmove_P(_ptr, str, length + 1);
        }
    }

    StaticString(const __FlashStringHelper *str) : _ptr(const_cast<__FlashStringHelper *>(str)) {
    }

    StaticString &operator=(StaticString &&str) {
        this->~StaticString();
        _ptr = std::exchange(str._ptr, nullptr);
        return *this;
    }

    bool isProgMem() const {
        return is_PGM_P(_ptr);
    }

private:
    void *_ptr;

#else

    StaticString &operator=(StaticString &&str) noexcept {
        this->~StaticString();
        _ptr = std::exchange(str._ptr, nullptr);
        _length = str._length;
        _progmem = str._progmem;
        str._length = 0;
        str._progmem = 0;
        return *this;
    }

    StaticString(const char *str, uint16_t length) : _ptr(length ? malloc(length + 1) : nullptr), _length(length), _progmem(false) {
        if (_ptr) {
            memcpy(_ptr, str, length + 1);
        }
    }

    StaticString(const __FlashStringHelper *str) : _ptr(const_cast<__FlashStringHelper *>(str)), _length((uint16_t)strlen_P(reinterpret_cast<PGM_P>(str))), _progmem(true) {
    }

    bool isProgMem() const {
        return _progmem;
    }

private:
    void *_ptr;
    uint16_t _length;
    bool _progmem;
#endif
};

#ifdef pop_macro_new
#pragma pop_macro("new")
#undef pop_macro_new
#endif
