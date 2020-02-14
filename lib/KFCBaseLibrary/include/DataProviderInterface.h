/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <BufferStream.h>
#include <StreamString.h>

#ifndef DEBUG_DATA_PROVIDER_INTERFACE
#define DEBUG_DATA_PROVIDER_INTERFACE                    1
#endif

#if DEBUG_DATA_PROVIDER_INTERFACE
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

class DataProviderInterface
{
public:
    typedef std::function<size_t(uint8_t *data, size_t size)> FillBufferCallback;
    typedef std::function<bool(const String &name, DataProviderInterface &provider)> ResolveCallback;

    DataProviderInterface(ResolveCallback callback) : _fillBuffer(), _callback(callback), _handled(false) {
    }
    virtual ~DataProviderInterface() {
        end();
    }

    void setResolveCallback(ResolveCallback callback) {
        _callback = callback;
    }

    void setFillBuffer(FillBufferCallback fillBuffer) {
        _fillBuffer = fillBuffer;
    }

    virtual operator bool() const {
        return _handled;
    }

    virtual bool begin(const String &name) {
        if (_handled) {
            end();
        }
        if (_callback) {
            _handled = _callback(name, *this);
        }
        _debug_printf_P(PSTR("name=%s handled=%p callback=%p fillBuffer=%p\n"), name.c_str(), _handled, &_callback, &_fillBuffer);
        return _handled;
    }

    virtual void end() {
        _handled = false;
        _fillBuffer = nullptr;
    }

    virtual size_t fillBuffer(uint8_t *data, size_t size) {
        size_t len = 0;
        if (_fillBuffer) {
            len = _fillBuffer(data, size);
            //_debug_printf_P(PSTR("fillBuffer size=%d: result=%d\n"), size, len);
            if (len == 0) {
                setFillBuffer(nullptr);
            }
        }
        return len;
    }

protected:
    FillBufferCallback _fillBuffer;
    ResolveCallback _callback;
    bool _handled;
};

#include "debug_helper_disable.h"
