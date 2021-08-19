/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#include <push_pack.h>

class ListDir
{
public:
    static constexpr uint32_t kDirectoryUUID = ~0U;

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
        {}

    };

    struct Listing {
        bool valid;
        ListingsHeader header;
        String filename;

        Listing() : valid(false) {}
    };

    static constexpr size_t kListingSize = sizeof(Listing);

    ListDir() {}
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
    bool _showPath(const String &path) const;
    // return name of the first subdirectory after _dirName, including trailing /
    String _getFirstSubDirectory(const String &path) const;


private:
    using DirectoryVector = std::vector<uint16_t>;

    String _filename;
    String _dirName;
    File _listings;
    Listing _listing;
    DirectoryVector _dirs;
    bool _isDir;
    bool _filterSubdirs;
    bool _hiddenFiles;

#if ESP8266

private:
    fs::Dir _dir;

#elif ESP32

private:
    fs::File _dir;
    fs::File _file;
#endif
};

static constexpr size_t kListDir = sizeof(ListDir);


#if ESP8266

inline __attribute__((__always_inline__))
File ListDir::openFile(const char* mode)
{
    return _dir.openFile(mode);
}

#endif

inline String ListDir::fileName()
{
    if (_listing.valid) {
        return _listing.filename;
    }
    return _filename;
}

inline size_t ListDir::fileSize()
{
    if (_listing.valid) {
        return _listing.header.orgSize;
    }
#if ESP8266
    return _dir.fileSize();
#elif ESP32
    return _file.size();
#endif
}

inline time_t ListDir::fileTime()
{
    if (_listing.valid) {
        return _listing.header.mtime;
    }
#if ESP8266
    auto time = _dir.fileTime();
    if (time == 0) {
        time = _dir.fileCreationTime();
    }
    return time;
#elif ESP32
    return 0;
#endif
}

inline __attribute__((__always_inline__))
bool ListDir::isFile() const
{
    return !isDirectory();
}

inline bool ListDir::isMapping() const
{
    return _listing.valid && (_listing.header.uuid != kDirectoryUUID);
}

inline __attribute__((__always_inline__))
bool ListDir::showHiddenFiles() const
{
    return _hiddenFiles;
}

inline bool ListDir::rewind()
{
    _listings = KFCFS.open(FSPGM(fs_mapping_listings), fs::FileOpenMode::read);
    _listing = Listing();
    _isDir = false;
#if ESP8266
    return _dir.rewind();
#elif ESP32
    _dir.rewindDirectory();
    return true;
#endif
}

inline bool ListDir::isDirectory() const
{
    if (_listing.valid) {
        return _listing.header.isDir;
    }
    if (_isDir) {
        return true;
    }
#if ESP8266
    return _dir.isDirectory();
#elif ESP32
    return false;
#endif

}

#include <pop_pack.h>
