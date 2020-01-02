/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>

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
#include "fs_mapping.h"

PROGMEM_STRING_DEF(fs_mapping_file, "/webui/.mappings");

Mappings Mappings::_instance;

uint32_t FSMapping::getFileSize() const
{
    return _fileSize;
}

String FSMapping::getHashString() const
{
    String hashStr;
    if (_hash) {
        char buf[3];
        auto ptr = _hash;
        auto count = getHashSize();
        while(count--) {
            snprintf_P(buf, sizeof(buf), PSTR("%02x"), *ptr++ & 0xff);
            hashStr += buf;
        }
    }
    return hashStr;
}

bool FSMapping::setHashFromHexStr(const char *hash)
{
    if (strlen(hash) == getHashSize() * 2) {
        if (_hash) {
            free(_hash);
        }
        auto count = getHashSize();
        if (nullptr == (_hash = (uint8_t *)malloc(getHashSize()))) {
            return false;
        }
        auto dst = _hash;
        auto src = hash;
        char buf[3];
        buf[2] = 0;
        while(count--) {
            buf[0] = *src++;
            buf[1] = *src++;
            *dst++ = (uint8_t)strtoul(buf, nullptr, 16);
        }
        return true;
    }
    return false;
}

bool FSMapping::setHash(uint8_t *hash)
{
    if (!hash) {
        if (_hash) {
            free(_hash);
            _hash = nullptr;
        }
        return true;
    }
    if (!_hash && nullptr == (_hash = (uint8_t *)malloc(getHashSize()))) {
        return false;
    }
    memcpy(_hash, hash, getHashSize());
    return true;
}

