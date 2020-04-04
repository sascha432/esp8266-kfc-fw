/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <WString.h>
#include <FS.h>
#include <FSImpl.h>

using namespace fs;

#ifndef DEBUG_FS_MAPPING
#define DEBUG_FS_MAPPING                    0
#endif

// timeout in milliseconds
#ifndef FS_MAPPING_AUTO_RELEASE_MEMORY
#define FS_MAPPING_AUTO_RELEASE_MEMORY      5000
#endif

#ifdef SPIFFS_OBJ_NAME_LEN
#define FS_MAPPING_MAX_FILENAME_LEN         (SPIFFS_OBJ_NAME_LEN + 1)
#else
#define FS_MAPPING_MAX_FILENAME_LEN         33
#endif

PROGMEM_STRING_DECL(fs_mapping_dir);
PROGMEM_STRING_DECL(fs_mapping_listings);

#include <push_pack.h>

uint32_t crc32b(const void *message, size_t length, uint32_t crc = ~0);

class FileMapping {
public:
    FileMapping() = default;

    FileMapping(uint32_t uuid) : _uuid(uuid) {
        _openByUUID();
    }
    FileMapping(const char *filename) : _filename(filename) {
        _openByFilename();
    }
    FileMapping(const __FlashStringHelper *filename) : _filename(filename) {
        _openByFilename();
    }

    const File open(const char *mode) const;

    const char *getFilename() const {
        return _filename.c_str();
    }

    const String &getFilenameString() const {
        return _filename;
    }

    uint32_t fileSize() const {
        return _fileSize;
    }

    uint32_t getModificationTime() const{
        return _modificationTime;
    }

    bool isGz() const {
        return _gzipped;
    }

    bool exists() const {
        return _filename.length() != 0;
    }

    bool isMapped() const {
        return _uuid != 0;
    }

private:
    void _openByFilename();
    void _openByUUID();

private:
    String _filename;
    uint32_t _uuid;
    struct __attribute__packed__  {
        uint32_t _modificationTime;
        uint32_t _fileSize: 24;
        uint32_t _gzipped : 1;
        uint32_t ___reserved : 7;
    };
};

#include <pop_pack.h>

class SPIFFSWrapper {
public:
    SPIFFSWrapper() {
    }
    static const File open(Dir dir, const char *mode);
    static const File open(const char *path, const char *mode);
    static const File open(const String &path, const char *mode) {
        return open(path.c_str(), mode);
    }

    static bool exists(const char *path);
    static bool exists(const String &path) {
        return exists(path.c_str());
    }
    static bool rename(const char* pathFrom, const char* pathTo);
    static bool rename(const String &pathFrom, const String &pathTo) {
        return rename(pathFrom.c_str(), pathTo.c_str());
    }
    static bool remove(const char *path);
    static bool remove(const String &path) {
        return remove(path.c_str());
    }
    // static Dir openDir(const char *path);
    // static Dir openDir(const String &path) {
    //     return openDir(path.c_str());
    // }
};

// // since we cannot access FileImpl inside File we need to wrap another one around
// class FSMappingFileImp : public FileImpl {
// public:
//     FSMappingFileImp(const File &file) : _file(file) {
//         _debug_printf("FSMappingFileImp(): name=%s\n", file.name() ? file.name() : "");
//     }
//     virtual size_t write(const uint8_t *buf, size_t size) {
//         return _file.write(buf, size);
//     }
//     virtual size_t read(uint8_t* buf, size_t size) {
//         return _file.read(buf, size);
//     }
//     virtual void flush() {
//         _file.flush();
//     }
//     virtual bool seek(uint32_t pos, SeekMode mode) {
//         return _file.seek(pos, mode);
//     }
//     virtual size_t position() const {
//         return _file.position();
//     }
//     virtual size_t size() const {
//         return _file.size();
//     }
//     virtual bool truncate(uint32_t size) {
// #if ESP32
// #else
//         return _file.truncate(size);
// #endif
//     }
//     virtual void close() {
//         _file.close();
//     }
//     virtual const char* name() const {
//         return _file.name();
//     }
//     virtual const char* fullName() const {
//         return name();
//     }
//     virtual bool isFile() const {
// #if ESP32
// #else
//         return _file.isFile();
// #endif
//     }
//     virtual bool isDirectory() const {
// #if ESP32
// #else
//         return _file.isDirectory();
// #endif
//     }
// #if ESP32
//      virtual time_t getLastWrite()  {
//          return 0;
//      }
// #endif

// private:
//     File _file;
// };

// class FSMappingDirImpl : public DirImpl {
// public:
//     FSMappingDirImpl(FS &fs, const String &dirName);

//     virtual FileImplPtr openFile(OpenMode openMode, AccessMode accessMode);
//     virtual const char *fileName() {
//         return _fileName.c_str();
//     }
//     virtual size_t fileSize();
//     virtual bool isFile() const {
//         return _isValid == FILE;
//     }
//     virtual bool isDirectory() const {
//         return _isValid == DIR;
//     }
//     virtual bool next();
//     virtual bool rewind();

// #if ESP32
//     virtual size_t write(const uint8_t *buf, size_t size) {

//     }
//     virtual size_t read(uint8_t* buf, size_t size) {

//     }
//     virtual void flush() {
//     }
//     virtual bool seek(uint32_t pos, SeekMode mode) {

//     }

//     virtual size_t position() const {

//     }
//     virtual size_t size() const {

//     }
//     virtual void close()  {

//     }
//     virtual time_t getLastWrite() {

//     }
//     virtual const char* name() const {

//     }
//     virtual FileImplPtr openNextFile(const char* mode) {

//     }
//     virtual void rewindDirectory(void) {

//     }
//     virtual operator bool() {

//     }
// #endif

// private:
//     bool _validate(const String &path);

// private:
//     typedef enum {
//         INVALID = 0,
//         FIRST_DIR = -1,
//         DIR = 1,
//         FILE = 2,
//     } ValidEnum_t;

//     FS &_fs;
//     ValidEnum_t _isValid;
//     Dir _dir;
//     String _dirName;
//     String _fileName;
//     String _virtualRoot;
//     const FSMappingEntry *_iterator;
//     const FSMappingEntry *_end;
//     StringVector _dirs;
// #if LOGGER
//     StringVector _logs;
//     StringVector::iterator _logsIterator;
// #endif
// };
