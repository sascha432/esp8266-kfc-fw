/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Write to multiple streams at once
 **/

#pragma once

#include <Arduino_compat.h>
#include <vector>

#ifndef HAVE_MYSERIAL_AND_DEBUGSERIAL
#define HAVE_MYSERIAL_AND_DEBUGSERIAL 1
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
