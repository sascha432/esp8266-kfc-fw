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

class StreamWrapper : public Stream {
public:
    typedef std::vector<Stream *> StreamWrapperVector;

    StreamWrapper();
    StreamWrapper(Stream *output, Stream *input = nullptr);
#if HAVE_MYSERIAL_AND_DEBUGSERIAL
    StreamWrapper(Stream *output, Stream *input, bool setWrapperAsDefault);
#endif
    ~StreamWrapper();

    // set stream used as input
    void setInput(Stream *input = nullptr);
    Stream *getInput();

    // manage output streams
    void add(Stream *output);
    void remove(Stream *output);
    void clear();
    void replace(Stream *output, bool input = false); // replace all
    Stream *first();
    Stream *last();
    size_t count() const;
    StreamWrapperVector &getChildren();

    virtual int available();
    virtual int read();
    virtual int peek();
    virtual size_t readBytes(char *buffer, size_t length);

    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    virtual void flush() {}

private:
    StreamWrapperVector _children;
    Stream *_input;
};
