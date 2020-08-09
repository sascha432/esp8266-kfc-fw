/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Buffer.h"

#ifndef DEBUG_STRING_DEDUPLICATOR
#define DEBUG_STRING_DEDUPLICATOR               0
#endif

class StringDeduplicator {
public:
    StringDeduplicator();
    StringDeduplicator(size_t bufferSize);
    ~StringDeduplicator();

    const char *isAttached(const char *, size_t len);
    const char *isAttached(const __FlashStringHelper *str, size_t len) {
        return isAttached(reinterpret_cast<const char *>(str), len);
    }
    const char *isAttached(const String &str){
        return isAttached(str.c_str(), str.length());
    }
    // returns nullptr if the string is not attach
    // otherwise it returns the string or a string with the same content
    const char *getAttachedString(const char *str);
    // attach a temporary string to the output object
    const char *attachString(const char *str);
    const char *attachString(const __FlashStringHelper *str) {
        return attachString(reinterpret_cast<const char *>(str));
    }
    const char *attachString(const String &str) {
        return attachString(str.c_str());
    }

    void clear();
private:
    size_t size() const;
    size_t count() const;

private:
    Buffer _buffer;
    std::vector<char *> _strings;
#if DEBUG_STRING_DEDUPLICATOR
    size_t _count;
#endif
};
