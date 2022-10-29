/**
 * Author: sascha_lammers@gmx.de
 */

#if DEBUG

#include <Arduino_compat.h>
#include <PrintString.h>
#include <StreamWrapper.h>
#include "debug_helper.h"

#if ESP8266

String &__validatePointer(const String &str, ValidatePointerType type, const char *file, int line, const char *func)
{
    __validatePointer(str.c_str(), type, file, line, func);
    return const_cast<String &>(str);
}

void *__validatePointer(const void *ptr, ValidatePointerType type, const char *file, int line, const char *func)
{
    if (static_cast<int>(type) & static_cast<int>(ValidatePointerType::P_NULL) && (ptr == nullptr)) {
        return nullptr;
    }
    if ((static_cast<int>(type) & static_cast<int>(ValidatePointerType::P_ALIGNED)) && (((uint32_t)ptr & 0x03) != 0)) {
        __DBG_printf(_VT100(bold_red) "INVALID POINTER %p(%u) NOT ALIGNED=%u FILE=%s LINE=%u FUNC=%s" _VT100(reset), ptr, type, ((uint32_t)ptr & 0x03), file, line, func);
__DBG_panic();
        return const_cast<void *>(ptr);
    }
    if (static_cast<int>(type) & static_cast<int>(ValidatePointerType::P_STACK)) {
        uint32_t stackPtr = 0;
        if ((uint32_t)ptr > SECTION_HEAP_END_ADDRESS && (uint32_t)ptr > (uint32_t)&stackPtr && (uint32_t)ptr < SECTION_STACK_END_ADDRESS) {
            return const_cast<void *>(ptr);
        }
    }
    if (static_cast<int>(type) & static_cast<int>(ValidatePointerType::P_PROGMEM)) {
        if (___IsValidPROGMEMPointer(ptr)) {
            return const_cast<void *>(ptr);
        }
    }
    if (static_cast<int>(type) & static_cast<int>(ValidatePointerType::P_HEAP)) {
        if (___IsValidHeapPointer(ptr)) {
            return const_cast<void *>(ptr);
        }
    }
    __DBG_printf(_VT100(bold_red) "INVALID POINTER %p(%u) FILE=%s LINE=%u FUNC=%s" _VT100(reset), ptr, type, file, line, func);
    __dump_binary_to(DEBUG_OUTPUT, ptr, 16, 16);
    // __DBG_panic("trace");
    // check if the pointer is in DRAM (compiled in data in RAM, outside the HEAP)
    if (!___IsValidDRAMPointer(ptr)) {
        #if 1
            if (static_cast<int>(type) & static_cast<int>(ValidatePointerType::P_NOSTRING)) {
                __DBG_panic();
            }
            static auto buffer = "INVALID";
            return const_cast<char *>(buffer);
        #else
            __DBG_panic();
        #endif
    }
    return nullptr;
}

#else

String &__validatePointer(const String &str, ValidatePointerType type, const char *file, int line, const char *func)
{
    __validatePointer(str.c_str(), type, file, line, func);
    return const_cast<String &>(str);
}

void *__validatePointer(const void *ptr, ValidatePointerType type, const char *file, int line, const char *func)
{
    return const_cast<void *>(ptr);
}

#endif

// use with "build_flags = -finstrument-functions"
#if HAVE_INSTRUMENT_FUNCTIONS

#include <stl_ext/fixed_circular_buffer>

extern "C" {

    void __cyg_profile_func_enter (void *this_fn, void *call_site) {
    }

    void __cyg_profile_func_exit  (void *this_fn, void *call_site) {
    }

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
#    include "logger.h"
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
        #if ESP8266
            ESP.wdtFeed();
        #endif
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
