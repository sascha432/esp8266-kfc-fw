/**
 * Author: sascha_lammers@gmx.de
 */

#if DEBUG

#include <Arduino_compat.h>
#include <PrintString.h>
#include <StreamWrapper.h>
#include "debug_helper.h"

// int DEBUG_OUTPUT_flush() {
//     DEBUG_OUTPUT.flush();
//     return 1;
// }

#if HAVE_MEM_DEBUG

uint32_t KFCMemoryDebugging::_newCount = 0;
uint32_t KFCMemoryDebugging::_deleteCount = 0;
uint32_t KFCMemoryDebugging::_allocCount = 0;
uint32_t KFCMemoryDebugging::_reallocCount = 0;
uint32_t KFCMemoryDebugging::_freeCount = 0;
uint32_t KFCMemoryDebugging::_failCount = 0;
uint32_t KFCMemoryDebugging::_freeNullCount = 0;
uint32_t KFCMemoryDebugging::_heapSize = 0;
uint32_t KFCMemoryDebugging::_startTime = 0;
KFCMemoryDebugging KFCMemoryDebugging::_instance;


void KFCMemoryDebugging::_add(const DebugContext &p, const void *ptr, size_t size)
{
    _instance.__add(p, ptr, size);
}

void KFCMemoryDebugging::_remove(const DebugContext &p, const void *ptr)
{
    _instance.__remove(p, ptr);
}

void KFCMemoryDebugging::__add(const DebugContext &p, const void *ptr, size_t size)
{
    if (ptr == nullptr) {
        _failCount++;
        if (p.isActive()) {
            p.prefix();
            p.getOutput().printf_P(PSTR("allocation failed size=%ld\n"), size);
        }
    }
    else {
        auto iterator = find(ptr);
        if (isValid(iterator)) {
            if (p.isActive()) {
                auto &o = p.getOutput();
                p.prefix();
                o.printf_P(PSTR("alloc: block already allocated: "));
                iterator->dump(o);
            }
        }
        else {
            add(ptr, size, p._file, p._line);
            _allocCount++;
        }
    }
}

void KFCMemoryDebugging::__remove(const DebugContext &p, const void *ptr)
{
    if (ptr == nullptr) {
        _freeNullCount++;
        if (p.isActive()) {
            p.prefix();
            p.getOutput().println(F("free: nullptr"));
        }
    }
    else {
        if (remove(ptr)) {
            _freeCount++;
        }
        else if (p.isActive()) {
            p.prefix();
            p.getOutput().printf_P(PSTR("free: invalid pointer %08x\n"), ptr);
        }
    }

}

void KFCMemoryDebugging::reset()
{
    _newCount = 0;
    _deleteCount = 0;
    _allocCount = 0;
    _reallocCount = 0;
    _freeCount = 0;
    _failCount = 0;
    _freeNullCount = 0;
    _startTime = millis();
}

void KFCMemoryDebugging::dump(Print &output, AllocType type)
{
    _instance.__dump(output, 0, type);
}

void KFCMemoryDebugging::__dumpShort(Print &output)
{
    if (_allocCount) {
        size_t heapSize = _heapSize;

        auto &list = AllocPointerList::getList();
        for(const auto &info: list) {
            if (info == AllocType::ALL) {
                heapSize += KFCMemoryDebugging::getHeapUsage(info.getSize());
            }
        }

        // size_t msize = sizeof(list[0]) * list.capacity();
        // heapSize += KFCMemoryDebugging::getHeapUsage(msize);

        output.printf_P(PSTR(" alloc=%u free=%u null=%u heap=%u ops=%.2f"),
            _allocCount, _freeCount, _freeNullCount, _heapSize,  (_allocCount + _freeCount) / ((millis() - _startTime) / 1000.0)
        );
    }
}

void KFCMemoryDebugging::dumpShort(Print &output)
{
    _instance.__dumpShort(output);
}

void KFCMemoryDebugging::markAllNoLeak()
{
    _instance.__markAllNoLeak();
}

void KFCMemoryDebugging::__dump(Print &output, size_t dumpBinaryMaxSize, AllocPointerList::AllocType type)
{
    AllocPointerList::dump(output, dumpBinaryMaxSize, type);
    auto ra = _allocCount / (float)getSystemUptime();
    auto rf = _freeCount / (float)getSystemUptime();
    output.printf_P(PSTR(
            "alloc: %u\nfree: %u\n---\nrealloc: %u\nnew: %u\ndelete: %u\nfailed: %u\n:nullptr freed: %u\nalloc/time ratio: %.3f\nfree/time ratio: %.3f\nratio/ratio: %.3f\nheap tracking: %u\n"
        ),
        _allocCount, _freeCount, _reallocCount, _newCount, _deleteCount, _failCount, _freeNullCount, ra, rf, ra / rf, _heapSize
    );
}

