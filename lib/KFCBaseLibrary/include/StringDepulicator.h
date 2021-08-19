/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Buffer.h"

#ifndef DEBUG_STRING_DEDUPLICATOR
#    define DEBUG_STRING_DEDUPLICATOR 1
#endif

#if !DEBUG_STRING_DEDUPLICATOR
#    pragma push_macro("__DBG_validatePointer")
#    undef __DBG_validatePointer
#    define __DBG_validatePointer(ptr, ...) ptr
#endif

#if defined(ESP8266)
// PROGMEM strings are detected by address and skipped
#    define safe_strcmp(a, b) strcmp(a, b)
#    define safe_strlen(a)    strlen(a)
#else
#    define safe_strcmp(a, b) strcmp_P(a, b)
#    define safe_strlen(a)    strlen_P(a)
#endif

class StringDeduplicator;

// object to store and manage strings with deduplication and PROGMEM detection

class StringBuffer : public Buffer {
public:
    StringBuffer() : Buffer() {}
    StringBuffer(size_t size) : Buffer(size) {}

    // return number of stored strings
    size_t count() const;

    // free space
    size_t space() const;

    // return pointer to an attached string
    const char *findStr(const char *str, size_t len) const;

    // add a string
    // returns nullptr if there is no space left
    const char *addString(const char *str, size_t len);

    #if DEBUG_STRING_DEDUPLICATOR
        void dump(Print &output) const;
    #endif
};

class StringBufferPool {
public:
    StringBufferPool() : _pool() {}

    void clear();

    size_t count() const;
    size_t space() const;
    size_t length() const;
    size_t size() const;

    const char *findStr(const char *str, size_t len) const;
    const char *addString(const char *str, size_t len);

    #if DEBUG_STRING_DEDUPLICATOR
        void dump(Print &output) const;
    #endif

private:
    friend StringDeduplicator;

    using StringBufferVector = std::vector<StringBuffer>;
    StringBufferVector _pool;
};

class StringDeduplicator {
public:
    #if DEBUG_STRING_DEDUPLICATOR
        StringDeduplicator() : _dupesCount(0), _fpDupesCount(0), _fpStrCount(0) {}
    #else
        StringDeduplicator() {}
    #endif

    ~StringDeduplicator() {
        clear();
    }

    // returns the pointer to the string if a suitable match is found
    // if strlen is available already, pass it as len
    const char *isAttached(const char *str, size_t *len = nullptr);
    const char *isAttached(const __FlashStringHelper *str, size_t *len = nullptr);
    const char *isAttached(const String &str);

    // attach a string to the object
    // the returned pointer can point to RAM or FLASH
    const char *attachString(const char *str);
    const char *attachString(const __FlashStringHelper *str);
    const char *attachString(const String &str);

    // clear buffer and release memory
    void clear();

    #if DEBUG_STRING_DEDUPLICATOR
        void dump(Print &output) const;
    #endif

private:
    StringBufferPool _strings;
    #if DEBUG_STRING_DEDUPLICATOR
        size_t _dupesCount;
        size_t _fpDupesCount;
        size_t _fpStrCount;
        std::vector<const char *> _fpStrings;
    #endif
};

#include "StringDepulicator.hpp"

#if !DEBUG_STRING_DEDUPLICATOR
#    pragma pop_macro("__DBG_validatePointer")
#endif
