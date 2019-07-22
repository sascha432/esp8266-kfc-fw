/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <memory>
#include <map>

class __FSMapping {
public:
    __FSMapping() {
        _path = nullptr;
    }
    __FSMapping(const char *path) {
        _gzipped = false;
        _path = path;
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
    const bool isValid() const {
        return _path != nullptr;
    }

    const char *getPath() const {
        return _path;
    }
    void setPath(const char *path) {
        _path = path;
    }
    const bool isPath(const char *path) const {
        return strcmp(_path, path) == 0;
    }

    const bool isGzipped() const {
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
    FSMapping() : __FSMapping() {
    }
    FSMapping(const char *path, const char *mappedPath, time_t modificationTime, uint32_t fileSize) : __FSMapping(path) {
        _mappedPath = mappedPath;
        _modificationTime = modificationTime;
        _fileSize = fileSize;
    }

    const time_t getModificatonTime() const {
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

    const bool isMapped(const char *mapped) const {
        return strcmp(_mappedPath, mapped) == 0;
    }

    uint32_t getFileSize() const;

    std::unique_ptr<uint8_t []> &getHashPtr();
    const char *getHash() const;
    const String getHashString() const;

    bool setHashFromHexStr(const char *hash);
    bool setHash(uint8_t *hash);

    const uint8_t getHashSize() const;

    const File open(const char *mode) const;

private:
    const char *_mappedPath;
    std::unique_ptr<uint8_t []> _hash;
    uint32_t _fileSize;
    time_t _modificationTime;
};


class Mappings;

typedef std::vector<FSMapping> FileMappingsList;
typedef std::vector<FSMapping>::iterator FileMappingsListIterator;

class Mappings {
    enum Flags {
        FLAGS_GZIPPED = FS_MAPPINGS_FLAGS_GZIPPED,
    };

public:
    Mappings();
    ~Mappings();

    static Mappings &getInstance();

    bool empty() {
        return _mappings.empty();
    }
    const FSMapping &get(const __FlashStringHelper *path) const {
        char buf[strlen_P((PGM_P)path) + 1];
        strcpy_P(buf, reinterpret_cast<PGM_P>(path));
        return get(buf);
    }
    const FSMapping &get(const String &path) const {
        return get(path.c_str());
    }
    const FSMapping &get(const char *path) const;

    FSMapping *find(const String &path) const {
        return find(path.c_str());
    }
    FSMapping *find(const char *path) const;

    const FSMapping &getByMappedPath(const String &mappedPath) const {
        return getByMappedPath(mappedPath.c_str());
    }
    const  FSMapping &getByMappedPath(const char *mappedPath) const;

    const FSMapping &getInvalid() const {
        return Mappings::_invalidMapping;
    }


    bool rename(const char* pathFrom, const char* pathTo);
    bool remove(const char *path);


// private:
    bool loadHashes();
    void _loadMappings(bool extended = false);
    void _storeMappings(bool sort = false);
    void _freeMappings();

    void dump(Print &stream);

    FileMappingsListIterator getBeginIterator() {
        return _mappings.begin();
    }
    FileMappingsListIterator getEndIterator() {
        return _mappings.end();
    }

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
