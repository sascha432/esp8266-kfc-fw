/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino.h>
// __AUTOMATED_HEADERS_STRART
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
// __AUTOMATED_HEADERS_END
#include <time.h>
#include <ProgmemStream.h>
#include "fs_mapping.h"

const char _mappings_file[] PROGMEM = { "/webui/.mappings" };

Mappings Mappings::_instance;
FSMapping Mappings::_invalidMapping;

uint32_t FSMapping::getFileSize() const {
    return _fileSize;
}

std::unique_ptr<uint8_t []> &FSMapping::getHashPtr() {
    return _hash;
}

const String FSMapping::getHashString() const {
    String hashStr;
    uint8_t *hash = _hash.get();
    if (hash) {
        char buf[3];
        uint8_t count = getHashSize();
        while(count--) {
            snprintf_P(buf, sizeof(buf), PSTR("%02x"), (int)(*hash++ & 0xff));
            hashStr += buf;
        }
    }
    return hashStr;
}

const char *FSMapping::getHash() const {
    return (const char *)_hash.get();
}

bool FSMapping::setHashFromHexStr(const char *hash) {
    if (strlen(hash) == getHashSize() * 2) {
        uint8_t hashBuf[getHashSize()];
        uint8_t count = getHashSize();
        uint8_t *dst = hashBuf;
        const char *src = hash;
        char buf[3];
        buf[2] = 0;
        while(count--) {
            buf[0] = *src++;
            buf[1] = *src++;
            *dst++ = (uint8_t)strtoul(buf, NULL, HEX);
        }
        return setHash(hashBuf);
    }
    return false;
}

bool FSMapping::setHash(uint8_t *hash) {
    if (!_hash) {
        _hash.reset(new uint8_t[getHashSize()]);
    }
    if (_hash) {
        memcpy(_hash.get(), hash, getHashSize());
        return true;
    }
    return false;
}

const uint8_t FSMapping::getHashSize() const {
    return FS_MAPPINGS_HASH_LENGTH;
}

bool Mappings::rename(const char* pathFrom, const char* pathTo) {
    // spiffs_t fs;
    // // SPIFFS_LOCK(&fs);
    if (loadHashes()) {
        for(auto mapping = _mappings.begin(); _mappings.end() != mapping; ++mapping) {
            if (strcmp(pathFrom, mapping->getPath()) == 0) {
                char *mappedPath = strdup(mapping->getMappedPath());
                if (mappedPath) {
                    File file = SPIFFS.open(mappedPath, "r");
                    _mappings.push_back(FSMapping(pathTo, mappedPath, time(nullptr), file.size()));
                    file.close();
                    _mappings.back().getHashPtr() = std::move(mapping->getHashPtr());
                    _mappings.erase(mapping);
                    debug_printf_P(PSTR("Renamed %s to %s (%s, %s)"), mapping->getPath(), _mappings.back().getPath(), _mappings.back().getMappedPath(), _mappings.back().getHashString().c_str());
                    _storeMappings(true);
                    free(mappedPath);
                }
                // SPIFFS_UNLOCK(&fs);
                return true;
            }
        }
    } else {
        debug_println(F("Failed to load hashes"));
    }
    // SPIFFS_UNLOCK(&fs);
    return false;
}

const File FSMapping::open(char const *mode) const {
    return SPIFFS.open(getMappedPath(), mode);
}

bool Mappings::remove(const char *path) {
    bool result = false;
    // spiffs_t fs;
    // SPIFFS_LOCK(&fs);
    if (loadHashes()) {
        _mappings.erase(std::remove_if(_mappings.begin(), _mappings.end(), [&](const FSMapping &mapping) {
            bool match = (strcmp(path, mapping.getPath()) == 0);
            if (match) {
                result = SPIFFS.remove(mapping.getMappedPath());
            }
            return match && result;
        }), _mappings.end());
        _storeMappings();
    } else {
        debug_println(F("Failed to load hashes"));
    }
    // SPIFFS_UNLOCK(&fs);
    return result;
}

FSMapping *Mappings::find(const char *path) const {
    const FSMapping &mapping = get(path);
    if (mapping.isValid()) {
        return (FSMapping *)&mapping;
    }
    return nullptr;
}

#if FS_MAPPING_SORTED

const FSMapping &Mappings::get(const char *path) const {
    // debug_printf_P(PSTR("get(%s)\n"), path);
    const auto &result = std::lower_bound(_mappings.begin(), _mappings.end(), path, [](const FSMapping &mapping, const char *path) {
        return strcmp(path, mapping.getPath()) > 0;
    });
    if (result != _mappings.end() && strcmp(path, result->getPath()) == 0) {
        return *result;
    }
    return Mappings::_invalidMapping;
}

