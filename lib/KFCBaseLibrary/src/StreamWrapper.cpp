/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include <NullStream.h>
#include <interrupts.h>
#include "StreamWrapper.h"

#if DEBUG_SERIAL_HANDLER
#define __DSW(fmt, ...)             ::printf(PSTR("SW:" fmt "\n"), ##__VA_ARGS__);
#else
#define __DSW(...)                  ;
#endif

extern NullStream NullSerial;

StreamCacheVector::StreamCacheVector(uint16_t size) : _size(size)
{
}

void StreamCacheVector::add(String line)
{
    if (_lines.size() < _size) {
        line.trim();
        if (line.length()) {
            _lines.push_back(line);
        }
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

void StreamCacheVector::dump(Stream &output)
{
    for(String line: _lines) {
        line.trim();
        if (line.length()) {
            output.println(line);
            delay(10);
        }
    }
}


StreamWrapper::StreamWrapper(Stream *output, Stream *input) : _streams(new StreamWrapperVector()), _freeStreams(true), _input(input)
{
    add(output);
}

StreamWrapper::StreamWrapper(StreamWrapperVector *streams, Stream *input) : _streams(streams), _freeStreams(false), _input(input)
{
}

// delegating constructors

StreamWrapper::StreamWrapper(Stream *output, nullptr_t input) : StreamWrapper(output, &NullSerial)
{
    add(output);
}

StreamWrapper::StreamWrapper(Stream *stream) : StreamWrapper(stream, stream)
{
}

StreamWrapper::StreamWrapper() : StreamWrapper(&NullSerial, nullptr)
{
}

StreamWrapper::~StreamWrapper()
{
    if (_freeStreams) {
        delete _streams;
    }
}

void StreamWrapper::setInput(Stream *input)
{
    _input = input;
}

void StreamWrapper::setInput(nullptr_t input)
{
    _input = &NullSerial;
}

Stream *StreamWrapper::getInput()
{
    return _input;
}

void StreamWrapper::add(Stream *output)
{
    InterruptLock lock;
    __DSW("add output=%p", output);
    if (std::find(_streams->begin(), _streams->end(), output) != _streams->end()) {
        __DSW("IGNORING DUPLICATE STREAM %p", output);
        return;
    }
    _streams->push_back(output);
}

void StreamWrapper::remove(Stream *output)
{
    InterruptLock lock;
    __DSW("remove output=%p", output);
    _streams->erase(std::remove(_streams->begin(), _streams->end(), output), _streams->end());
    if (_streams->empty() || _input == output) {
        setInput(&NullSerial);
    }
}

void StreamWrapper::clear()
{
    InterruptLock lock;
    __DSW("clear");
    _streams->clear();
    setInput(&NullSerial);
}

void StreamWrapper::replaceFirst(Stream *output, Stream *input)
{
    InterruptLock lock;
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

int StreamWrapper::available()
{
    return _input->available();
}

int StreamWrapper::read()
{
    return _input->read();
}

int StreamWrapper::peek()
{
    return _input->peek();
}

size_t StreamWrapper::readBytes(char *buffer, size_t length)
{
    return _input->readBytes(buffer, length);
}

size_t StreamWrapper::write(uint8_t data)
{
    bool canYield = can_yield();
    for(const auto stream: *_streams) {
        if (stream->write(data) != 1 && canYield) {
            delay(1);
            // __DSW("stream=%p write %u/%u", stream, 0, 1);
            stream->write(data);
        }
    }
    return sizeof(data);
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

void StreamWrapper::flush()
{
    for(const auto stream: *_streams) {
        if (stream != &Serial0 && stream != &Serial1) {
            stream->flush();
        }
    }
}
