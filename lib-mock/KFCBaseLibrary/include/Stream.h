/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <stdlib.h>
#include "WString.h"
#include "Print.h"

enum SeekMode { SeekSet = SEEK_SET, SeekCur = SEEK_CUR, SeekEnd = SEEK_END };

/*

Print
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size); = calling write(uint8_t)
    virtual void flush() {}

Stream
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    virtual size_t readBytes(char *buffer, size_t start_length); // read chars from stream into buffer
    virtual size_t readBytes(uint8_t *buffer, size_t start_length) {
        return readBytes((char *) buffer, start_length);
    }
    virtual String readString();



*/

class Stream : public Print {
protected:
    unsigned long _startMillis;
    unsigned long _timeout;
public:
    Stream(FILE *fp = nullptr);

    void setTimeout(unsigned long timeout);

    virtual size_t size() const;

    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;

    virtual size_t readBytes(char *buffer, size_t length);
    virtual size_t readBytes(uint8_t *buffer, size_t length);
    virtual void flush();
    virtual String readString();

    String readStringUntil(char terminator);

public:
    FILE *_fp_get() const;
    void _fp_set(FILE *fp);
    size_t _fp_position() const;
    bool _fp_seek(long pos, SeekMode mode = SeekSet);
    void _fp_close();
    int _fp_available() const;
    int _fp_read(uint8_t *buffer, size_t len);
    size_t _fp_write(uint8_t data);
    int _fp_read();
    int _fp_peek();

private:
    FILE *_fp;
    size_t _size;
};