#else

const FSMapping &Mappings::get(const char *path) const {
    // debug_printf_P(PSTR("get(%s)\n"), path);
    for(const auto &mapping : _mappings) {
        if (mapping.isPath(path)) {
            return mapping;
        }
    }
    return Mappings::_invalidMapping;
}

#endif

const FSMapping &Mappings::getByMappedPath(const char *mappedPath) const {
    // debug_printf_P(PSTR("getByMappedPath(%s)\n"), mappedPath);
    for(const auto &mapping : _mappings) {
        if (mapping.isMapped(mappedPath)) {
            return mapping;
        }
    }
    return Mappings::_invalidMapping;
}


Mappings &Mappings::getInstance() {
    if (Mappings::_instance.empty()) {
        Mappings::_instance._loadMappings();
    }
    return Mappings::_instance;
}


Mappings::Mappings() {
    _data = nullptr;
}

Mappings::~Mappings() {
    _freeMappings();
}


typedef struct __attribute__((packed)) mapping_t {
    uint16_t path_offset;
    uint16_t mapped_path_offset;
    uint8_t flags;
    uint32_t mtime;
    uint32_t file_size;
    uint8_t hash[FS_MAPPINGS_HASH_LENGTH];
} mapping_t;

bool Mappings::loadHashes() {

    char buf[strlen_P(_mappings_file) + 1];
    strcpy_P(buf, _mappings_file);

    File file = SPIFFS.open(buf, "r");
    if (file) {
        FS_MAPPINGS_COUNTER_TYPE items;
        if (file.read((uint8_t *)&items, sizeof(items)) == sizeof(items)) {
            mapping_t header;
            FS_MAPPINGS_COUNTER_TYPE pos = 0;
            while(pos < items) {
                if (file.read((uint8_t *)&header, sizeof(header)) != sizeof(header) || !_mappings.at(pos).setHash(header.hash)) {
                    return false;
                }
                pos++;
            }
            return true;
        }
    }
    return false;
}

void Mappings::_loadMappings(bool extended) {

/*

store an index of the files in memory, 4 bytes per file

2 byte first 2 letters of the filename
2 bytes the offset of the record on flash

sort the index and user lowerbound and uper bound to find the range to read ferom flash.
maybe it is a good idea to hash the filename ato get an even distribution over the entire index. that should reduice the data to read for each lookuip dramaticially
this hash does not need to be calculated, it will just replace the first 2 letters of the filename (crc16  maybe)

directories can be stored seprately or not

*/


    _freeMappings();

    char buf[strlen_P(_mappings_file) + 1];
    strcpy_P(buf, _mappings_file);

    File file = SPIFFS.open(buf, "r");
    if (file) {
        FS_MAPPINGS_COUNTER_TYPE items;
        if (file.read((uint8_t *)&items, sizeof(items)) == sizeof(items)) {
            // debug_printf_P(PSTR("items %d size %d\n"), items, sizeof(mapping_t));
            uint16_t index_size = items * sizeof(mapping_t);
            // debug_printf_P(PSTR("index size %d\n"), index_size);
            uint16_t data_size  = file.size() - index_size - sizeof(items);
            // debug_printf_P(PSTR("data_size %d\n"), data_size);
            _data = (uint8_t *)malloc(data_size);   // all strings are stored in single block of memory and the offset is passed to FSMapping
            if (_data) {
                if (file.seek(index_size + sizeof(items), SeekSet)) {
                    if (file.read(_data, data_size) == data_size) {
                        if (file.seek(sizeof(items), SeekSet)) {
                            FS_MAPPINGS_COUNTER_TYPE count = items;
                            _mappings.reserve(items);
                            while(count--) {
                                mapping_t header;
                                if (file.read((uint8_t *)&header, sizeof(header)) == sizeof(header)) {
                                    _mappings.push_back(FSMapping((const char *)&_data[header.path_offset], (const char *)&_data[header.mapped_path_offset], header.mtime, header.file_size));
                                    FSMapping &mapping = _mappings.back();
                                    mapping.setGzipped(!!(header.flags & FLAGS_GZIPPED));
                                    if (extended) {
                                        mapping.setHash(header.hash);
                                    }
                                    // debug_printf_P(PSTR("header, offsets %d %d, flags %d, path %s, mapped path %s mtime %lu, file size %lu\n"), (int)header.path_offset, (int)header.mapped_path_offset, (int)header.flags, mapping.getPath(), mapping.getMappedPath(), (ulong)header.mtime, (ulong)header.file_size);
                                } else {
                                    debug_printf_P(PSTR("Cannot read index %d/%d"), count, items);
                                }
                            }
                        }
                    } else {
                        debug_printf_P(PSTR("Cannot read mapping data %d\n"), data_size);
                    }
                } else {
                    debug_printf_P(PSTR("Seek failed offset %d\n"), index_size);
                }
            } else {
                debug_printf_P(PSTR("Cannot allocate mapping buffer %d\n"), data_size);
            }
        } else {
            debug_println(F("Cannot read mapping item count"));
        }
    }

// #if DEBUG
//     dump(&DEBUG_OUTPUT);
// #endif
}

