/**
 * Author: sascha_lammers@gmx.de
 */

#include "ListDir.h"
#include <misc.h>
#include <crc16.h>
#include "fs_mapping.h"

#include "debug_helper_disable.h"
// #include "debug_helper_enable.h"

ListDir::ListDir(const String &dirName, bool filterSubdirs, bool hiddenFiles) :
    _dirName(append_slash(dirName)),
    _vfsFile(KFCFS.open(FSPGM(fs_mapping_listings), FileOpenMode::read)),
    _filterSubdirs(filterSubdirs),
    _hiddenFiles(hiddenFiles)
{
    __LDBG_printf("dirName=%s", _dirName.c_str());
}

// _dirName = "/mydir/subdir0/"
// dir = "/mydir/subdir0/mystuff/many/files.txt"
// returns "/mydir/subdir0/mystuff"
//
// _dirName = "/mydir/subdir0/"
// dir = "/mydir/subdir0/anotherfile.txt"
// returns String()
String ListDir::_getFirstSubDirectory(const String &path) const
{
    auto ptr = strchr(path.c_str() + _dirName.length(), '/');
    if (ptr) {
        return path.substring(0, ptr - path.c_str());
    }
    return String();
}

#if !USE_LITTLEFS

bool ListDir::_showPath(const String &path) const
{
    if (_hiddenFiles) {
        return true;
    }
    auto pos = path.indexOf(F("/."));
    return (
        pos == -1 ||
        pos == (int)path.length() - 2    // emulated directories end with "/." do not treat them as hidden files
    );
}

#endif

uint8_t ListDir::_countSubDirs(const String &dir, uint8_t limit) const
{
    if (_filterSubdirs) {
        #if !defined(ESP8266)
            if (dir.startsWith(_dirName))
        #endif
        {
            const char *ptr = dir.c_str() + _dirName.length(); // always ends with '/'
            while(*ptr) {
                if (*ptr == '/' && --limit == 0) {
                    break;
                }
                ptr++;
            }
            __LDBG_printf("is_sub=%u max=%u ptr=%s dir=%s dirName=%s", limit == 0, limit, ptr, dir.c_str(), _dirName.c_str());
        }
    }
    return limit;
}

bool ListDir::next()
{
    if (_vfsFile) {
        // read virtual files and directories
        while(true) {
            if (_vfsFile.readBytes(_vfs.header, sizeof(_vfs.header)) != sizeof(_vfs.header)) {
                __LDBG_printf("read failure: header pos=%u size=%u", _vfsFile.position(), _vfsFile.size());
                _openDir();
                break;
            }
            else {
                _vfs.filename = _vfsFile.readStringUntil('\n');
                if (!_vfs.filename.length()) {
                    __LDBG_printf("filename empty pos=%u size=%u", _vfsFile.position(), _vfsFile.size());
                    _openDir();
                    break;
                }
                else {
                    __LDBG_printf("uuid=%x filename=%s filesize=%u/%u mtime=%u gzipped=%u dir=%u skip=%u", _vfs.header.uuid, _vfs.filename.c_str(), _vfs.header.size, _vfs.header.orgSize, _vfs.header.mtime, _vfs.header.gzipped, _vfs.header.isDir, !_vfs.filename.startsWith(_dirName));
                    if (_vfs.filename.startsWith(_dirName) && _countSubDirs(_vfs.filename) >= 2 && _showPath(_vfs.filename.c_str())) {
                        _vfs.valid = true;
                        return true;
                    }
                }
            }
        }
    }
    while (_dir.next()) {
        if (_showPath(_dir.fileName())) {

            #if !USE_LITTLEFS || ESP32
                _filename = _dir.fullName();
            #else
                _filename = _dirName + _dir.fileName();
            #endif

            if (_dir.isDirectory()) {
                append_slash(_filename);
            }
            __LDBG_printf("filename=%s is_dir=%d mapping=%u", _filename.c_str(), _dir.isDirectory(), _filename.startsWithIgnoreCase(FSPGM(fs_mapping_dir)));

            if (!_filename.startsWithIgnoreCase(FSPGM(fs_mapping_dir))) {
                __LDBG_printf("file=%s is_dir=%d size=%u time=%u", _filename.c_str(), _dir.isDirectory(), _dir.fileSize(), _dir.fileTime());
                return true;
            }
        }
    }
    return false;
}
