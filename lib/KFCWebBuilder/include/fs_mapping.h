/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <Buffer.h>
#include <memory>
#include <map>

#ifndef DEBUG_FS_MAPPING
#define DEBUG_FS_MAPPING            1
#endif

#ifndef FS_MAPPING_READ_ONLY
#define FS_MAPPING_READ_ONLY        1
#endif

#include <push_pack.h>

typedef struct __attribute__packed__ mapping_t {
    uint16_t path_offset;
    uint16_t mapped_path_offset;
    uint8_t flags;
    uint32_t mtime;
    uint32_t file_size;
    uint8_t hash[FS_MAPPINGS_HASH_LENGTH];
} mapping_t;

typedef struct __attribute__packed__ {
    uint16_t pathOffset;
    uint16_t mappedPathOffset;
    uint32_t fileSize: 31;
    uint32_t gzipped: 1;
    time_t modificationTime;
} FSMappingEntry;

typedef struct __attribute__packed__ {
    uint8_t hash[FS_MAPPINGS_HASH_LENGTH];
} FSMappingHash;

#include <pop_pack.h>

class Mappings {
public:
    Mappings();
    ~Mappings();

    static Mappings &getInstance();

    int findEntry(const String &path) const {
        return findEntry(path.c_str());
    }
    int findEntry(const char *path) const;

    int findMappedEntry(const String &mappedPath) const {
        return findMappedEntry(mappedPath.c_str());
    }
    int findMappedEntry(const char *mappedPath) const;

    const FSMappingEntry *getEntry(int num) const;
    const char *getMappedPath(int num) const;
    const char *getPath(int num) const;

    const File openFile(int num, const char *mode) const;
    const File openFile(const FSMappingEntry *mapping, const char *mode) const;

#if !FS_MAPPING_READ_ONLY
    bool rename(int num, const char* pathTo);
    bool remove(int num);

    bool loadHashes();
    void _storeMappings(bool sort = false);
#endif

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
        return &_entries[_count];
    }

    void _loadMappings(bool extended = false);
    void _freeMappings();

    void dump(Print &output);

    bool validEntry(int num) const {
        return !((unsigned)num >= (unsigned)_count);
    }

    const char *getPath(const FSMappingEntry *entry) const {
        return _data.getConstChar() + entry->pathOffset;
    }
    const char *getMappedPath(const FSMappingEntry *entry) const {
        return _data.getConstChar() + entry->mappedPathOffset;
    }

private:
    uint8_t _count;
    Buffer _data;
    FSMappingEntry *_entries;
#if !FS_MAPPING_READ_ONLY
    FSMappingHash *_hashes;
#endif
    EventScheduler::TimerPtr _releaseTimer;

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
