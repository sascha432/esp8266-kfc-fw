/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include <NullStream.h>
#if ESP8266
#include <interrupts.h>
#endif
#include "StreamWrapper.h"

extern NullStream NullSerial;

StreamWrapper::StreamWrapper(Stream *output, nullptr_t input) : StreamWrapper(output, &NullSerial)
{
    add(output);
}

StreamWrapper::StreamWrapper() : StreamWrapper(&NullSerial, nullptr)
{
}

void StreamWrapper::setInput(nullptr_t input)
{
    _input = &NullSerial;
}

void StreamWrapper::remove(Stream *output)
{
    #if ESP8266
        InterruptLock lock;
    #elif ESP32
        portMuxLock mLock(_mux);
    #endif
    __DSW("remove output=%p", output);
    _streams->erase(std::remove(_streams->begin(), _streams->end(), output), _streams->end());
    if (_streams->empty() || _input == output) {
        setInput(&NullSerial);
    }
}

void StreamWrapper::clear()
{
    #if ESP8266
        InterruptLock lock;
    #elif ESP32
        portMuxLock mLock(_mux);
    #endif
    __DSW("clear");
    _streams->clear();
    setInput(&NullSerial);
}

void StreamWrapper::add(Stream *output)
{
    #if ESP8266
        InterruptLock lock;
    #elif ESP32
        portMuxLock mLock(_mux);
    #endif
    __DSW("add output=%p", output);
    if (std::find(_streams->begin(), _streams->end(), output) != _streams->end()) {
        __DSW("IGNORING DUPLICATE STREAM %p", output);
        return;
    }
    _streams->push_back(output);
}

void StreamWrapper::replaceFirst(Stream *output, Stream *input)
{
    #if ESP8266
        InterruptLock lock;
    #elif ESP32
        portMuxLock mLock(_mux);
    #endif
    __DSW("output=%p size=%u", output, _streams->size());
    if (_streams->size() > 1) {
        if (_streams->front() == _input) {
            setInput(input);
        }
        _streams->front() = output;
    }
    else {
        add(output);
        setInput(input);
    }
}

void StreamCacheVector::write(const uint8_t *buffer, size_t size)
{
    if (_lines.size() >= _size) {
        return;
    }
    auto ptr = buffer;
    while(size--) {
        auto byte = pgm_read_byte(ptr++);
        switch(byte) {
            case '\r':
                break;
            case '\n':
                if (_buffer.length() < 20) {
                    _buffer += F("<\\n> ");
                }
                else {
                    if (_buffer.length()) {
                        _buffer += '\n';
                        add(_buffer);
                        _buffer.clear();
                    }
                }
                break;
            default:
                _buffer += (char)byte;
                break;
        }
    }
}

size_t StreamWrapper::write(const uint8_t *buffer, size_t size)
{
    #if DEBUG
        if (!buffer) {
            ::printf(PSTR("StreamWrapper::write(nullptr, %u)\n"), size);
            panic();
        }
    #endif
    if (!buffer || !size) {
        return 0;
    }
    bool canYield = can_yield();
    if (canYield) {
        auto start = millis();
        for(const auto stream: *_streams) {
            if (stream == &Serial0 || stream == &Serial1) {
                stream->write(buffer, size);
            }
            else {
                auto len = size;
                auto ptr = buffer;
                do {
                    auto written = stream->write(ptr, len);
                    if (written < len) {
                        ptr += written;
                        delay(1);
                    }
                    #if DEBUG
                        if (written > len) {
                            ::printf(PSTR("written=%u > len=%u\n"), written, len);
                            panic();
                        }
                    #endif
                    len -= written;
                }
                while (len && ((millis() - start) < 1000));
            }
        }
    }
    else {
        for(const auto stream: *_streams) {
            stream->write(buffer, size);
        }
    }
    return size;
}

