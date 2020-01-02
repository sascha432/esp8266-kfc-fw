/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <memory>
#include <map>

class __FSMapping {
public:
    __FSMapping() : _path(nullptr) {
    }
    __FSMapping(const char *path) : _path(path), _gzipped(false) {
    }
    virtual ~__FSMapping() {
    }

    operator bool() const {
        return _path != nullptr;
    }
    bool operator !() const {
        return _path == nullptr;
    }
    operator void*() const {
        return _path == nullptr ? nullptr : (void *)*this;
    }
    bool isValid() const {
        return _path != nullptr;
    }

    const char *getPath() const {
        return _path;
    }
    void setPath(const char *path) {
        _path = path;
    }
    bool isPath(const char *path) const {
        return strcmp(_path, path) == 0;
    }

    bool isGzipped() const {
        return _gzipped;
    }
    void setGzipped(bool gzipped) {
        _gzipped = gzipped;
    }

private:
    const char *_path;
    bool _gzipped;
};

class FSMapping : public __FSMapping {
public:
    FSMapping() : __FSMapping(), _hash(nullptr) {
    }
    FSMapping(const char *path, const char *mappedPath, time_t modificationTime, uint32_t fileSize) : __FSMapping(path) {
        _mappedPath = mappedPath;
        _modificationTime = modificationTime;
        _fileSize = fileSize;
    }
    virtual ~FSMapping() {
        if (_hash) {
            free(_hash);
        }
    }

    FSMapping(FSMapping &&mapping) {
        *this = std::move(mapping);
    }

    FSMapping &operator=(const FSMapping &mapping) {
        memcpy(this, &mapping, sizeof(*this));
        if (mapping._hash) {
            _hash = (uint8_t *)malloc(getHashSize());
            memcpy(_hash, mapping._hash, getHashSize());
        }
        return *this;
    }
    FSMapping &operator=(FSMapping &&mapping) {
        memcpy(this, &mapping, sizeof(*this));
        mapping._hash = nullptr;
        return *this;
    }

    time_t getModificatonTime() const {
        return _modificationTime;
    }
    const time_t *getModificatonTimePtr() const {
        return &_modificationTime;
    }

    void setModificationTime(time_t modificationTime) {
        _modificationTime = modificationTime;
    }

    // SPIFFS filename
    const char *getMappedPath() const {
        return _mappedPath;
    }

    void setMappedPath(const char *mappedPath) {
        _mappedPath = mappedPath;
    }

    bool isMapped(const char *mapped) const {
        return strcmp(_mappedPath, mapped) == 0;
    }

    uint32_t getFileSize() const;

    uint8_t *getHash() const {
        return _hash;
    }
    String getHashString() const;

    bool setHashFromHexStr(const char *hash);
    bool setHash(uint8_t *hash);

    constexpr static uint8_t getHashSize() {
        return FS_MAPPINGS_HASH_LENGTH;
    }

    const File open(const char *mode) const;

private:
    const char *_mappedPath;
    uint8_t *_hash;
    uint32_t _fileSize;
    time_t _modificationTime;
};


class Mappings;

typedef std::vector<FSMapping> FileMappingsList;
typedef std::vector<FSMapping>::const_iterator FileMappingsListIterator;

class Mappings {
    enum Flags {
        FLAGS_GZIPPED = FS_MAPPINGS_FLAGS_GZIPPED,
    };

public:
    Mappings();
    ~Mappings();

    static Mappings &getInstance();

    inline bool empty() const {
        return _mappings.empty();
    }
    inline FileMappingsListIterator get(const String &path) const {
        return get(path.c_str());
    }
    FileMappingsListIterator get(const char *path) const;

    inline FSMapping *find(const String &path) const {
        return find(path.c_str());
    }
    FSMapping *find(const char *path) const;

    inline FileMappingsListIterator getByMappedPath(const String &mappedPath) const {
        return getByMappedPath(mappedPath.c_str());
    }
    FileMappingsListIterator getByMappedPath(const char *mappedPath) const;

    bool rename(const char* pathFrom, const char* pathTo);
    bool remove(const char *path);


// private:
    bool loadHashes();
    void _loadMappings(bool extended = false);
    void _storeMappings(bool sort = false);
    void _freeMappings();

    void dump(Print &output);

    FileMappingsList &getMappings() {
        return _mappings;
    }

    void test();

public:
    static FSMapping _invalidMapping;
private:
    static Mappings _instance;

    FileMappingsList _mappings;
    uint8_t *_data;
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
