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


#if defined(ESP8266)
#define safe_strcmp(a, b) strcmp(a, b)
#else
#define safe_strcmp(a, b) strcmp_P(a, b)
#endif


size_t StringBuffer::count() const
{
    size_t count = 0;
    auto begin = Buffer::begin();
    auto end = Buffer::end();
    while(begin < end) {
        if (!*begin++) {
            count++;
        }
    }
    return count;
}

size_t StringBuffer::space() const
{
    return size() - length();
}
PROGMEM_STRING_DECL(__pure_virtual);

const char *StringBuffer::findStr(const char *str, size_t len) const
{
    auto begin = cstr_begin();
    auto end = cstr_end();
    while(begin + len < end) {
        if  (!safe_strcmp(begin, str)) {
            return begin;
        }
#if 1
        begin += strlen(begin) + 1;
#else
        // partial string deduplication
        // finds 1-2% more duplicates but mostly short ones
        begin++;
#endif
    }
#if 0
    auto pbegin = (const char *)0x4029638d;
    auto pend = (const char *)SECTION_IROM0_TEXT_END_ADDRESS;
    auto start = micros();
    while(pbegin < pend) {
        if  (!strcmp_P_P(pbegin, str)) {
            auto dur=micros()-start;
            __DBG_printf("%u: found %p for %p(%s)", dur, pbegin, str, str);
            return pbegin;
        }
        pbegin++;
    }
    auto dur=micros()-start;
    __DBG_printf("%u: not found %s", dur, str);
#endif
    return nullptr;
}

const char *StringBuffer::addString(const char *str, size_t len)
{
    if (len + 1 > size()) {
        return nullptr;
    }
    auto ptr = cstr_end();
    write_P(str, len);
    write(0);
    return ptr;
}

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
        output.printf_P(PSTR("%08x(%u): %-*.*s\n"), begin, len, n, n, begin);
        begin += len + 1;
    }
#endif
}

StringBufferPool::StringBufferPool() : _pool()
{
}

void StringBufferPool::clear()
{
#if DEBUG_STRING_DEDUPLICATOR && 0
    if (_pool.size()) {
        StringVector list;
        for(const auto &buffer: _pool) {
            list.push_back(PrintString(F("%u:%u:%u"), buffer.length(), buffer.space(), buffer.count()));
            buffer.dump(DEBUG_OUTPUT);
        }
        __DBG_printf("pool=%p [%s]", this, implode(F(", "), list).c_str());
    }
#endif
    _pool.clear();
}

size_t StringBufferPool::count() const
{
    size_t count = 0;
    for(const auto &buffer: _pool) {
        count += buffer.count();
    }
    return count;
}

size_t StringBufferPool::space() const
{
    return size() - length();
}

size_t StringBufferPool::length() const
{
    size_t length = 0;
    for(const auto &buffer: _pool) {
        length += buffer.length();
    }
    return length;
}

size_t StringBufferPool::size() const
{
    size_t size = 0;
    for(const auto &buffer: _pool) {
        size += buffer.size();
    }
    return size;
}

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
        _pool.emplace_back(128);
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
        size_t newSize = _pool.size() == 1 ? 256 : 128;
        if (len + 64 > newSize) { // this string required extra space
            newSize = len + 64;
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


StringDeduplicator::StringDeduplicator()
#if DEBUG_STRING_DEDUPLICATOR
    : _dupesCount(0), _fpDupesCount(0), _fpStrCount(0)
#endif
{
}

StringDeduplicator::~StringDeduplicator()
{
    clear();
}

const char *StringDeduplicator::isAttached(const char *str, size_t len)
{
#if defined(ESP8266)
    if (is_PGM_P(str)) {
#if DEBUG_STRING_DEDUPLICATOR
        _fpStrCount++;
        auto iterator = std::find(_fpStrings.begin(), _fpStrings.end(), str);
        if (iterator == _fpStrings.end()) {
            _fpStrings.push_back(str);
        }
#endif
        return str;
    }
#endif

#if DEBUG_STRING_DEDUPLICATOR
    auto ptr = _strings.findStr(str, len);
    if (ptr) {
        _dupesCount++;
    }
    return ptr;
#else
    return _strings.findStr(str, len);
#endif
}

const char *StringDeduplicator::attachString(const char *str)
{
    size_t len = strlen_P(str);
    const char *ptr = isAttached(str, len);
    if (ptr) {
        return ptr;
    }

#if DEBUG_STRING_DEDUPLICATOR
    for(const auto str2: _fpStrings) {
        if (strcmp_P_P(str, str2) == 0) {
            _fpDupesCount++;
        }
    }
#endif

    return _strings.addString(str, len);
}

void StringDeduplicator::clear()
{
#if DEBUG_STRING_DEDUPLICATOR
    size_t heap = 0;
    if (_strings.size()) {
        heap = ESP.getFreeHeap();
        __DBG_printf("clear=%p len=%u size=%u count=%u space=%u", this, _strings.length(), _strings.size(), _strings.count(), _strings.space());
#if DEBUG_STRING_DEDUPLICATOR
        __DBG_printf("dupes=%u fpdupes=%u fpcnt=%u fpsize=%u", _dupesCount, _fpDupesCount, _fpStrCount, _fpStrings.size());
#endif
    }
#if DEBUG_STRING_DEDUPLICATOR
    _fpStrings.clear();
    _fpDupesCount = 0;
    _dupesCount = 0;
    _fpStrCount = 0;
#endif
    _strings.clear();
    if (heap) {
        __DBG_printf("this=%p free'd=%u", this, ESP.getFreeHeap() - heap);
    }

#else
    _strings.clear();
#endif
}


