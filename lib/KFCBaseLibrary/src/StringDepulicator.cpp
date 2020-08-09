/**
* Author: sascha_lammers@gmx.de
*/

#include "StringDepulicator.h"

StringDeduplicator::StringDeduplicator() : StringDeduplicator(512)
{
}

StringDeduplicator::StringDeduplicator(size_t bufferSize) : _buffer(bufferSize)
#if DEBUG_STRING_DEDUPLICATOR
    , _count(0)
#endif
{
    __DBG_printf("sd=%p buffer=%u", this, _buffer.size());
}

StringDeduplicator::~StringDeduplicator()
{
#if DEBUG_STRING_DEDUPLICATOR
    clear();
#else
    __DBG_printf("sd=%p buffer=%u list=%u", this, _buffer.length(), _strings.size());
    for(const auto ptr: _strings) {
        free(ptr);
    }
#endif
    _buffer.clear();
    __DBG_printf("sd=%p buffer size=%u", this, _buffer.size());
}

const char *StringDeduplicator::isAttached(const char *str, size_t len)
{
#if defined(ESP8266)
    if (is_PGM_P(str)) {
        return str;
    }
#endif
    auto ptr = (const char *)_buffer.begin();
    auto endPtr = (const char *)_buffer.end();
    if (!(str >= ptr && str < endPtr)) { // inside buffer?
        auto iterator = std::find(_strings.begin(), _strings.end(), (char *)str);
        if (iterator != _strings.end()) {
#if DEBUG_STRING_DEDUPLICATOR
                _count++;
#endif
            return *iterator;
        }
        for(const auto str2: _strings) {
#if defined(ESP8266)
            if (!strcmp(str2, str)) {
#else
            if (!strcmp_P(str2, str)) {
#endif
#if DEBUG_STRING_DEDUPLICATOR
                _count++;
#endif
                return str2;
            }
        }
    }
    while(ptr + len < endPtr) {
#if defined(ESP8266)
        if (!strcmp(ptr, str)) {
#else
        if (!strcmp_P(ptr, str)) {
#endif
#if DEBUG_STRING_DEDUPLICATOR
            _count++;
#endif
            return ptr;
        }
        ptr += strlen(ptr) + 1;
    }
    return nullptr;
}

// attach a temporary string to the output object
const char *StringDeduplicator::attachString(const char *str)
{
    size_t len = strlen_P(str);
    const char *hasStr = isAttached(str, len);
    if (hasStr) {
        return hasStr;
    }
    if (len >= 15 || !_buffer.size() || _buffer.length() + len + 1 >= _buffer.size()) {
        char *ptr = (char *)malloc(len + 1);
        memcpy_P(ptr, str, len + 1);
        ptr[len] = 0;
        _strings.push_back(ptr);
        return ptr;
    }
    auto result = _buffer.end();
    _buffer.write_P(str, len + 1);
    return reinterpret_cast<const char *>(result);
}

void StringDeduplicator::clear()
{
#if DEBUG_STRING_DEDUPLICATOR
    __DBG_printf("dupes=%u count=%u size=%u list=%u", _count, count(), size(), _strings.size());
    for(auto *str: _strings) {
        DEBUG_OUTPUT.printf_P(PSTR("%p[%d] %-80.80s\n"), str, strlen_P(str), str);
    }
    auto ptr = (const char *)_buffer.begin();
    auto endPtr = (const char *)_buffer.end();
    while(ptr < endPtr) {
        size_t len = strlen(ptr);
        DEBUG_OUTPUT.printf_P(PSTR("b[%d] %s\n"), len, ptr);
        ptr += len + 1;
    }
    _count = 0;
#else
    __DBG_printf("sd=%p buffer=%u list=%u", this, _buffer.length(), _strings.size());
#endif
    for(const auto ptr: _strings) {
        free(ptr);
    }
    _strings.clear();
    _buffer.setLength(0);
}

size_t StringDeduplicator::size() const
{
    size_t size = 0;
    for(const auto ptr: _strings) {
        size += strlen(ptr);
    }
    return size + _buffer.length();
}

size_t StringDeduplicator::count() const
{
    size_t count = _strings.size();
    auto ptr = (const char *)_buffer.begin();
    auto endPtr = (const char *)_buffer.end();
    while(ptr < endPtr) {
        if (!*ptr++) {
            count++;
        }
    }
    return count;
}