void Mappings::dump(Print &stream) {

    stream.printf_P(PSTR("Total mappings %d\n"), _mappings.size());

    for(const auto &mapping : _mappings) {
        stream.printf_P(PSTR("Path %s, mapped %s, mtime %lu\n"),  mapping.getPath(), mapping.getMappedPath(), (ulong)mapping.getModificatonTime());
    }

}

void Mappings::_storeMappings(bool sort) {

    // _loadMappings(true);
    char buf[strlen_P(_mappings_file) + 1];
    strcpy_P(buf, _mappings_file);

    File file = SPIFFS.open(buf, "w");
    if (file) {

        FS_MAPPINGS_COUNTER_TYPE items = _mappings.size();
        mapping_t header;
        uint16_t pathOffset;
        uint16_t mappedOffset;

#if FS_MAPPING_SORTED
        if (sort) {
            std::sort(_mappings.begin(), _mappings.end(), [](const FSMapping &a, const FSMapping &b) {
                return strcmp(a.getPath(), b.getPath()) > 0;
            });
        }
#endif

        pathOffset = 0;

        // item count
        file.write((uint8_t *)&items, sizeof(items));

        // headers
        for(const auto &mapping : _mappings) {
            mappedOffset = pathOffset + strlen(mapping.getPath()) + 1;
            header.path_offset = pathOffset;
            header.mapped_path_offset = mappedOffset;
            header.flags = mapping.isGzipped() ? FLAGS_GZIPPED : 0;
            header.mtime = mapping.getModificatonTime();
            header.file_size = mapping.getFileSize();
            memcpy(header.hash, mapping.getHash(), mapping.getHashSize());
            file.write((uint8_t *)&header, sizeof(header));
            pathOffset = mappedOffset + strlen(mapping.getMappedPath()) + 1;
        }

        // files
        for(const auto &mapping : _mappings) {
            file.write((uint8_t *)mapping.getPath(), strlen(mapping.getPath()) + 1);
            file.write((uint8_t *)mapping.getMappedPath(), strlen(mapping.getMappedPath()) + 1);
        }

        file.close();

        _freeMappings();
    }

}

void Mappings::_freeMappings() {
    _mappings.clear();
    if (_data) {
        free(_data);
        _data = nullptr;
    }
}

const File SPIFFSWrapper::open(Dir dir, const char *mode) {
    FSMapping *mapping;
    if ((mapping = Mappings::getInstance().find(dir.fileName())) != nullptr) {
        return mapping->open(mode);
    }
    return dir.openFile(mode);
}

const File SPIFFSWrapper::open(const char *path, const char *mode) {
    FSMapping *mapping;
    if ((mapping = Mappings::getInstance().find(path)) != nullptr) {
        return mapping->open(mode);
    }
    return SPIFFS.open(path, mode);
}

bool SPIFFSWrapper::exists(const char *path) {
    Mappings &mappings = Mappings::getInstance();
    if (mappings.find(path)) {
        return true;
    }
    return SPIFFS.exists(path);
}

bool SPIFFSWrapper::rename(const char* pathFrom, const char* pathTo) {
    Mappings &mappings = Mappings::getInstance();
    if (mappings.find(pathFrom)) {
        return mappings.rename(pathFrom, pathTo);
    }
    return SPIFFS.rename(pathFrom, pathTo);
}

bool SPIFFSWrapper::remove(const char *path) {
    Mappings &mappings = Mappings::getInstance();
    if (mappings.find(path) != nullptr) {
        return mappings.remove(path);
    }
    return SPIFFS.remove(path);
}

Dir SPIFFSWrapper::openDir(const char *path) {
    return SPIFFS_openDir(path);
}
