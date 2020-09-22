/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <FSImpl.h>

using namespace fs;

class File : public Stream {
public:
    File(FileImplPtr p = FileImplPtr(), FS *baseFS = nullptr);
    File(FILE *fp, FileImplPtr p = FileImplPtr(), FS *baseFS = nullptr);

    operator bool() const {
        return _fp_get() != nullptr;
    }

    size_t write(uint8_t data) override;
    using Print::write;

    //size_t write(const uint8_t *buffer, size_t length) override {
    //    return Print::write(buffer, length);
    //}

    int available() override;
    int read() override;
    int peek() override;
    using Stream::readBytes;

    size_t readBytes(char *buffer, size_t length)  override;
    bool seek(uint32_t pos, SeekMode mode = SeekSet);
    void close();
    size_t position() const;

private:
    FileImplPtr _p;
    FS* _baseFS;
};

class Dir {
public:
    static void __test();

    Dir(const String directory);
    ~Dir();

    bool next();
    bool isDirectory() const;
    bool isFile() const;
    const String fileName() const;
    uint32_t fileSize() const;
    File openFile(const char *mode);

private:
    HANDLE _handle;
    WIN32_FIND_DATA _ffd;
    String _directory;
};

class _SPIFFS {
public:
    File open(const String filename, const char *mode);
    Dir openDir(const String dir);
    bool exists(const String filename);
    bool remove(const String filename);
    bool rename(const String fromPath, const String toPath);

    void begin();
    void end();
};

extern _SPIFFS SPIFFS;
