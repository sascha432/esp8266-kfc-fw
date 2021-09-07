/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

class ListDir
{
public:
    static constexpr uint32_t kDirectoryUUID = ~0U;

    #include <push_pack.h>

    struct __attribute__packed__ ListingsHeader {
        uint32_t uuid;
        uint32_t size;
        uint32_t orgSize;
        uint32_t mtime;
        bool gzipped: 1;
        bool isDir: 1;
        uint8_t ___reserved: 6;

        ListingsHeader(bool directory = false) :
            uuid(directory ? kDirectoryUUID : 0),
            size(0),
            orgSize(0),
            mtime(0),
            gzipped(false),
            isDir(directory),
            ___reserved(0)
        {
        }

        operator char *() {
            return reinterpret_cast<char *>(this);
        }
    };

    #include <pop_pack.h>

    struct Listing {
        bool valid;
        ListingsHeader header;
        String filename;

        Listing() : valid(false)
        {
        }
    };

    ListDir(const String &dirName, bool filterSubdirs = false, bool hiddenFiles = true);

    File openFile(const char *mode);

    String fileName();
    size_t fileSize();
    time_t fileTime();

    bool next();
    bool rewind();
    bool isFile() const;
    bool isDirectory() const;
    bool isMapping() const;

    bool showHiddenFiles() const;

private:
    // count number of subdirectories after _dirName
    uint8_t _countSubDirs(const String &path, uint8_t limit = 2) const;
    // returns true if path should be included
    // for _hiddenFiles = true it is always true
    #if USE_LITTLEFS
        bool _showPath(const char *path) const;
    #else
        bool _showPath(const String &path) const;
    #endif
    // return name of the first subdirectory after _dirName, including trailing /
    String _getFirstSubDirectory(const String &path) const;
    void _openDir();

private:
    using DirectoryVector = std::vector<uint16_t>;

    String _filename;
    String _dirName;
    Dir _dir;
    File _vfsFile;
    Listing _vfs;
    DirectoryVector _dirs;
    #if !USE_LITTLEFS
        bool _isDir;
    #endif
    bool _filterSubdirs;
    bool _hiddenFiles;
};

inline __attribute__((__always_inline__))
File ListDir::openFile(const char* mode)
{
    return _dir.openFile(mode);
}

inline String ListDir::fileName()
{
    if (_vfs.valid) {
        return _vfs.filename;
    }
    return _filename;
}

inline size_t ListDir::fileSize()
{
    if (_vfs.valid) {
        return _vfs.header.orgSize;
    }
    return _dir.fileSize();
}

inline time_t ListDir::fileTime()
{
    if (_vfs.valid) {
        return _vfs.header.mtime;
    }
    #if ESP32 || USE_LITTLEFS
        return _dir.fileTime();
    #elif ESP8266
        auto time = _dir.fileTime();
        if (time == 0) {
            time = _dir.fileCreationTime();
        }
        return time;
    #endif
}

inline __attribute__((__always_inline__))
bool ListDir::isFile() const
{
    return !isDirectory();
}

inline bool ListDir::isMapping() const
{
    return _vfs.valid && (_vfs.header.uuid != kDirectoryUUID);
}

inline __attribute__((__always_inline__))
bool ListDir::showHiddenFiles() const
{
    return _hiddenFiles;
}

inline bool ListDir::rewind()
{
    _vfsFile = KFCFS.open(FSPGM(fs_mapping_listings), fs::FileOpenMode::read);
    _vfs = Listing();
    #if !USE_LITTLEFS
        _isDir = false;
    #endif
    return _dir.rewind();
}

inline bool ListDir::isDirectory() const
{
    return _vfs.valid ? _vfs.header.isDir : (
        #if !USE_LITTLEFS
            _isDir ||
        #endif
        _dir.isDirectory());
}

inline void ListDir::_openDir()
{
    // read file system
    _vfsFile.close();
    _vfs.valid = false;
    _dir = KFCFS_openDir(_dirName);
}

#if USE_LITTLEFS

inline bool ListDir::_showPath(const char *path) const
{
    return _hiddenFiles || (*path != '.');
}

#endif
