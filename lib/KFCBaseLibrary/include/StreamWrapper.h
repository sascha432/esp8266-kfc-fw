/**
 * Author: sascha_lammers@gmx.de
 */

/**
 * Write to multiple streams at once
 **/

#pragma once

#include <Arduino_compat.h>
#include <vector>

typedef std::vector<Stream *> streamWrapperVector;

class StreamWrapper : public Stream {
public:
    StreamWrapper();
    StreamWrapper(Stream *output, Stream *input = nullptr);
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
    streamWrapperVector &getChildren();

    virtual int available();
    virtual int read();
    virtual int peek();
    virtual size_t readBytes(char *buffer, size_t length);

    virtual size_t write(uint8_t);
    virtual size_t write(const uint8_t *buffer, size_t size);

    virtual void flush() {}

private:
    streamWrapperVector _children;
    Stream *_input;
};
