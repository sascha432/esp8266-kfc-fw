/**
* Author: sascha_lammers@gmx.de
*/

#include "BufferStream.h"

BufferStream::BufferStream() : Buffer(), Stream(), _position()
{
    debug_printf("len=%u size=%u ptr=%p\n", length(), size(), Buffer::get());
}

int BufferStream::available()
{
    int len = length() - _position;
    return len > 0 ? len : 0;
}

size_t BufferStream::available() const
{
    int len = length() - _position;
    return len > 0 ? len : 0;
}

bool BufferStream::seek(long pos, int mode) 
{
    if (mode == SEEK_SET) {
        if ((size_t)pos >= length()) {
            return false;
        }
        _position = pos;
    }
    else if (mode == SEEK_CUR) {
        if ((size_t)(_position + pos) >= length()) {
            return false;
        }
        _position += pos;
    }
    else if (mode == SEEK_END) {
        if ((size_t)pos > Buffer::length()) {
            return false;
        }
        _position = length() - pos;
    }
    return true;
}

char BufferStream::charAt(size_t pos) const 
{
    if (available()) {
        return _buffer[pos];
    }
    return 0;
}

size_t BufferStream::position() const 
{
    return _position;
}

int BufferStream::read() 
{
    if (available()) {
        return _buffer[_position++];
    }
    return -1;
}

int BufferStream::peek() 
{
    if (available()) {
        return _buffer[_position];
    }
    return -1;
}

size_t BufferStream::_read(uint8_t *buffer, size_t length)
{
    if (available()) {
        auto len = std::min((size_t)available(), length);
        memcpy(buffer, &_buffer[_position], len);
        _position += len;
        return len;
    }
    return 0;    
}

void BufferStream::clear() 
{
    Buffer::clear();
    _position = 0;
}

void BufferStream::reset()
{
    _length = 0;
    _position = 0;
}
