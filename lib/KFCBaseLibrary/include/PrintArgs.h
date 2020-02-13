/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#include <BufferStream.h>
#include <PrintString.h>

#ifndef ENABLE_PRINT_ARGS
#define ENABLE_PRINT_ARGS                   1
#endif

#ifndef DEBUG_PRINT_ARGS
#define DEBUG_PRINT_ARGS                    1
#endif

#if DEBUG_PRINT_ARGS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

class PrintArgs
{
public:
#if ENABLE_PRINT_ARGS

    typedef PrintArgs PrintInterface;

    static PrintArgs &getInstance() {
        static PrintArgs *instance = nullptr;
        if (!instance) {
            instance = new PrintArgs();
        }
        return *instance;
}

    static PrintInterface &toPrint(Print &print) {
        getInstance()._print = &print;
        return getInstance();
    }

#else

    typedef Print PrintInterface;

    static PrintInterface &toPrint(Print &print) {
        return print;
    }

#endif

    PrintArgs() : _bufferPtr(), _position(), _strLength(), _print() {
    }
    PrintArgs(const PrintArgs & print) = delete;

    void clear();
    size_t fillBuffer(uint8_t *data, size_t size);

    template <typename T, typename... Args>
    void printf_P(const T &format, const Args &... args) {
        _buffer.write(sizeof...(args));
        _store((size_t)format);
        _collect(args...);
        if (_print) {
            _print->printf_P((PGM_P)format, args...);
        }
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

public:
    void dump(Print &output);

private:
    Buffer _buffer;
    uint8_t *_bufferPtr;
    size_t _position;
    size_t _strLength;
    Print *_print;
};

#include "debug_helper_disable.h"
