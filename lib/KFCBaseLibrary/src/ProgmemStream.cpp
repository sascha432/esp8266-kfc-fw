/**
 * Author: sascha_lammers@gmx.de
 */

#include <algorithm>
#include <pgmspace.h>
#include "ProgmemStream.h"
#include "ProgmemFileImpl.h"

ProgmemStream::ProgmemStream() : ProgmemStream(nullptr, 0) {
}

ProgmemStream::ProgmemStream(PGM_P content, size_t size) {
    _content = content;
    _size = size;
    _position = 0;
}

ProgmemStream::~ProgmemStream() {
}

File ProgmemStream::open() const {
    char name[32];
    snprintf_P(name, sizeof(name), PSTR("PGM_P@%p"), _content);
    return File(std::make_shared<ProgmemFileImpl>(ProgmemFileImpl(*this, name)));
}

int ProgmemStream::available() {
    int32_t left = _size - _position;
    return std::min(left, 32767);
}

int ProgmemStream::read() {
    if (available()) {
        int c = pgm_read_byte(&_content[_position++]);
        return c;
    } else {
        return -1;
    }
}

int ProgmemStream::peek() {
    if (available()) {
        int c = pgm_read_byte(&_content[_position]);
        return c;
    } else {
        return -1;
    }
}

size_t ProgmemStream::read(uint8_t *buffer, size_t length) {
    size_t count = 0;
    int c;
    while (count < length && (c = read()) >= 0) {
        *buffer++ = (uint8_t)c;
        count++;
    }
    return count;
}

size_t ProgmemStream::write(uint8_t) {
    return 0;
}

size_t ProgmemStream::write(const uint8_t * buffer, size_t size) {
    return 0;
}

bool ProgmemStream::seek(uint32_t pos, SeekMode mode) {
    if (mode == SeekCur) {
        if (_position + pos >= _size) {
            return false;
        }
        _position += pos;
    } else if (mode == SeekEnd) {
        if (pos > _size) {
            return false;
        }
        _position = _size - pos;
    } else if (mode == SeekSet) {
        if (pos >= _size) {
            return false;
        }
        _position = pos;
    }
    return true;
}

size_t ProgmemStream::position() const {
    return _position;
}

size_t ProgmemStream::size() const {
    return _size;
}

void ProgmemStream::close() {
    _content = nullptr;
    _position = 0;
    _size = 0;
}
