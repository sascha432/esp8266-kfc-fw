/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "StringDepulicator.h"
#include <numeric>

inline size_t StringBuffer::count() const
{
    size_t count = 0;
    auto begin = Buffer::begin();
    auto end = Buffer::end();
    while (begin < end) {
        if (!*begin++) {
            count++;
        }
    }
    return count;
}

inline size_t StringBuffer::space() const
{
    return size() - length();
}

inline const char *StringBuffer::addString(const char *str, size_t len)
{
    if (len + 1 > size()) {
        return nullptr;
    }
    auto ptr = cstr_end();
    write_P(str, len);
    write(0);
    return ptr;
}

inline void StringBufferPool::clear()
{
#if DEBUG_STRING_DEDUPLICATOR && 0
    if (_pool.size()) {
        StringVector list;
        for (const auto &buffer : _pool) {
            list.push_back(PrintString(F("%u:%u:%u"), buffer.length(), buffer.space(), buffer.count()));
            buffer.dump(DEBUG_OUTPUT);
        }
        __DBG_printf("pool=%p [%s]", this, implode(F(", "), list).c_str());
    }
#endif
    _pool.clear();
}

inline size_t StringBufferPool::count() const
{
    return std::accumulate(_pool.begin(), _pool.end(), (size_t)0, [](size_t sum, const StringBuffer &buffer) -> size_t {
        return sum + buffer.count();
    });
}

inline size_t StringBufferPool::space() const
{
    return size() - length();
}

inline size_t StringBufferPool::length() const
{
    return std::accumulate(_pool.begin(), _pool.end(), (size_t)0, [](size_t sum, const StringBuffer &buffer) -> size_t {
        return sum + buffer.length();
    });
}

inline size_t StringBufferPool::size() const
{
    return std::accumulate(_pool.begin(), _pool.end(), (size_t)0, [](size_t sum, const StringBuffer &buffer) -> size_t {
        return sum + buffer.size();
    });
}


#if DEBUG_STRING_DEDUPLICATOR

inline const char *StringDeduplicator::isAttached(const char *str, size_t *len)
{
    if (is_PGM_P(str)) {
        _fpStrCount++;
        auto iterator = std::find(_fpStrings.begin(), _fpStrings.end(), str);
        if (iterator == _fpStrings.end()) {
            _fpStrings.push_back(str);
        }
        return str;
    }

    auto ptr = _strings.findStr(str, (len == nullptr) ? safe_strlen(str) : (*len == ~0U ? (*len = safe_strlen(str)) : *len));
    if (ptr) {
        _dupesCount++;
    }
    return ptr;
}

#else

inline const char *StringDeduplicator::isAttached(const char *str, size_t *len)
{
    if (is_PGM_P(str)) {
        return str;
    }
    return _strings.findStr(str, (len == nullptr) ? safe_strlen(str) : (*len == ~0U ? (*len = safe_strlen(str)) : *len));
}

#endif

inline const char *StringDeduplicator::isAttached(const __FlashStringHelper *str, size_t *len)
{
    return isAttached(reinterpret_cast<const char *>(str), len);
}

inline const char *StringDeduplicator::isAttached(const String &str)
{
    size_t len = str.length();
    return isAttached(str.c_str(), &len);
}

inline const char *StringDeduplicator::attachString(const char *str)
{
    size_t len = ~0U;
    const char *ptr = isAttached(str, &len);
    if (ptr) {
        return ptr;
    }

#if DEBUG_STRING_DEDUPLICATOR
    for (const auto str2 : _fpStrings) {
        if (strcmp_P_P(str, str2) == 0) {
            _fpDupesCount++;
        }
    }
#endif

    return _strings.addString(str, len);
}

inline const char *StringDeduplicator::attachString(const __FlashStringHelper *str)
{
    __DBG_validatePointer(str, VP_HPS);
    return attachString(reinterpret_cast<const char *>(str));
}

inline const char *StringDeduplicator::attachString(const String &str)
{
    __DBG_validatePointer(str, VP_HPS);
    return attachString(str.c_str());
}

#if DEBUG_STRING_DEDUPLICATOR == 0
inline void StringDeduplicator::clear()
{
    _strings.clear();
}
#endif
