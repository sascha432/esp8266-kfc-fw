/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

class DebugContext {
public:
#if DEBUG_INCLUDE_SOURCE_INFO
    DebugContext() : _file(nullptr), _line(0), _functionName(nullptr) {}
    DebugContext(const char* file, int line, const char* functionName) : _file(file), _line(line), _functionName(functionName) {}
    void prefix() const {
        DEBUG_OUTPUT.printf_P(___debugPrefix, millis(), _file, _line, ESP.getFreeHeap(), _functionName);
    }
    const char* _file;
    int _line;
    const char* _functionName;
#else
    void prefix() const {
        DEBUG_OUTPUT.print(FPSTR(___debugPrefix));
    }
#endif

    template<typename T>
    inline T printResult(T result) const {
        if (isActive) {
            prefix();
            getOutput().print(F("result="));
            getOutput().println(result);
        }
        return result;
    }

    template<typename T>
    inline T printsResult(T result, const String &resultStr) const {
        if (isActive) {
            prefix();
            getOutput().print(F("result="));
            getOutput().println(resultStr);
        }
        return result;
    }

    template<typename T>
    inline T printfResult(T result, const char *format, ...) const {
        if (isActive()) {
            prefix();
            va_list arg;
            va_start(arg, format);
            vprintf(format, arg);
            va_end(arg);
        }
        return result;
    }

    void vprintf(const char *format, va_list arg) const;

    static void pause(uint32_t timeout = ~0);

    static bool isActive() {
        return __state == DEBUG_HELPER_STATE_ACTIVE;
    }
    static Print &getOutput() {
        return DEBUG_OUTPUT;
    }
    static uint8_t __state;
    static DebugContext __pos;
    static bool __store_pos(DebugContext &&dctx);
    static const char* pretty_function(const char* name);
    static void activate(bool state = true) {
        __state = state ? DEBUG_HELPER_STATE_ACTIVE : DEBUG_HELPER_STATE_DISABLED;
    }

    static bool reportAssert(const DebugContext &ctx, const __FlashStringHelper *message);
};

