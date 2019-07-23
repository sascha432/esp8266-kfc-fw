/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

class ProgmemStream : public Stream {
public:
    ProgmemStream();
    ProgmemStream(PGM_P content, size_t size);
    virtual ~ProgmemStream();

    File open() const;

    operator bool() const {
        return _content != nullptr;
    }

    bool operator !() const {
        return _content == nullptr;
    }

    operator void*() const {
        return _content == nullptr ? nullptr : (void *)*this;
    }

    virtual int available() override;
    virtual int read() override;
    virtual int peek() override;
    virtual size_t read(uint8_t *buffer, size_t length);
    size_t read(char *buffer, size_t length) {
        return read((uint8_t *)buffer, length);
    }

    virtual size_t write(uint8_t) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    bool seek(uint32_t pos, SeekMode mode);
    virtual size_t position() const;
    virtual size_t size() const;

    void close();

#if defined(ESP32)
    virtual void flush() {}
#endif

protected:
    PGM_P _content;
    size_t _position;
    size_t _size;
};
