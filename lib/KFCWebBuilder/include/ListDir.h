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

    ListDir() {}
    ListDir(const String &dirName, bool filterSubdirs = false);

    File openFile(const char *mode);

    String fileName();
    size_t fileSize();
    time_t fileTime();

    bool next();
    bool rewind();
    bool isFile() const;
    bool isDirectory() const;
    bool isMapping() const;

private:
    bool _isSubdir(const String &dir) const;

private:
    String _filename;
    String _dirName;
    File _listings;
    Listing_t _listing;
    bool _isDir;
    bool _filterSubdirs;

#if ESP8266

private:
    fs::Dir _dir;

#elif ESP32

private:
#endif
};

#include <pop_pack.h>
