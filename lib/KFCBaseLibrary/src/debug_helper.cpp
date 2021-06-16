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
