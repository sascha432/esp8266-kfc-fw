/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <MicrosTimer.h>

#if defined(ESP32)
#include <WiFi.h>
#include <SPIFFS.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>
#include <FS.h>
#else
#error Platform not supported
#endif
#include <time.h>
#include <ProgmemStream.h>
#include "progmem_data.h"
#include "fs_mapping.h"
#if LOGGER
#include "logger.h"
#endif

#if DEBUG_FS_MAPPING
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(fs_mapping_dir, "/webui/");

uint32_t crc32b(const void *message, size_t length, uint32_t crc)
{
    uint32_t byte, mask;
    const uint8_t *ptr = reinterpret_cast<const uint8_t *>(message);

    while (length--) {
        byte = *ptr++;
        crc = crc ^ byte;
        for (int j = 7; j >= 0; j--) {
            mask = -(int32_t)(crc & 1);
            crc = (crc >> 1) ^ (0xedb88320 & mask);
        }
    }
    return ~crc;
}

void FileMapping::_openByFilename()
{
    if (SPIFFS.exists(_filename)) {
        _uuid = 0;
        _fileSize = SPIFFS.open(_filename, fs::FileOpenMode::read).size();
        _modificationTime = 0;
        _gzipped = 0;
        _debug_printf_P(PSTR("file=%s uuid=%08x gz=%u size=%u mtime=%u\n"), _filename.c_str(), _uuid, _gzipped, _fileSize, _modificationTime);
    }
    else {
        _uuid = crc32b(_filename.c_str(), _filename.length());
        _openByUUID();
    }
}

void FileMapping::_openByUUID()
{
    char buf[33];
    snprintf_P(buf, sizeof(buf) - 1, PSTR("%s%08x.lnk"), SPGM(fs_mapping_dir), _uuid);
    auto file = SPIFFS.open(buf, fs::FileOpenMode::read);
    if (file) {
        if (file.readBytes(reinterpret_cast<char *>(&_modificationTime), 8) == 8) {
            _filename = file.readString();
            _debug_printf_P(PSTR("file=%s uuid=%08x gz=%u size=%u mtime=%u\n"), _filename.c_str(), _uuid, _gzipped, _fileSize, _modificationTime);
            return;
        }
    }
    _debug_printf_P(PSTR("file=%s uuid=%08x buf=%s not found\n"), _filename.c_str(), _uuid, buf);
    _uuid = 0;
    _filename = String();
}

const File FileMapping::open(const char *mode) const
{
    _debug_printf_P(PSTR("file=%s uuid=%08x gz=%u size=%u mtime=%u\n"), _filename.c_str(), _uuid, _gzipped, _fileSize, _modificationTime);
    if (_uuid) {
        char buf[33];
        snprintf_P(buf, sizeof(buf) - 1, PSTR("%s%08x"), SPGM(fs_mapping_dir), _uuid);
        return SPIFFS.open(buf, mode);
    }
    else {
        return SPIFFS.open(_filename.c_str(), mode);
    }
}

const File SPIFFSWrapper::open(Dir dir, const char *mode)
{
#if ESP8266
    return dir.openFile(mode);
#else
    return File();
    // return dir.open(mode);
#endif
}

const File SPIFFSWrapper::open(const char *path, const char *mode)
{
#if LOGGER
    auto ptr = path;
    if (*ptr == '/') {
        ptr++;
    }
    if (!strncmp_P(ptr, PSTR("log:"), 4)) {
        return _logger.openLog(ptr);
    }
#endif
    auto file = SPIFFS.open(path, mode);
    if (file) {
        return file;
    }
    auto mapping = FileMapping(path);
    if (mapping.exists()) {
        return mapping.open(mode);
    }
    return File();
}

bool SPIFFSWrapper::exists(const char *path)
{
    if (SPIFFS.exists(path)) {
        return true;
    }
    return FileMapping(path).exists();
}

bool SPIFFSWrapper::rename(const char* pathFrom, const char* pathTo)
{
    if (exists(pathTo)) {
        return false;
    }
    return SPIFFS.rename(pathFrom, pathTo);
}

bool SPIFFSWrapper::remove(const char *path)
{
    return SPIFFS.remove(path);
}

// Dir SPIFFSWrapper::openDir(const char *path)
// {
//     return Dir(DirImplPtr(new FSMappingDirImpl(SPIFFS, path)));
// }

// FSMappingDirImpl::FSMappingDirImpl(FS &fs, const String &dirName) : _fs(fs), _dirName(dirName)
// {
//     append_slash(_dirName);
//     _debug_printf_P(PSTR("FSMappingDirImpl::FSMappingDirImpl(): dirName=%s\n"), _dirName.c_str());
// #if LOGGER
//     if (_dirName.equals(FSPGM(slash))) {
//         _logger.getLogs(_logs);
//         _logsIterator = _logs.begin();
//     }
// #endif
//     rewind();
// }

