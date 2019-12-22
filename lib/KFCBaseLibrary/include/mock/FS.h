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
        fopen_s(&fp, filename.c_str(), mode);
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
