/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <FSImpl.h>
#include "ProgmemStream.h"

class ProgmemFileImpl : public fs::FileImpl {
public:
    ProgmemFileImpl(const ProgmemStream &stream, const char *name);

    virtual size_t read(uint8_t* buf, size_t size) override;
    virtual size_t write(const uint8_t *, size_t) override  {
        return 0;
    }
    virtual void flush() {
    }

    virtual bool seek(uint32_t pos, SeekMode mode);
    virtual size_t position() const;
    virtual size_t size() const;

    virtual void close() {
        _stream.close();
    }

    virtual const char *name() const;

#if defined(ESP32)

    virtual time_t getLastWrite() {
        return 0;
    }

    virtual boolean isDirectory(void) {
        return false;
    }

    virtual fs::FileImplPtr openNextFile(const char* mode) {
        return nullptr;
    }

    virtual void rewindDirectory(void) {
    }

    virtual operator bool() {
        return (bool)_stream;
    }


#elif defined(ARDUINO_ESP8266_RELEASE_2_5_2)

    virtual bool truncate(uint32_t size) override {
        return false;
    }

    virtual const char* fullName() const override {
        return name();
    }

    virtual bool isFile() const override {
        return true;
    }

    virtual bool isDirectory() const override {
        return false;
    }

#endif


private:
    ProgmemStream _stream;
    std::unique_ptr<char []> _name;
};
