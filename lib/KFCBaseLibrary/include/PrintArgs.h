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
#define DEBUG_CHECK_DUPES                   1
#else
#include "debug_helper_disable.h"
#define DEBUG_CHECK_DUPES                   1
#endif

// adds streaming capabilities to Print objects
// supported method: printf_P
// all pointers passed to printf_P must be valid until PrintArgs gets destroyed

// use PrintArgsPrintWrapper(Print &output) to write to any Print object without streaming (pass through mode)

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
#if DEBUG_PRINT_ARGS
        __DBG_print("--attached---");
        for(const auto &str: _attached) {
            __DBG_print(str);
        }
        __DBG_print("--end---");
#endif
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
            _store((size_t)format);
            _collect(args...);
        }
    }

public:
    // returns nullptr if the string is not attach
    // otherwise it returns the string or a string with the same content
    const char *getAttachedString(const char *cStr) const {
        auto iterator = std::find(_attached.begin(), _attached.end(), cStr);
        if (iterator != _attached.end()) {
            return iterator->c_str();
        }
        return nullptr;
    }

    // attach a temporary string to the output object
    const char *attachString(String &&str) {
#if DEBUG_CHECK_DUPES
        auto iterator = std::find(_attached.begin(), _attached.end(), str);
        if (iterator != _attached.end()) {
            __DBG_printf("duplicate string='%s'", iterator->c_str());
        }
#endif
        _attached.emplace_back(std::move(str));
        return _attached.back().c_str();
    }
    const char *attachConstString(const String &str) {
        return attachConstString(str.c_str());
    }
    const char *attachConstString(const __FlashStringHelper *fstr) {
#if DEBUG_CHECK_DUPES
        auto iterator = std::find(_attached.begin(), _attached.end(), fstr);
        if (iterator != _attached.end()) {
            __DBG_printf("duplicate string='%s'", iterator->c_str());
        }
#endif
        _attached.emplace_back(fstr);
        return _attached.back().c_str();
    }
    const char *attachConstString(const char *cstr) {
#if DEBUG_CHECK_DUPES
        auto iterator = std::find(_attached.begin(), _attached.end(), cstr);
        if (iterator != _attached.end()) {
            __DBG_printf("duplicate string='%s'", iterator->c_str());
        }
#endif
        _attached.emplace_back(cstr);
        return _attached.back().c_str();
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

    StringVector _attached;
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