void KFCMemoryDebugging::__markAllNoLeak()
{
    AllocPointerList::markAllNoLeak(true);
}

size_t KFCMemoryDebugging::getHeapUsage(size_t size, bool addOverhead)
{
    return ((size + 7) & ~7) + (addOverhead ? 4 : 0);
}

KFCMemoryDebugging &KFCMemoryDebugging::getInstance()
{
    return _instance;
}

#endif

uint8_t DebugContext::__state = DEBUG_HELPER_STATE_DISABLED; // needs to be disabled until the output stream has been initialized
DebugContext DebugContext::__pos;
DebugContext::Stack DebugContext::_stack;

bool DebugContext::__store_pos(DebugContext &&dctx)
{
    __pos = std::move(dctx);
    return true;
}

#if LOGGER
#include "Logger.h"
#endif

DebugContext::Guard::Guard(DebugContext &&ctx)
{
    ctx._stackGuard = this;
    _stack.emplace(std::move(ctx));
}

DebugContext::Guard::Guard(DebugContext &&ctx, const char *fmt, ...)
{
    ctx._stackGuard = this;
    va_list arg;
    va_start(arg, fmt);
    _args = PrintString(FPSTR(fmt), arg);
    va_end(arg);
    _stack.emplace(std::move(ctx));
}

DebugContext::Guard::~Guard()
{
    _stack.pop();
}

const String &DebugContext::Guard::getArgs() const
{
    return _args;
}

bool DebugContext::reportAssert(const DebugContext &ctx, const __FlashStringHelper *message)
{
    auto file = KFCFS.open(F("/.logs/assert.log"), fs::FileOpenMode::append);
    if (file) {
        PrintString str;
        str.strftime_P(SPGM(strftime_date_time_zone), time(nullptr));
        file.print(str);
        file.print(' ');
        str = PrintString();
        str.printf_P(PSTR("%s:%u %s: "), __S(ctx._file), ctx._line, __S(ctx._functionName));
        file.print(str);
        file.printf_P(PSTR(" assert(%s)\n"), FPSTR(message));
        file.close();

#if LOGGER
        Logger_error(F("%sassert(%s) failed"), str.c_str(), FPSTR(message));
#endif

    }
    return true;
}

#if DEBUG_INCLUDE_SOURCE_INFO
const char ___debugPrefix[] PROGMEM = "D%08lu (%s:%u <%d:%u> %s): ";
#else
const char ___debugPrefix[] PROGMEM = "DBG: ";
#endif

void DebugContext::prefix() const 
{
    getOutput().printf_P(___debugPrefix, millis(), _file, _line, ESP.getFreeHeap(), can_yield(), _functionName);
}

String DebugContext::getPrefix() const 
{
    PrintString str(F("%s:%u: "), __S(_file), _line);
    return str;
}

void DebugContext::printStack() const
{
    if (!_stack.empty()) {
        auto stack = _stack;
        auto &output = getOutput();
        output.println(F("Stack trace"));
        while (!stack.empty()) {
            auto &item = stack.top();
            output.printf_P(PSTR("#%u %s:%u"), stack.size(), __S(item._file), item._line);
            if (item._stackGuard) {
                output.print(F(" args="));
                output.print(item._stackGuard->getArgs());
            }
            output.println();
            stack.pop();
        }
    }
}

void DebugContext::vprintf(const char *format, va_list arg) const
{
    getOutput().print(PrintString(FPSTR(format), arg));
}

void DebugContext::pause(uint32_t timeout)
{
    while (Serial.available()) {
        Serial.read();
    }
    if (timeout != ~0U) {
        timeout += millis();
    }
    DEBUG_OUTPUT.println(F("\n\nProgram execution paused, press # to continue...\n"));
    while(millis() <= timeout) {
        if (Serial.available() && Serial.read() == '#') {
            break;
        }
        delay(300);
        ESP.wdtFeed();
    }
}

const char *DebugContext::pretty_function(const char* name)
{
#if _WIN32
    {
        auto ptr = strstr(name, "__thiscall ");
        if (ptr) {
            name = ptr + 11;
        }
    }
#endif
    PGM_P start = name;
    PGM_P ptr = strchr_P(name, ':');
    if (!ptr) {
        ptr = strchr_P(name, '(');
    }
    if (ptr) {
        start = ptr;
        while (start > name && pgm_read_byte(start - 1) != ' ') {
            start--;
        }
    }
    ptr = strchr_P(start, '(');
    if (ptr) {
        static char buf[32];
        size_t len = ptr - start;
        if (len >= sizeof(buf)) {
            len = sizeof(buf) - 1;
        }
        strncpy_P(buf, start, len)[len] = 0;
        return buf;
    }
    return start;
}


#endif
