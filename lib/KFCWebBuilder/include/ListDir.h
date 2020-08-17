/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <FS.h>

#include <push_pack.h>

class ListDir
{
public:
    typedef struct __attribute__packed__ {
        uint32_t uuid;
        uint32_t size;
        uint32_t orgSize;
        uint32_t mtime;
        uint8_t gzipped: 1;
        uint8_t isDir: 1;
        uint8_t ___reserved: 6;
    } ListingsHeader_t;

    typedef struct {
        bool valid;
        ListingsHeader_t header;
        String filename;
    } Listing_t;

    ListDir();
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

    bool showHiddenFiles() const {
        return _hiddenFiles;
    }

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
    Listing_t _listing;
    DirectoryVector _dirs;
    bool _isDir: 1;
    bool _filterSubdirs: 1;
    bool _hiddenFiles: 1;

#if ESP8266

private:
    fs::Dir _dir;

#elif ESP32

private:
    fs::File _dir;
    fs::File _file;
#endif
};


#include <pop_pack.h>
