/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <misc.h>
#include <stack>

class DebugContext {
public:

#if DEBUG_INCLUDE_SOURCE_INFO
    class Guard {
    public:
        Guard(DebugContext &&ctx);
        Guard(DebugContext &&ctx, const char *fmt, ...);
        ~Guard();

        const String &getArgs() const;

    private:
        String _args;
    };

    DebugContext() : _file(nullptr), _line(0), _functionName(nullptr), _stackGuard(nullptr) {}
    DebugContext(const char* file, int line, const char* functionName) : _file(file), _line(line), _functionName(functionName), _stackGuard(nullptr) {}

    void prefix() const;
    String getPrefix() const;

    void printStack() const;

#else
    DebugContext() {}
    DebugContext(const char *file, int line, const char *functionName) {}

    void prefix() const {
        getOutput().print(FPSTR(___debugPrefix));
    }
    String getPrefix() const {
        return String();
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

    inline static bool isActive() {
        return __state == DEBUG_HELPER_STATE_ACTIVE;
    }

    inline static bool isActive(const char *file, long line, const char *funcname) {
        return __state == DEBUG_HELPER_STATE_ACTIVE;
    }

    inline static Print &getOutput() {
        return DEBUG_OUTPUT;
    }

    inline static void activate(bool state = true) {
        __state = state ? DEBUG_HELPER_STATE_ACTIVE : DEBUG_HELPER_STATE_DISABLED;
    }

    inline static void setState(uint8_t state = DEBUG_HELPER_STATE_ACTIVE) {
        __state = state;
    }

    static bool reportAssert(const DebugContext &ctx, const __FlashStringHelper *message);

    static bool __store_pos(DebugContext &&dctx);

    static const char *pretty_function(const char *name);

public:
#if DEBUG_INCLUDE_SOURCE_INFO
    using Stack = std::stack<DebugContext>;

    const char *_file;
    int _line;
    const char *_functionName;
    Guard *_stackGuard;
    // String _functionName


    static Stack _stack;
#endif

    static uint8_t __state;
    static DebugContext __pos;
};
