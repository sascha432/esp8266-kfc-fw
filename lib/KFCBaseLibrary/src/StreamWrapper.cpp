/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include <NullStream.h>
#include "StreamWrapper.h"

#if 1
#define __DSW(fmt, ...)             ::printf(PSTR("SW:" fmt "\n"), ##__VA_ARGS__);
#else
#define __DSW(...)                  ;
#endif

extern NullStream NullSerial;

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

StreamWrapper::StreamWrapper() : StreamWrapper(&NullSerial)
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
    __DSW("add output=%p", output);
    if (std::find(_streams->begin(), _streams->end(), output) != _streams->end()) {
        __DSW("IGNORING DUPLICATE STREAM %p", output);
        return;
    }
    _streams->push_back(output);
}

void StreamWrapper::remove(Stream *output)
{
    __DSW("remove output=%p", output);
    _streams->erase(std::remove(_streams->begin(), _streams->end(), output), _streams->end());
    if (_streams->empty() || _input == output) {
        setInput(&NullSerial);
    }
}

void StreamWrapper::clear()
{
    __DSW("clear");
    _streams->clear();
    setInput(&NullSerial);
}

void StreamWrapper::replaceFirst(Stream *output, Stream *input)
{
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
    for(const auto stream: *_streams) {
        if (stream->write(data) != sizeof(data)) {
            delay(1); // 1ms per byte
            __DSW("stream=%p write %u/%u", stream, 0, 1);
            stream->write(data);
        }
    }
    return sizeof(data);
}

size_t StreamWrapper::write(const uint8_t *buffer, size_t size)
{
    for(const auto stream: *_streams) {
        auto len = size;
        auto ptr = buffer;
        auto timeout = millis() + 100;
        do {
            auto written = stream->write(ptr, len);
            if (written < len) {
                __DSW("stream=%p write %u/%u", stream, written, len);
                ptr += written;
                delay(1);
            }
            len -= written;
        }
        while (len && millis() < timeout);
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
