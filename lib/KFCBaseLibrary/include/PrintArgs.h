/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#include <BufferStream.h>
#include <PrintString.h>

#ifndef DEBUG_PRINT_ARGS
#define DEBUG_PRINT_ARGS                    0
#endif

#if DEBUG_PRINT_ARGS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

class PrintArgs
{
public:
    typedef PrintArgs PrintInterface;

    PrintArgs(const PrintArgs &print) = delete;

    PrintArgs() : _bufferPtr(), _position(), _strLength() {
#if DEBUG_PRINT_ARGS
        _outputSize = 0;
#endif
    }
    ~PrintArgs() {
    }

    void clear();
    size_t fillBuffer(uint8_t *data, size_t size);

    template <typename T, typename... Args>
    void printf_P(const T &format, const Args &... args) {
        _buffer.write(sizeof...(args));
        //_debug_printf_P(PSTR("this=%p buffer=%d\n"), this, _buffer.size());
        _store((size_t)format);
        _collect(args...);
    }

private:
    static const uint16_t NoLength = ~0;

private:
    int _printf(uint8_t *buffer, size_t size, void **args);
    void _store(size_t data);
    void _collect();

    template <typename T> void _collect(const T &t) {
        _store((size_t)t);
    }

    template <typename T, typename... Args>
    void _collect(const T &t, const Args &... args) {
        _store((size_t)t);
        _collect(args...);
    }

// public:
//     void dump(Print &output);

private:
    Buffer _buffer;
    uint8_t *_bufferPtr;
    int _position;
    int _strLength;
#if DEBUG_PRINT_ARGS
    int _outputSize;
#endif
};

#include "debug_helper_disable.h"