// FileImplPtr FSMappingDirImpl::openFile(OpenMode openMode, AccessMode accessMode)
// {
//     _debug_printf_P(PSTR("file=%s\n"), _fileName.c_str());
//     String mode;
//     if (openMode == OM_APPEND) {
//         mode = 'a';
//     }
//     else if (openMode == OM_TRUNCATE) {
//         mode = 'w';
//     }
//     else {
//         mode = 'r';
//     }
//     if (accessMode == AM_RW) {
//         mode += '+';
//     }
//     return FileImplPtr(new FSMappingFileImp(SPIFFSWrapper::open(_fileName, mode.c_str())));
// }

// size_t FSMappingDirImpl::fileSize()
// {
//     if (_isValid == FILE) {
//         if (_iterator != _end && Mappings::endIterator() == _end) {
//             return _iterator->fileSize;
//         }
//         return SPIFFSWrapper::open(_fileName, fs::FileOpenMode::read).size();
//     }
//     return 0;
// }

// bool FSMappingDirImpl::rewind()
// {
// #if ESP32
//     return false;
// #else
//     _debug_println();
//     _dirs.clear();
//     _dirs.push_back(_dirName);
//     _dir = _fs.openDir(FSPGM(slash));
//     _isValid = _dir.next() ? FIRST_DIR : INVALID;
//     auto &map = Mappings::getInstance();
//     _iterator = map.begin();
//     _end = map.end();
//     return _isValid != INVALID;
// #endif
// }


// bool FSMappingDirImpl::next()
// {
//     if (_isValid == INVALID) {
//         return false;
//     }

//     if (_end != Mappings::endIterator()) { // mappings were released or have been modified and all iterators have become invalid
//         _isValid = INVALID;
//         return false;
//     }

//     auto &map = Mappings::getInstance();
//     if (_iterator != _end) { // go through virtual files first
//         while(_iterator != _end) {
//             String filename = _virtualRoot + map.getPath(_iterator);
//             ++_iterator;
//             if (_validate(filename)) {
//                 _debug_printf(PSTR("next() = %s %d V%c\n"), _fileName.c_str(), true, isFile() ? 'F' : 'D');
//                 return true;
//             }
//         }
//         _isValid = FIRST_DIR; // reset to intial state once done
//     }

//     // check _dir
//     bool isValid;
//     bool result = false;
//     do {
//         if (_isValid == FIRST_DIR) { // first next() call, we have the result already
//             _isValid = INVALID;
//             result = true;
//         }
//         else {
//             result = _dir.next();
//             if (!result) {
//                 break;
//             }
//         }
//         isValid = _validate(_dir.fileName());
//         if (isValid) { // check if this file is mapped and hide it
//             char buf[FS_MAPPING_MAX_FILENAME_LEN];
//             for (auto it = map.begin(); it != _end; ++it) {
//                 if (_dir.fileName().equals(map.getMappedPath(it, buf, sizeof(buf)))) {
//                     isValid = false;
//                 }
//             }
//         }
//     } while (!isValid);

// #if LOGGER
//     if (!result) {
//         _isValid = INVALID;
//         if(_logsIterator != _logs.end()) {
//             result = true;
//             _fileName = _dirName + *_logsIterator;
//             _isValid = FILE;
//             ++_logsIterator;
//         }
//     }
// #endif

//     _debug_printf(PSTR("next() = %s %d %c\n"), _fileName.c_str(), result, isFile() ? 'F' : 'D');
//     return result;
// }

// bool FSMappingDirImpl::_validate(const String &path)
// {
//     _debug_printf_P(PSTR("FSMappingDirImpl::_validate(%s)\n"), path.c_str());
//     if (!path.startsWith(_dirName)) { // not a match
//         _isValid = INVALID;
//         _debug_printf_P(PSTR("_validate(%s): startWith()=false\n"), path.c_str());
//         return false;
//     }
//     int pos = _dirName.length(); // points to the first character of the file or subdirectory
//     int pos2 = path.indexOf('/', pos + 1);
//     if (pos2 != -1) { // sub directory pos - pos2
//         String fullpath = path.substring(0, pos2);
//         if (std::find(_dirs.begin(), _dirs.end(), fullpath) != _dirs.end()) { // do we have it already?
//             return false;
//         }
//         _dirs.push_back(fullpath);
//         _isValid = DIR;
//         _fileName = fullpath;
//         _debug_printf_P(PSTR("_validate(%s): DIR: %s\n"), path.c_str(), _fileName.c_str());

//     }
//     else if (pos2 == -1) { // filename or "."
//         auto name = path.c_str() + pos;
//         if (*name == '.' && *(name + 1) == 0) {
//             return false;
//         }
//         _isValid = FILE;
//         _fileName = path;
//         _debug_printf_P(PSTR("_validate(%s): FILE: %s\n"), path.c_str(), _fileName.c_str());
//     }
//     else {
//         _isValid = INVALID;
//         debug_printf_P(PSTR("_validate(%s): INVALID\n"), path.c_str());
//         return false;
//     }
//     return true;
// }
