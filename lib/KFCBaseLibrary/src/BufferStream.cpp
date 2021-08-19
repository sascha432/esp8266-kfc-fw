/**
* Author: sascha_lammers@gmx.de
*/

#include "BufferStream.h"

#include <debug_helper_enable.h>

bool BufferStream::seek(long pos, SeekMode mode)
{
    switch(mode) {
        case SeekMode::SeekSet:
            if ((size_t)pos >= _length) {
                return false;
            }
            _position = pos;
            break;
        case SeekMode::SeekCur:
            if ((size_t)(_position + pos) >= _length) {
                return false;
            }
            _position += pos;
            break;
        case SeekMode::SeekEnd:
            if ((size_t)pos > _length) {
                return false;
            }
            _position = _length - pos;
            break;
    }
    return true;
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

#if HAVE_BUFFER_STREAM_FS

BufferStreamFS BSFS;

File BufferStreamFS::open(const char *filename, const char *mode)
{
    struct {
        bool read;
        bool write;
        bool append;
        bool binary;
    } modes = {};

    return File();//TODO is disabled

    __LDBG_printf("filename=%s mode=%s", filename, mode);

    while(*mode) {
        switch(*mode++) {
            case 'r':
                modes.read = true;
                break;
            case 'w':
                modes.write = true;
                break;
            case 'b':
                modes.binary = true;
                break;
            case 'a':
                modes.append = true;
                break;
            case '+':
                // modes.read = true;
                // modes.write = true;
                break;
        }
    }

    if (modes.read) {
        if (!_streams) {
            __LDBG_print("no streams available");
            return File();
        }
        auto iterator = _find(filename);
        if (modes.read && iterator == _streams->end()) {
            __LDBG_print("file not found");
            return File();
        }
    }

    if (!modes.write && !modes.append && !modes.read) {
        __LDBG_print("invalid open mode");
        return File();
    }

    if (!_streams) {
        _streams = new FileBufferStreamVector();
        if (!_streams) {
            __LDBG_print("failed to allocate memory");
            return File();
        }
    }

    auto iterator = _find(filename);
    if (iterator->_stream.use_count() > 1) {
        __LDBG_printf("file in use: %u", iterator->_stream.use_count());
        return File();
    }

    if (iterator == _streams->end()) {
        _streams->emplace_back(filename, BufferStreamPtr(new BufferStream()));
    }
    else {
        if (modes.write) {
            iterator->_stream.reset(new BufferStream());
        }
    }
    if (modes.append) {
        iterator->_stream->seek(0, SeekMode::SeekEnd);
    }

    return File(fs::FileImplPtr(new FileBufferStreamImpl(iterator->_stream)));


    // r	rb		open for reading (The file must exist)	beginning
// r+	rb+	r+b	open for reading and writing (The file must exist)	beginning

// w	wb		open for writing (creates file if it doesn't exist). Deletes content and overwrites the file.	beginning
// a	ab		open for appending (creates file if it doesn't exist)	end
// w+	wb+	w+b	open for reading and writing. If file exists deletes content and overwrites the file, otherwise creates an empty new file	beginning
// a+	ab+	a+b	open for reading and writing (append if file exists)	end

    // fs::FileOpenMode::append
}

bool BufferStreamFS::rename(const char* pathFrom, const char* pathTo)
{
    __LDBG_printf("from=%s to=%s", pathFrom, pathTo);
    if (!_streams) {
        return false;
    }
    if (exists(pathTo)) {
        return false;
    }
    auto iterator = _find(pathFrom);
    if (iterator == _streams->end()) {
        return false;
    }

    iterator->rename(pathTo);
    return true;
}

#endif
