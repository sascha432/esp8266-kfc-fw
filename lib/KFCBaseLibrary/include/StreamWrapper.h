/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Write to multiple streams at once
 **/

#pragma once

#include <Arduino_compat.h>
#include <vector>

#if DEBUG_SERIAL_HANDLER
#    define __DSW(fmt, ...) ::printf(PSTR("SW:" fmt "\n"), ##__VA_ARGS__);
#else
#    define __DSW(...) ;
#endif

#ifndef HAVE_MYSERIAL_AND_DEBUGSERIAL
#    define HAVE_MYSERIAL_AND_DEBUGSERIAL 1
#endif

class StreamCacheVector {
public:
    using LineCacheVector = std::vector<String>;

public:
    StreamCacheVector(uint16_t size = 128);

    void add(String line);
    void write(const uint8_t *buffer, size_t size);
    void dump(Stream &output);

    void setSize(uint16_t size) {
        _size = size;
    }

    void clear() {
        _lines.clear();
    }

    LineCacheVector &lines() {
        return _lines;
    };

private:
    LineCacheVector _lines;
    String _buffer;
    uint16_t _size;
};

class StreamWrapper : public Stream {
public:
    typedef std::vector<Stream *> StreamWrapperVector;

    StreamWrapper(const StreamWrapper &) = delete;
    StreamWrapper(StreamWrapper &&) = default;

    StreamWrapper();
    StreamWrapper(Stream *output, Stream *input);
    StreamWrapper(Stream *output, nullptr_t input);
    StreamWrapper(Stream *stream);

    // use provided streams and do not delete streams object
    // can be used to clone a stream wrapper, the original object must not be destroyed
    StreamWrapper(StreamWrapperVector *streams, Stream *input);
    ~StreamWrapper();

    // set stream used as input
    void setInput(Stream *input);
    void setInput(nullptr_t input = nullptr);
    Stream *getInput();

    // manage output streams
    void add(Stream *output);
    void remove(Stream *output);
    void clear();
    // replace first stream and input if it is the first stream. if there isn't any, add the stream
    void replaceFirst(Stream *output, Stream *input);
    void replaceFirst(Stream *stream) {
        replaceFirst(stream, stream);
    }
    size_t size() const {
        return _streams->size();
    }
    size_t empty() const {
        return _streams->empty();
    }
    StreamWrapperVector *getStreams() {
        return _streams;
    }

    virtual int available();
    virtual int read();
    virtual int peek();
    virtual size_t readBytes(char *buffer, size_t length);

    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    virtual void flush();

private:
    StreamWrapperVector *_streams;
    bool _freeStreams;
    Stream *_input;
};

inline StreamCacheVector::StreamCacheVector(uint16_t size) : _size(size)
{
}

inline void StreamCacheVector::add(String line)
{
    if (_lines.size() < _size) {
        line.trim();
        if (line.length()) {
            _lines.push_back(line);
        }
    }
}

inline void StreamCacheVector::dump(Stream &output)
{
    for(String line: _lines) {
        line.trim();
        if (line.length()) {
            output.println(line);
            delay(10);
        }
    }
}

inline StreamWrapper::StreamWrapper(Stream *output, Stream *input) : _streams(new StreamWrapperVector()), _freeStreams(true), _input(input)
{
    add(output);
}

inline StreamWrapper::StreamWrapper(StreamWrapperVector *streams, Stream *input) : _streams(streams), _freeStreams(false), _input(input)
{
}

inline StreamWrapper::StreamWrapper(Stream *stream) : StreamWrapper(stream, stream)
{
}


inline StreamWrapper::~StreamWrapper()
{
    if (_freeStreams) {
        delete _streams;
    }
}

inline void StreamWrapper::setInput(Stream *input)
{
    _input = input;
}

inline Stream *StreamWrapper::getInput()
{
    return _input;
}

inline int StreamWrapper::available()
{
    return _input->available();
}

inline int StreamWrapper::read()
{
    return _input->read();
}

inline int StreamWrapper::peek()
{
    return _input->peek();
}

inline size_t StreamWrapper::readBytes(char *buffer, size_t length)
{
    return _input->readBytes(buffer, length);
}

inline size_t StreamWrapper::write(uint8_t data)
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

inline void StreamWrapper::flush()
{
    for(const auto stream: *_streams) {
        if (stream != &Serial0 && stream != &Serial1) {
            stream->flush();
        }
    }
}
