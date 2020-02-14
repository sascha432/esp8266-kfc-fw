/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <FSImpl.h>

using namespace fs;

class File : public Stream {
public:
    File(FileImplPtr p = FileImplPtr(), FS* baseFS = nullptr) : _p(p), _baseFS(baseFS) {
    }
    File(FILE *fp, FileImplPtr p = FileImplPtr(), FS* baseFS = nullptr) : _p(p), _baseFS(baseFS), Stream(fp) {
    }

    operator bool() const {
        return _fp_get() != nullptr;
    }

    size_t write(uint8_t data) override {
        return _fp_write(data);
    }
    using Print::write;
    //size_t write(const uint8_t *buffer, size_t length) override {
    //    return Print::write(buffer, length);
    //}
    int available() override {
        return _fp_available();
    }
    int read() override {
        return _fp_read();
    }
    int peek() override {
        return _fp_peek();
    }
    using Stream::readBytes;
    size_t readBytes(char *buffer, size_t length)  override {
        return _fp_read((uint8_t *)buffer, length);
    }
    bool seek(uint32_t pos, SeekMode mode = SeekSet) {
        return _fp_seek(pos, mode);
    }
    void close() {
        _fp_close();
    }
    size_t position() const {
        return _fp_position();
    }


private:
    FileImplPtr _p;
    FS* _baseFS;
};

class Dir {
public:

    static void __test();

    Dir(const String directory) : _handle(INVALID_HANDLE_VALUE) {
        _directory = directory;
        _directory.replace('/', '\\');
        if (!_directory.endsWith("\\")) {
            _directory += '\\';
        }
    }
    ~Dir() {
        if (_handle != INVALID_HANDLE_VALUE) {
            FindClose(_handle);
            _handle = INVALID_HANDLE_VALUE;
        }
    }

    bool next() {
        bool result;
        if (_handle == INVALID_HANDLE_VALUE) {
            TCHAR path[MAX_PATH];
            size_t convertedChars = 0;
            mbstowcs_s(&convertedChars, path, MAX_PATH, _directory.c_str(), _TRUNCATE);
            StringCchCat(path, MAX_PATH, TEXT("*"));
            if ((_handle = FindFirstFile(path, &_ffd)) == INVALID_HANDLE_VALUE) {
                return false;
            }
            result = true;
        } else {
            result = FindNextFile(_handle, &_ffd);
        }
        return result;
    }

    bool isDirectory() const {
        return (_handle != INVALID_HANDLE_VALUE) && (_ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool isFile() const {
        return (_handle != INVALID_HANDLE_VALUE) && !(_ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    }

    const String fileName() const {
        String tmp;
        tmp.reserve(wcslen(_ffd.cFileName) + 1);
        size_t convertedChars;
        wcstombs_s(&convertedChars, tmp.begin(), tmp.capacity(), _ffd.cFileName, _TRUNCATE);
        tmp.replace('\\', '/');
        return tmp;
    }

    uint32_t fileSize() const {
        if (isFile()) {
            return _ffd.nFileSizeLow;
        }
        return 0;
    }

    File openFile(const char *mode);

private:
    HANDLE _handle;
    WIN32_FIND_DATA _ffd;
    String _directory;
};


class _SPIFFS {
public:
    File open(const String filename, const char *mode) {
        FILE *fp;
        String modeStr = mode;
        if (modeStr.indexOf('b') == -1 && modeStr.indexOf('t') == -1) {
            modeStr += 'b';
        }
        fp = fopen(filename.c_str(), modeStr.c_str());
        //fopen_s(&fp, filename.c_str(), modeStr.c_str());
        return File(fp);
    }

    Dir openDir(const String dir) {
        return Dir(dir);
    }

    bool exists(const String filename) {
        File file = open(filename, "r");
        bool result = !!file;
        file.close();
        return result;
    }

    bool remove(const String filename) {
        return ::remove(filename.c_str()) == 0;
    }

    bool rename(const String fromPath, const String toPath) {
        return ::rename(fromPath.c_str(), toPath.c_str()) == 0;
    }
};

extern _SPIFFS SPIFFS;