bool Mappings::rename(const char* pathFrom, const char* pathTo)
{
    // spiffs_t fs;
    // // SPIFFS_LOCK(&fs);
    if (loadHashes()) {
        for(auto iterator = _mappings.begin(); iterator != _mappings.end(); ++iterator) {
            if (strcmp(pathFrom, iterator->getPath()) == 0) {
                char *mappedPath = strdup(iterator->getMappedPath());
                if (mappedPath) {
                    File file = SPIFFS.open(mappedPath, fs::FileOpenMode::read);
                    _mappings.emplace_back(FSMapping(pathTo, mappedPath, time(nullptr), file.size()));
                    file.close();
                    _mappings.back().setHash(iterator->getHash());
                    _mappings.erase(iterator);
                    debug_printf_P(PSTR("Renamed %s to %s (%s, %s)"), iterator->getPath(), _mappings.back().getPath(), _mappings.back().getMappedPath(), _mappings.back().getHashString().c_str());
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

const File FSMapping::open(char const *mode) const
{
    return SPIFFS.open(getMappedPath(), mode);
}

bool Mappings::remove(const char *path)
{
    bool result = false;
    // spiffs_t fs;
    // SPIFFS_LOCK(&fs);
    if (loadHashes()) {
        for(auto iterator = _mappings.begin(); iterator != _mappings.end(); ++iterator) {
            if (!strcmp(path, iterator->getPath())) {
                SPIFFS.remove(iterator->getMappedPath());
                _mappings.erase(iterator);
                break;
            }
        }
        _storeMappings();
    } else {
        debug_println(F("Failed to load hashes"));
    }
    // SPIFFS_UNLOCK(&fs);
    return result;
}

FSMapping *Mappings::find(const char *path) const
{
    auto iterator = get(path);
    if (iterator != _mappings.end()) {
        return (FSMapping *)&(*iterator);
    }
    return nullptr;
}

#if FS_MAPPING_SORTED

FileMappingsListIterator Mappings::get(const char *path) const
{
    // debug_printf_P(PSTR("get(%s)\n"), path);
    auto result = std::lower_bound(_mappings.begin(), _mappings.end(), path, [](const FSMapping &mapping, const char *path) {
        return strcmp(path, mapping.getPath()) > 0;
    });
    if (result != _mappings.end() && strcmp(path, result->getPath()) == 0) {
        return result;
    }
    return _mappings.end();
}

#else

FileMappingsListIterator Mappings::get(const char *path) const {
    // debug_printf_P(PSTR("get(%s)\n"), path);
    for(auto iterator = _mappings.begin(); iterator != _mappings.end(); ++iterator) {
        if (iterator->isPath(mappedPath)) {
            return iterator;
        }
    }
    return _mappings.end();
}

#endif

FileMappingsListIterator Mappings::getByMappedPath(const char *mappedPath) const
{
    // debug_printf_P(PSTR("getByMappedPath(%s)\n"), mappedPath);
    for(auto iterator = _mappings.begin(); iterator != _mappings.end(); ++iterator) {
        if (iterator->isMapped(mappedPath)) {
            return iterator;
        }
    }
    return _mappings.end();
}


Mappings &Mappings::getInstance()
{
    if (Mappings::_instance.empty()) {
        Mappings::_instance._loadMappings();
    }
    return Mappings::_instance;
}


Mappings::Mappings()
{
    _data = nullptr;
}

Mappings::~Mappings()
{
    _freeMappings();
}


#include <push_pack.h>

typedef struct __attribute__packed__ mapping_t {
    uint16_t path_offset;
    uint16_t mapped_path_offset;
    uint8_t flags;
    uint32_t mtime;
    uint32_t file_size;
    uint8_t hash[FS_MAPPINGS_HASH_LENGTH];
} mapping_t;

#include <pop_pack.h>

bool Mappings::loadHashes()
{
    File file = SPIFFS.open(FSPGM(fs_mapping_file), fs::FileOpenMode::read);
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

void Mappings::_loadMappings(bool extended)
{
    _freeMappings();

    File file = SPIFFS.open(FSPGM(fs_mapping_file), fs::FileOpenMode::read);
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
                                    _mappings.emplace_back(FSMapping((const char *)&_data[header.path_offset], (const char *)&_data[header.mapped_path_offset], header.mtime, header.file_size));
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

void Mappings::dump(Print &output)
{
    output.printf_P(PSTR("Total mappings %d\n"), _mappings.size());

    for(const auto &mapping : _mappings) {
        output.printf_P(PSTR("Path %s, mapped %s, mtime %u\n"), mapping.getPath(), mapping.getMappedPath(), (uint32_t)mapping.getModificatonTime());
    }
}

void Mappings::_storeMappings(bool sort)
{
    // _loadMappings(true);
    File file = SPIFFS.open(FSPGM(fs_mapping_file), fs::FileOpenMode::write);
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

void Mappings::_freeMappings()
{
    _mappings.clear();
    if (_data) {
        free(_data);
        _data = nullptr;
    }
}

const File SPIFFSWrapper::open(Dir dir, const char *mode)
{
    FSMapping *mapping;
    if ((mapping = Mappings::getInstance().find(dir.fileName())) != nullptr) {
        return mapping->open(mode);
    }
    return dir.openFile(mode);
}

const File SPIFFSWrapper::open(const char *path, const char *mode)
{
    FSMapping *mapping;
    if ((mapping = Mappings::getInstance().find(path)) != nullptr) {
        return mapping->open(mode);
    }
    return SPIFFS.open(path, mode);
}

bool SPIFFSWrapper::exists(const char *path)
{
    Mappings &mappings = Mappings::getInstance();
    if (mappings.find(path)) {
        return true;
    }
    return SPIFFS.exists(path);
}

bool SPIFFSWrapper::rename(const char* pathFrom, const char* pathTo)
{
    Mappings &mappings = Mappings::getInstance();
    if (mappings.find(pathFrom)) {
        return mappings.rename(pathFrom, pathTo);
    }
    return SPIFFS.rename(pathFrom, pathTo);
}

bool SPIFFSWrapper::remove(const char *path)
{
    Mappings &mappings = Mappings::getInstance();
    if (mappings.find(path) != nullptr) {
        return mappings.remove(path);
    }
    return SPIFFS.remove(path);
}

Dir SPIFFSWrapper::openDir(const char *path)
{
    return SPIFFS_openDir(path);
}
