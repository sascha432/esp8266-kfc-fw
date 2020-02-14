/**
 * Author: sascha_lammers@gmx.de
 */

#include "ProgmemFileImpl.h"

ProgmemFileImpl::ProgmemFileImpl(const ProgmemStream & stream, const char * name) {
    _stream = stream;
    _name.reset(new char[strlen(name) + 1]);
    strcpy(_name.get(), name);
}

size_t ProgmemFileImpl::read(uint8_t * buf, size_t size) {
    if (!_stream) {
        return 0;
    }
    return _stream.read(buf, size);
}

bool ProgmemFileImpl::seek(uint32_t pos, SeekMode mode) {
    if (!_stream) {
        return false;
    }
    return _stream.seek(pos, mode);
}

size_t ProgmemFileImpl::position() const {
    if (!_stream) {
        return 0;
    }
    return _stream.position();
}

size_t ProgmemFileImpl::size() const {
    if (!_stream) {
        return 0;
    }
    return _stream.size();
}

const char * ProgmemFileImpl::name() const {
    debug_printf("name=%s\n", _name.get());
    return (const char *)_name.get();
}
