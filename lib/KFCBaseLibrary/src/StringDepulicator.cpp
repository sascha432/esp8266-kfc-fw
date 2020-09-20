/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#include <misc.h>
#include "StringDepulicator.h"

#if DEBUG_STRING_DEDUPLICATOR
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

static constexpr size_t kFirstPoolSize = 56;
static constexpr size_t knthPoolSize = 32;

const char *StringBuffer::findStr(const char *str, size_t len) const
{
    auto begin = cstr_begin();
    auto end = cstr_end();
    auto findLen = safe_strlen(str);
    while (begin + len < end) {
        size_t thisLen = strlen(begin);
        // we can skip comparing if the string to find is longer
        if (findLen <= thisLen) {
            // if it is equal or shorter, we can compare the end if there is a partial match
            auto partial = begin + (thisLen - findLen);
            if (!safe_strcmp(partial, str)) {
                return partial;
            }
        }
        begin += thisLen + 1;
    }
#if 0
    // takes hundreds of milliseconds
    auto pbegin = (const char *)0x4029638d;
    auto pend = (const char *)SECTION_IROM0_TEXT_END_ADDRESS;
    auto start = micros();
    while (pbegin < pend) {
        if (!strcmp_P_P(pbegin, str)) {
            auto dur = micros() - start;
            __DBG_printf("%u: found %p for %p(%s)", dur, pbegin, str, str);
            return pbegin;
        }
        pbegin++;
    }
    auto dur = micros() - start;
    __DBG_printf("%u: not found %s", dur, str);
#endif
    return nullptr;
}


#if DEBUG_STRING_DEDUPLICATOR

void StringBuffer::dump(Print &output) const
{
#if 1
    auto begin = cstr_begin();
    auto end = cstr_end();
    while(begin < end) {
        size_t len = strlen(begin);
        size_t n = len;
        if (n > 40) {
            n = 40;
        }
        output.printf_P(PSTR("%08x(%u): %-0.*s\n"), begin, len, n, begin);
        begin += len + 1;
    }
#endif
}

void StringBufferPool::dump(Print &output) const
{
    size_t n = 0;
    for(auto &pool: _pool) {
        output.printf_P(PSTR("POOL #%u capacity=%u space=%u\n"), n, pool.capacity(), pool.space());
        pool.dump(output);
        n++;
    }
}

#endif

const char *StringBufferPool::findStr(const char *str, size_t len) const
{
    if (len == 0) {
        return emptyString.c_str();
    }
    for(const auto &buffer: _pool) {
        if (str >= buffer.cstr_begin() && str < buffer.cstr_end()) { // str belongs to this buffer
            return buffer.findStr(str, len);
        }
    }
    for(const auto &buffer: _pool) { // search all buffers
        const char *ptr = buffer.findStr(str, len);
        if (ptr) {
            return ptr;
        }
    }
    return nullptr;
}

const char *StringBufferPool::addString(const char *str, size_t len)
{
    if (len == 0) {
        return emptyString.c_str();
    }

    if (_pool.empty()) {
        _pool.emplace_back(kFirstPoolSize);
    }

    StringBuffer *target = nullptr;
    for(auto &buffer: _pool) {
        // find best match
        size_t spaceNeeded = buffer.length() + len + 1;
        if (spaceNeeded == buffer.size()) {
            target = &buffer;
            break;
        }
        else if (!target && spaceNeeded < buffer.size()) {
            target = &buffer;
        }
    }
    if (!target) {
        // add new pool
        size_t newSize = knthPoolSize;
        if (len > newSize) { // this string required extra space
            newSize = len + 1 + knthPoolSize;
        }
        newSize = (newSize + 7) & ~7; // align to memory block size
        __LDBG_printf("pools=%u new=%u", _pool.size(), newSize);
        _pool.emplace_back(newSize);
        target = &_pool.back();
    }
#if DEBUG_STRING_DEDUPLICATOR && 0
    auto ptr = target->addString(str, len);
    DEBUG_OUTPUT.printf_P(PSTR("add %p[%d] %-20.20s... %-20.20s...\n"), str, len, str, ptr);
    return ptr;
#else
    return target->addString(str, len);
#endif
}

#if DEBUG_STRING_DEDUPLICATOR

void StringDeduplicator::dump(Print &output) const
{
    _strings.dump(output);
}

void StringDeduplicator::clear()
{
    dump(DEBUG_OUTPUT);

    size_t heap = 0;
    if (_strings.size()) {
        heap = ESP.getFreeHeap();
        __DBG_printf("clear=%p len=%u size=%u count=%u space=%u overhead=%u", this, _strings.length(), _strings.size(), _strings.count(), _strings.space(), sizeof(StringBufferPool) + _strings._pool.capacity() + _strings._pool.size() * 4);
        __DBG_printf("dupes=%u fpdupes=%u fpcnt=%u fpsize=%u", _dupesCount, _fpDupesCount, _fpStrCount, _fpStrings.size());
    }
    _fpStrings.clear();
    _fpDupesCount = 0;
    _dupesCount = 0;
    _fpStrCount = 0;
    _strings.clear();
    if (heap) {
        __DBG_printf("this=%p free'd=%u", this, ESP.getFreeHeap() - heap);
    }
}
#endif
