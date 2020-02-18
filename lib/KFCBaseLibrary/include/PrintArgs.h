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

// use PrintArgsPrintWrapper(Print &output) to write to any Print Object

class PrintArgs
{
public:
    typedef PrintArgs PrintInterface;

    PrintArgs(const PrintArgs &print) = delete;

    PrintArgs() : _bufferPtr(nullptr), _position(0), _strLength(0) {
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
        if (_position == PrintObject) {
            reinterpret_cast<Print *>(_bufferPtr)->printf_P(format, std::forward<const Args &>(args)...);
        }
        else {
            _buffer.write(sizeof...(args));
            //_debug_printf_P(PSTR("this=%p buffer=%d\n"), this, _buffer.size());
            _store((size_t)format);
            _collect(args...);
        }
    }

private:
    int _printf(uint8_t *buffer, size_t size, void **args);
    void _store(size_t data);

    void _collect() {}

    template <typename T>
    void _collect(const T &t) {
        _store((size_t)t);
    }

    template <typename T, typename... Args>
    void _collect(const T &t, const Args &... args) {
        _store((size_t)t);
        _collect(args...);
    }

// public:
//     void dump(Print &output);

protected:
    static const int PrintObject = -1;

    uint8_t *_bufferPtr;
    int _position;
private:
    Buffer _buffer;
    int _strLength;
#if DEBUG_PRINT_ARGS
    int _outputSize;
#endif
};

class PrintArgsPrintWrapper : public PrintArgs
{
public:
    PrintArgsPrintWrapper(Print &output) {
        _bufferPtr = reinterpret_cast<uint8_t *>(&output);
        _position = PrintArgs::PrintObject;
    }
    PrintArgsPrintWrapper(const PrintArgsPrintWrapper &wrapper) {
        _bufferPtr = wrapper._bufferPtr;
        _position = wrapper._position;
    }
};

#include "debug_helper_disable.h"
