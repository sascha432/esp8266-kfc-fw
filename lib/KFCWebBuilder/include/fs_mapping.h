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

PROGMEM_STRING_DECL(fs_mapping_file);
PROGMEM_STRING_DECL(fs_mapping_dir);

#include <push_pack.h>

// file structure
typedef struct __attribute__packed__ mapping_t {
    uint16_t path_offset;
    FS_MAPPINGS_COUNTER_TYPE mapped_path_uid;
    uint8_t flags;
    uint32_t mtime;
    uint32_t file_size;
} mapping_t;

// memory structure
typedef struct __attribute__packed__ {
    uint16_t pathOffset;
    FS_MAPPINGS_COUNTER_TYPE mappedPathUID;
    uint32_t fileSize: 31;
    uint32_t gzipped: 1;
    time_t modificationTime: 32; // the bitfield prevents using it as pointer. create a copy on stack if passed to time() or other libc functions that might not like unaligned pointers
} FSMappingEntry;

#include <pop_pack.h>

#ifdef strcmp_P
inline int __strcmp_P(const char *str1, const char *str2) {
    return strcmp_P(str1, str2);
}
#else
#define __strcmp_P strcmp_P
#endif

class Mappings {
public:
    Mappings();
    ~Mappings();

    static Mappings &getInstance();

public:
    bool empty() const {
        return !_count;
    }
    size_t size() const {
        return _count;
    }
    const FSMappingEntry *begin() const {
        return &_entries[0];
    }
    const FSMappingEntry *end() const {
        return &_entries[_instance._count];
    }
    // returns nullptr if mappings are not in memory
    static const FSMappingEntry *endIterator() {
        return _instance._entries + _instance._count;
    }

public:
    typedef int(* compareFunc)(const char *, const char *);

    const FSMappingEntry *findEntry(const char *path, compareFunc compare) const;
    const FSMappingEntry *findEntry(const String &path) const {
        return findEntry(path.c_str(), strcmp);
    }
    const FSMappingEntry *findEntry(const char *path) const {
        return findEntry(path, strcmp);
    }
    const FSMappingEntry *findEntry(const __FlashStringHelper *path) const {
        return findEntry(reinterpret_cast<PGM_P>(path), __strcmp_P);
    }

    const char *getPath(const FSMappingEntry *entry) const {
        if (entry) {
            return _data + entry->pathOffset;
        }
        return nullptr;
    }
    const char *getMappedPath(const FSMappingEntry *entry, char *buf, size_t size) const;
    String getMappedPath(const FSMappingEntry *entry) const;

    const File openFile(const FSMappingEntry *mapping, const char *mode) const;

public:
    void dump(Print &output);

    static void gc();
    static const FSMappingEntry *getEntry(const String &path) {
        return getInstance().findEntry(path.c_str());
    }
    static const FSMappingEntry *getEntry(const char *path) {
        return getInstance().findEntry(path);
    }
    static const FSMappingEntry *getEntry(const __FlashStringHelper *path) {
        return getInstance().findEntry(path);
    }
    static const File open(const FSMappingEntry *mapping, const char *mode) {
        return getInstance().openFile(mapping, mode);
    }

private:
    void _gc();
    void _loadMappings();
    bool _readMappings();
    void _freeMappings();

private:
    uint8_t _count;
    char *_data;
    FSMappingEntry *_entries;
    uint32_t _releaseTimeout;

    static Mappings _instance;
};

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
    static Dir openDir(const char *path);
    static Dir openDir(const String &path) {
        return openDir(path.c_str());
    }
};

// since we cannot access FileImpl inside File we need to wrap another one around
class FSMappingFileImp : public FileImpl {
public:
    FSMappingFileImp(const File &file) : _file(file) {
        _debug_printf("FSMappingFileImp(): name=%s\n", file.fullName() ? file.fullName() : "");
    }
    virtual size_t write(const uint8_t *buf, size_t size) {
        return _file.write(buf, size);
    }
    virtual size_t read(uint8_t* buf, size_t size) {
        return _file.read(buf, size);
    }
    virtual void flush() {
        _file.flush();
    }
    virtual bool seek(uint32_t pos, SeekMode mode) {
        return _file.seek(pos, mode);
    }
    virtual size_t position() const {
        return _file.position();
    }
    virtual size_t size() const {
        return _file.size();
    }
    virtual bool truncate(uint32_t size) {
        return _file.truncate(size);
    }
    virtual void close() {
        _file.close();
    }
    virtual const char* name() const {
        return _file.name();
    }
    virtual const char* fullName() const {
        return _file.fullName();
    }
    virtual bool isFile() const {
        return _file.isFile();
    }
    virtual bool isDirectory() const {
        return _file.isDirectory();
    }

private:
    File _file;
};

class FSMappingDirImpl : public DirImpl {
public:
    FSMappingDirImpl(FS &fs, const String &dirName);

    virtual FileImplPtr openFile(OpenMode openMode, AccessMode accessMode);
    virtual const char *fileName() {
        return _fileName.c_str();
    }
    virtual size_t fileSize();
    virtual bool isFile() const {
        return _isValid == FILE;
    }
    virtual bool isDirectory() const {
        return _isValid == DIR;
    }
    virtual bool next();
    virtual bool rewind();

private:
    bool _validate(const String &path);

private:
    typedef enum {
        INVALID = 0,
        FIRST_DIR = -1,
        DIR = 1,
        FILE = 2,
    } ValidEnum_t;

    FS &_fs;
    ValidEnum_t _isValid;
    Dir _dir;
    String _dirName;
    String _fileName;
    String _virtualRoot;
    const FSMappingEntry *_iterator;
    const FSMappingEntry *_end;
    StringVector _dirs;
};
