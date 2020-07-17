/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include <NullStream.h>
#include "StreamWrapper.h"

extern NullStream NullSerial;

StreamWrapper::StreamWrapper(Stream *output, Stream *input) : _input(input)
{
    add(output);
}

StreamWrapper::StreamWrapper(Stream *output, nullptr_t input) : _input(&NullSerial)
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
    clear();
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
    _streams.push_back(output);
}

void StreamWrapper::remove(Stream *output)
{
    _streams.erase(std::remove(_streams.begin(), _streams.end(), output), _streams.end());
    if (_streams.empty() || _input == output) {
        setInput(&NullSerial);
    }
}

void StreamWrapper::clear()
{
    _streams.clear();
    setInput(&NullSerial);
}

void StreamWrapper::replace(Stream *output, Stream *input)
{
    _streams.clear();
    add(output);
    setInput(input);
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
    for(const auto stream: _streams) {
        if (stream->write(data) != sizeof(data)) {
            delay(1); // 1ms per byte
            ::printf(PSTR("stream=%p write %u/%u\n"), stream, 0, 1);
            stream->write(data);
        }
    }
    return sizeof(data);
}

size_t StreamWrapper::write(const uint8_t *buffer, size_t size)
{
    for(const auto stream: _streams) {
        auto len = size;
        auto ptr = buffer;
        auto timeout = millis() + 100;
        do {
            auto written = stream->write(ptr, len);
            if (written < len) {
                ::printf(PSTR("stream=%p write %u/%u\n"), stream, written, len);
                ptr += written;
                delay(1);
            }
            len -= written;
        }
        while (len && millis() < timeout);
    }
    return size;
}
