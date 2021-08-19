/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "Buffer.h"
#if USE_LITTLEFS
#include <LittleFS.h>
#else
#include <FS.h>
#endif
#if HAVE_BUFFER_STREAM_FS
#include <FSImpl.h>
#endif

class BufferStream : public Buffer, public Stream {
public:
    //using Buffer::Buffer; // using seems to screw up intilizing the object with GCC. adding a constructor that calls the original works fine
    using Buffer::write;
    using Buffer::length;
    using Buffer::size;

    BufferStream() : Buffer(), _position(0) {
    }
    BufferStream(size_t size) : Buffer(size), _position(0) {
    }
    BufferStream(const String &str) : Buffer(str), _position(0) {
    }
    BufferStream(String &&str) : Buffer(str), _position(0) {
    }

    virtual int available() override;
    size_t available() const;

    bool seek(long pos, SeekMode mode);
    bool seek(uint32_t pos) {
        return seek(pos, SeekMode::SeekSet);
    }
    size_t position() const;
    void clear();
    void reset();
    void remove(size_t index, size_t count = ~0);
    void removeAndShrink(size_t index, size_t count = ~0, size_t minFree = 32);

    virtual int read() override;
    virtual int peek() override;

    int charAt(size_t pos) const;

    using Buffer::operator[];

    BufferStream &operator=(const BufferStream &buffer) {
        Buffer::operator=(buffer);
        _position = buffer._position;
        return *this;
    }

    virtual size_t write(uint8_t data) override {
        return Buffer::write(data);
    }
    virtual size_t write(const uint8_t *data, size_t len) override {
        return Buffer::write((uint8_t *)data, len);
    }

    virtual size_t readBytes(char *buffer, size_t length) {
        return readBytes((uint8_t *)buffer, length);
    }
    virtual size_t readBytes(uint8_t *buffer, size_t length);

#if ESP32
    virtual void flush() {
    }
#endif

protected:
    size_t _available() const;

    size_t _position;
};

inline size_t BufferStream::_available() const
{
    int len = _length - _position;
    return len > 0 ? len : 0;
}

inline size_t BufferStream::available() const
{
    return _available();
}

inline int BufferStream::charAt(size_t pos) const
{
    if (_available()) {
        return _buffer[pos];
    }
    return -1;
}

inline size_t BufferStream::position() const
{
    return _position;
}

inline void BufferStream::clear()
{
    Buffer::clear();
    _position = 0;
}

inline void BufferStream::reset()
{
    _length = 0;
    _position = 0;
}

inline int BufferStream::available()
{
    return _available();
}

inline int BufferStream::read()
{
    if (_available()) {
        return _buffer[_position++];
    }
    return -1;
}

inline int BufferStream::peek()
{
    if (_available()) {
        return _buffer[_position];
    }
    return -1;
}

inline size_t BufferStream::readBytes(uint8_t *buffer, size_t length)
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

inline void BufferStream::removeAndShrink(size_t index, size_t count, size_t minFree)
{
    remove(index, count);
    if (_length + minFree < _size) {
        shrink(_length);
    }
}

#if HAVE_BUFFER_STREAM_FS

class BufferStreamFS;

class FileBufferStreamImpl : public fs::FileImpl {
public:
    using BufferStreamPtr = std::shared_ptr<BufferStream>;

    FileBufferStreamImpl(BufferStreamPtr &stream) : _stream(stream) {
    }
    virtual ~FileBufferStreamImpl() {
    }

    virtual size_t write(const uint8_t *buf, size_t size) {
        if (!_stream) {
            return 0;
        }
        return _stream->write(buf, size);
    }

    #if ESP8266
    virtual int read(uint8_t* buf, size_t size)
    #else
    virtual size_t read(uint8_t* buf, size_t size)
    #endif
    {
        if (!_stream) {
            return 0;
        }
        return _stream->readBytes(buf, size);
    }

    virtual void flush() {}

    virtual bool seek(uint32_t pos, SeekMode mode) {
        if (!_stream) {
            return false;
        }
        return _stream->seek(pos, mode);
    }

