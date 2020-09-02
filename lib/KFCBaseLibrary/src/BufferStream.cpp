/**
* Author: sascha_lammers@gmx.de
*/

#include "BufferStream.h"

int BufferStream::available()
{
    return _available();
}

bool BufferStream::seek(long pos, int mode)
{
    if (mode == SEEK_SET) {
        if ((size_t)pos >= _length) {
            return false;
        }
        _position = pos;
    }
    else if (mode == SEEK_CUR) {
        if ((size_t)(_position + pos) >= _length) {
            return false;
        }
        _position += pos;
    }
    else if (mode == SEEK_END) {
        if ((size_t)pos > _length) {
            return false;
        }
        _position = _length - pos;
    }
    return true;
}

int BufferStream::read()
{
    if (_available()) {
        return _buffer[_position++];
    }
    return -1;
}

int BufferStream::peek()
{
    if (_available()) {
        return _buffer[_position];
    }
    return -1;
}

size_t BufferStream::readBytes(uint8_t *buffer, size_t length)
{
    size_t len;
    if ((len = _available()) != 0) {
        len = std::min(len, length);
        memcpy(buffer, &_buffer[_position], len);
        _position += len;
        return len;
    }
    return 0;
}

void BufferStream::remove(size_t index, size_t count)
{
    if (!count || index >= _length) {
        return;
    }
    if (count > _length - index) {
        count = _length - index;
    }
    if (_position >= index) {
        if (_position < index + count) {
            _position = index;
        }
        else {
            _position -= count;
        }
    }
    Buffer::_remove(index, count);
}

void BufferStream::removeAndShrink(size_t index, size_t count, size_t minFree)
{
    remove(index, count);
    if (_length + minFree < _size) {
        shrink(_length);
    }
}
