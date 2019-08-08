/**
* Author: sascha_lammers@gmx.de
*/

#include "BufferStream.h"

BufferStream::BufferStream() : Buffer(), Stream() {
    _position = 0;
}

int BufferStream::available() {
    return length() - _position;
}

bool BufferStream::seek(long pos, int mode) {
    if (mode == SEEK_SET) {
        if ((size_t)pos >= Buffer::size()) {
            return false;
        }
        _position = pos;
    }
    else if (mode == SEEK_CUR) {
        if ((size_t)(_position + pos) >= Buffer::size()) {
            return false;
        }
        _position += pos;
    }
    else if (mode == SEEK_END) {
        if ((size_t)pos > Buffer::size()) {
            return false;
        }
        _position = Buffer::size() - pos;
    }
    return true;
}

char BufferStream::charAt(size_t pos) const {
    if (pos < length()) {
        return _buffer[pos];
    }
    return 0;
}

size_t BufferStream::position() const {
    return _position;
}

int BufferStream::read() {
    if (_position < length()) {
        return _buffer[_position++];
    }
    return -1;
}

int BufferStream::peek() {
    if (_position < length()) {
        return _buffer[_position];
    }
    return -1;
}

void BufferStream::clear() {
    Buffer::clear();
    _position = 0;
}
