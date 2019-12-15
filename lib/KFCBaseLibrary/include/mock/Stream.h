/**
* Author: sascha_lammers@gmx.de
*/

#pragma once


#include <stdlib.h>
#include "WString.h"
#include "Print.h"

enum SeekMode { SeekSet = SEEK_SET, SeekCur = SEEK_CUR, SeekEnd = SEEK_END };

class Stream : public Print {
public:
    Stream() : Print() {
        _fp = nullptr;
        _size = 0;
    }
    Stream(FILE *fp) : _fp(fp) {
        if (fp) {
            seek(0, SeekEnd);
            _size = position();
            seek(0);
        }
    }
    //virtual ~Stream() {
    //	if (_fp) {
    //           fclose(_fp);
    //	}
    //}

    FILE *getFile() {
        return _fp;
    }
    void setFile(FILE *fp) {
        _fp = fp;
        seek(0, SeekEnd);
        _size = position();
        seek(0);
    }

    virtual size_t position() const {
        return ftell(_fp);
    }

    virtual bool seek(long pos, int mode) {
        int res = fseek(_fp, pos, mode);
        if (res != 0) {
            perror("seek");
        }
        // printf("DEBUG seek %d %d = %d => %d\n", pos, mode, res, res == 0);
        return res == 0;
        // return fseek(_fp, pos, mode) == 0;
    }
    bool seek(long pos) {
        return seek(pos, SeekSet);
    }

    virtual size_t size() const {
        return _size;
    }

    virtual operator bool() const {
        return _fp != nullptr;
    }

    virtual int read() {
        if (!_fp || ferror(_fp)) {
            return -1;
        }
        uint8_t b;
        if (fread(&b, sizeof(b), 1, _fp) != 1) {
            if (ferror(_fp)) {
                perror("read");
            }
            printf("DEBUG read -1\n");
            return -1;
        }
        // printf("DEBUG read byte\n");
        unsigned val = b;
        return val;
    }

    virtual int peek() {
        long pos = ftell(_fp);
        int res = read();
        fseek(_fp, pos, SEEK_SET);
        return res;
    }

    int read(uint8_t *buffer, size_t len) {
        uint8_t *ptr = buffer;
        while (len--) {
            int ch = read();
            if (ch == -1) {
                break;
            }
            *ptr++ = ch;
        }
        return ptr - buffer;
    }

    String readStringUntil(char terminator) {
        int ch;
        String buf;
        while (true) {
            if (!available() || (ch = read()) == -1) {
                printf("readStringUntil(%d) failed\n", terminator);
                break;
            }
            if (ch == terminator) {
                break;
            }
            if (buf.length() > 1024) {
                printf("readStringUntil(%d) failed, reached 1024byte length\n", terminator);
                return String();
            }
            buf += (char)ch;
        }
        return buf;
    }

    String readString() {
        String buf;
        while (available()) {
            int ch = read();
            if (ch == -1) {
                break;
            }
            buf += (char)ch;
        }
        return buf;
    }

    virtual size_t readBytes(char *buffer, size_t length) {
        return read((uint8_t *)buffer, length);
    }
    virtual size_t readBytes(uint8_t *buffer, size_t length) {
        return readBytes((char *)buffer, length);
    }

    size_t write(uint8_t data) override {
        auto res = fwrite(&data, 1, 1, _fp);
        if (ferror(_fp)) {
            perror("write");
        }
        return res;
    }

    virtual void close() {
        if (_fp) {
            fclose(_fp);
        }
    }

    virtual void flush() {
        fflush(_fp);
    }

    virtual int available() {
        return feof(_fp) ? 0 : 1;
    }

private:
    FILE *_fp;
    size_t _size;
};
