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
}

void KFCMemoryDebugging::dump(Print &output, AllocType type)
{
    _instance.__dump(output, 0, type);
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
            "alloc: %u\nfree: %u\n---\nrealloc: %u\nnew: %u\ndelete: %u\nfailed: %u\n:nullptr freed: %u\nalloc/time ratio: %.3f\nfree/time ratio: %.3f\nratio/ratio: %.3f\n"
        ),
        _allocCount, _freeCount, _reallocCount, _newCount, _deleteCount, _failCount, _freeNullCount, ra, rf, ra / rf
    );
}

void KFCMemoryDebugging::__markAllNoLeak()
{
    AllocPointerList::markAllNoLeak(true);
}

KFCMemoryDebugging &KFCMemoryDebugging::getInstance()
{
    return _instance;
}

#endif

// String DebugContext::__file;
// String DebugContext::__function;
uint8_t DebugContext::__state = DEBUG_HELPER_STATE_DISABLED; // needs to be disabled until the output stream has been initialized
// DebugContextFilterVector DebugContext::__filters;

// FixedCircularBuffer<DebugContext::Positon_t,100> DebugContext::__pos;

#if DEBUG_INCLUDE_SOURCE_INFO
const char ___debugPrefix[] PROGMEM = "D%08lu (%s:%u <%d:%u> %s): ";
#else
const char ___debugPrefix[] PROGMEM = "DBG: ";
#endif


// void DebugContext::regPrint(FixedCircularBuffer<Positon_t,100>::iterator it) {
//     while(it != __pos.end()) {
//         auto &position = (*it);
//         DEBUG_OUTPUT.printf_P(PSTR("%s:%u - %u\n"), position.file, position.line, position.time);
//         ++it;
//     }
// }

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
        static String buf;
        buf = std::move(PrintString(reinterpret_cast<const __FlashBufferHelper *>(start), ptr - start));
        return buf.c_str();
    }
    return start;
}


#endif
