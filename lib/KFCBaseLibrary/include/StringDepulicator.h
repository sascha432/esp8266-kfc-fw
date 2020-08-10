/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Buffer.h"

#ifndef DEBUG_STRING_DEDUPLICATOR
#define DEBUG_STRING_DEDUPLICATOR               0
#endif

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
};

class StringBufferPool {
public:
    StringBufferPool();

    void clear();
    size_t count() const;
    size_t space() const;
    size_t length() const;
    size_t size() const;
    const char *findStr(const char *str, size_t len) const;
    const char *addString(const char *str, size_t len);

private:
    using StringBufferVector = std::vector<StringBuffer>;
    StringBufferVector _pool;
};

class StringDeduplicator {
public:
    StringDeduplicator();
    ~StringDeduplicator();

    // returns the pointer to the string if a suitable match is found
    const char *isAttached(const char *str, size_t len);
    const char *isAttached(const __FlashStringHelper *str, size_t len) {
        return isAttached(reinterpret_cast<const char *>(str), len);
    }
    const char *isAttached(const String &str){
        return isAttached(str.c_str(), str.length());
    }

    // attach a string to the object
    // the returned pointer can point to RAM or FLASH
    const char *attachString(const char *str);
    const char *attachString(const __FlashStringHelper *str) {
        return attachString(reinterpret_cast<const char *>(str));
    }
    const char *attachString(const String &str) {
        return attachString(str.c_str());
    }

    // clear buffer and release memory
    void clear();

private:
    StringBufferPool _strings;
    // std::vector<char *> _strings;
#if DEBUG_STRING_DEDUPLICATOR
    size_t _dupesCount;
    size_t _fpDupesCount;
    size_t _fpStrCount;
    std::vector<const char *> _fpStrings;
#endif
};
