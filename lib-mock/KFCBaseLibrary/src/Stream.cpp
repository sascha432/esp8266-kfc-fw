/**
* Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

#include <Arduino_compat.h>
#include "Stream.h"

Stream::Stream(FILE *fp) : Print(), _timeout(1000), _fp(fp), _fp_size(0)
{
    _fp = fp;
    if (fp) {
        _fp_seek(0, SeekEnd);
        _fp_size = _fp_position();
        _fp_seek(0);
    }
}

void Stream::setTimeout(unsigned long timeout) 
{ 
    _timeout = timeout; 
}

size_t Stream::size() const 
{
    return _fp_size;
}

int Stream::read() {
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
    CHECK_MEMORY();
    unsigned val = b;
    return val;
}

String Stream::readStringUntil(char terminator) {
    String buf;
    int ch;
    while ((ch = read()) != -1) {
        buf += (char)ch;
        if (ch == terminator) {
            break;
        }
    }
    return buf;
}

String Stream::readString() {
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

void Stream::flush() {
    CHECK_MEMORY();
    if (_fp) {
        fflush(_fp);
    }
}

size_t Stream::readBytes(char *buffer, size_t length) {
    return readBytes((uint8_t *)buffer, length);
}

size_t Stream::readBytes(uint8_t *buffer, size_t length) {
    size_t written = 0;
    while (available() && length--) {
        *buffer++ = read();
        written++;

    }
    return written;
    //return _fp_read((uint8_t *)buffer, length);
}

int Stream::_fp_available() const {
    CHECK_MEMORY();
    return _fp ? (ferror(_fp) || feof(_fp) ? 0 : 1) : 0;
}

FILE *Stream::_fp_get() const {
    return _fp;
}

size_t Stream::_fp_position() const {
    return _fp ? ftell(_fp) : 0;
}

void Stream::_fp_set(FILE *fp) {
    _fp = fp;
    _fp_seek(0, SeekEnd);
    _fp_size = _fp_position();
    _fp_seek(0, SeekSet);
    clearerr(_fp);
}

bool Stream::_fp_seek(long pos, SeekMode mode) {
    if (!_fp) {
        return 0;
    }
    clearerr(_fp);
    int res = fseek(_fp, pos, mode);
    if (res != 0) {
        perror("seek");
    }
    return res == 0;
}

void Stream::_fp_close() {
    CHECK_MEMORY();
    if (_fp) {
        fclose(_fp);
        _fp = nullptr;
    }
}

int Stream::_fp_read(uint8_t *buffer, size_t len) {
    if (!_fp) {
        return 0;
    }
    //DEBUG_ASSERT(buffer);
    //DEBUG_ASSERT(len <= INTPTR_MAX);
    auto res = fread(buffer, 1, len, _fp);
    if (ferror(_fp)) {
        perror("read");
    }
    CHECK_MEMORY();
    return res;
}

size_t Stream::_fp_write(uint8_t data) {
    if (!_fp) {
        return 0;
    }
    auto res = fwrite(&data, 1, 1, _fp);
    if (ferror(_fp)) {
        perror("write");
    }
    return res;
}

int Stream::_fp_peek() {
    if (!_fp) {
        return 0;
    }
    long pos = ftell(_fp);
    int res = _fp_read();
    fseek(_fp, pos, SEEK_SET);
    clearerr(_fp);
    return res;
}

int Stream::_fp_read() {
    if (!_fp) {
        return 0;
    }
    uint8_t buf[1];
    int res = _fp_read(buf, 1);
    if (res != 1) {
        return -1;
    }
    return *buf;
}

#endif