    virtual size_t position() const {
        if (!_stream) {
            return 0;
        }
        return _stream->position();
    }

    virtual size_t size() const {
        if (!_stream) {
            return 0;
        }
        return _stream->size();
    }

    virtual bool truncate(uint32_t size) {
        if (!_stream) {
            return false;
        }
        return _stream->_changeBuffer(size);
    }


    virtual void close() {
        _stream.reset();
    }

    virtual const char* name() const;
    virtual const char* fullName() const {
        return name();
    }

    virtual bool isFile() const {
        return true;
    }
    virtual bool isDirectory() const {
        return false;
    }

    // virtual void setTimeCallback(time_t (*cb)(void)) { }
    // virtual time_t getLastWrite() { return 0; } // Default is to not support timestamps
    // virtual time_t getCreationTime() { return 0; } // Default is to not support timestamps

protected:
    friend BufferStreamFS;

    BufferStreamPtr _stream;
};

class BufferStreamFS {
public:
    using BufferStreamPtr = FileBufferStreamImpl::BufferStreamPtr;

    class FileBufferStreamMeta {
    public:

        FileBufferStreamMeta(const String filename, const BufferStreamPtr &stream) : _filename(filename), _stream(stream) {}

        bool operator==(const String &str) const {
            return _filename.equals(str);
        }

        bool operator==(const char *str) const {
            return _filename.equals(str);
        }

        bool operator==(const BufferStreamPtr &ptr) const{
            return ptr && _stream && ptr.get() == _stream.get();
        }

        void rename(const char *filename) {
            _filename = filename;
        }

        const char *getFilename() const {
            return _filename.c_str();
        }

    private:
        friend BufferStreamFS;

        String _filename;
        BufferStreamPtr _stream;
    };

    using FileBufferStreamVector = std::vector<FileBufferStreamMeta>;

public:
    BufferStreamFS() : _streams(nullptr) {
    }
    ~BufferStreamFS() {
        end();
    }

    File open(const char *filename, const char *mode);
    File open(const String &filename, const char *mode) {
        return open(filename.c_str(), mode);
    }

    bool exists(const char *path);
    bool exists(const String& path) {
        return exists(path.c_str());
    }

    bool remove(const char* path);
    bool remove(const String& path) {
        return remove(path.c_str());
    }

    bool rename(const char* pathFrom, const char* pathTo);
    bool rename(const String& pathFrom, const String& pathTo) {
        return rename(pathFrom.c_str(), pathTo.c_str());
    }

    void begin() {
    }

    void end() {
        if (_streams) {
            delete _streams;
            _streams = nullptr;
        }
    }

protected:
    friend FileBufferStreamImpl;

    FileBufferStreamVector::iterator _find(const char *path);
    FileBufferStreamVector::iterator _find(const BufferStreamPtr &ptr);

protected:
    FileBufferStreamVector *_streams;
};

extern BufferStreamFS BSFS;

inline const char *FileBufferStreamImpl::name() const
{
    if (!_stream) {
        return emptyString.c_str();
    }
    auto iterator = BSFS._find(_stream);
    if (iterator == BSFS._streams->end()) {
        return emptyString.c_str();
    }
    return iterator->getFilename();
}

inline bool BufferStreamFS::exists(const char *path)
{
    if (!_streams) {
        return false;
    }
    auto iterator = _find(path);
    return (iterator != _streams->end());
}

inline bool BufferStreamFS::remove(const char* path)
{
    __LDBG_printf("path=%s", path);
    if (!_streams) {
        return false;
    }
    auto size = _streams->size();
    _streams->erase(std::remove(_streams->begin(), _streams->end(), path), _streams->end());
    return size != _streams->size();
}

inline BufferStreamFS::FileBufferStreamVector::iterator BufferStreamFS::_find(const char *path)
{
    return std::find(_streams->begin(), _streams->end(), path);
}

inline BufferStreamFS::FileBufferStreamVector::iterator BufferStreamFS::_find(const BufferStreamPtr &ptr)
{
    return std::find(_streams->begin(), _streams->end(), ptr);
}

#endif
