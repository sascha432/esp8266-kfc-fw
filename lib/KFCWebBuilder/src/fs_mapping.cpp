/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <EventTimer.h>

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

#if DEBUG_FS_MAPPING
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(fs_mapping_file, "/webui/.mappings");

Mappings Mappings::_instance;

Mappings &Mappings::getInstance()
{
    // _debug_printf_P(PSTR("Mappings::getInstance(): size=%u\n"), _instance._count);
    if (_instance.empty()) {
        _instance._loadMappings();
    }
    return _instance;
}


Mappings::Mappings() : _entries(nullptr), _releaseTimer(nullptr)
{
#if !FS_MAPPING_READ_ONLY
    _hashes = nullptr;
#endif
}

Mappings::~Mappings()
{
    _freeMappings();
}

int Mappings::findEntry(const char *path) const
{
    for(FS_MAPPINGS_COUNTER_TYPE i = 0; i < _count; i++) {
        if (!strcmp(path, getPath(&_entries[i]))) {
            return i;
        }
    }
    return -1;
}

int Mappings::findMappedEntry(const char *mappedPath) const
{
    for(FS_MAPPINGS_COUNTER_TYPE i = 0; i < _count; i++) {
        if (!strcmp(mappedPath, getMappedPath(&_entries[i]))) {
            return i;
        }
    }
    return -1;
}

const FSMappingEntry *Mappings::getEntry(int num) const
{
    if (validEntry(num)) {
        return &_entries[num];
    }
    return nullptr;
}

const char *Mappings::getMappedPath(int num) const
{
    if (validEntry(num)) {
        return getMappedPath(&_entries[num]);
    }
    return nullptr;
}

const char *Mappings::getPath(int num) const
{
    if (validEntry(num)) {
        return getPath(&_entries[num]);
    }
    return nullptr;
}

const File Mappings::openFile(int num, char const *mode) const
{
    if (validEntry(num)) {
        return openFile(&_entries[num], mode);
    }
    return File();
}

const File Mappings::openFile(const FSMappingEntry *mapping, const char *mode) const
{
    if (mapping) {
        return SPIFFS.open(getMappedPath(mapping), mode);
    }
    return File();
}

#if !FS_MAPPING_READ_ONLY

bool Mappings::rename(int num, const char* pathTo)
{
    if (validEntry(num)) {
        // spiffs_t fs;
        // // SPIFFS_LOCK(&fs);
        if (loadHashes()) {
            FSMappingEntry &renameEntry = _entries[num];

            // update record
            renameEntry.modificationTime = time(nullptr);
            renameEntry.pathOffset = _data.length();
            _data.write(pathTo, strlen(pathTo)); // append new path to end of data block

            _storeMappings(true);   // sort after rename
            // SPIFFS_UNLOCK(&fs);
            return true;

        } else {
            _debug_println(F("Failed to load hashes"));
        }
        // SPIFFS_UNLOCK(&fs);
    }
    return false;
}

bool Mappings::remove(int num)
{
    bool result = false;
    if (validEntry(num)) {
        // spiffs_t fs;
        // SPIFFS_LOCK(&fs);
        if (loadHashes()) {
            FSMappingEntry &deleteEntry = _entries[num];
            result = SPIFFS.remove(getMappedPath(&deleteEntry));

            _count--;
            memmove(&_entries[num], &_entries[num + 1], (_count - num) * sizeof(*_entries));
            memmove(&_hashes[num], &_hashes[num + 1], (_count - num) * sizeof(*_hashes));

            _storeMappings(false); // we do not need to sort after removing a file
        } else {
            _debug_println(F("Failed to load hashes"));
        }
        // SPIFFS_UNLOCK(&fs);
    }
    return result;
}

bool Mappings::loadHashes()
{
    if (!_count) {
        return false;
    }
    File file = SPIFFS.open(FSPGM(fs_mapping_file), fs::FileOpenMode::read);
    if (file) {
        FS_MAPPINGS_COUNTER_TYPE items;
        if (file.read((uint8_t *)&items, sizeof(items)) == sizeof(items)) {
            if (_count != items) { // check if we got a match otherwise not all records have been loaded
                if (_hashes) {
                    free(_hashes);
                }
                _hashes = (FSMappingHash *)malloc(items * sizeof(FSMappingHash));
                if (_hashes) {
                    mapping_t header;
                    FS_MAPPINGS_COUNTER_TYPE pos = 0;
                    while(pos < items) {
                        if (file.read((uint8_t *)&header, sizeof(header)) != sizeof(header)) {
                            return false;
                        }
                        memcpy(_hashes[pos++].hash, header.hash, sizeof(_hashes->hash));
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

#endif


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
            if (_data.reserve(data_size)) {
                if (file.seek(index_size + sizeof(items), SeekSet)) {
                    if (file.read(_data.get(), data_size) == data_size) {
                        _data.setLength(data_size);
                        if (file.seek(sizeof(items), SeekSet)) {
                            _entries = (FSMappingEntry *)malloc(sizeof(FSMappingEntry) * items);
                            if (_entries) {
#if !FS_MAPPING_READ_ONLY
                                if (extended) {
                                    _hashes = (FSMappingHash *)malloc(sizeof(FSMappingHash) * items);
                                }
                                if (!extended || _hashes) {
#endif
                                    // auto count = items;
                                    _debug_printf_P(PSTR("Mappings::_loadMappings(): count=%u, data=%u, entries=%u, hashes=%u\n"),
                                        items,
                                        data_size,
                                        sizeof(FSMappingEntry) * items,
                                        extended ? (sizeof(FSMappingHash) * items) : 0
                                    );
                                    for(_count = 0; _count < items; _count++) {
                                        mapping_t header;
                                        if (file.read((uint8_t *)&header, sizeof(header)) == sizeof(header)) {

                                            _entries[_count].pathOffset = header.path_offset;
                                            _entries[_count].mappedPathOffset = header.mapped_path_offset;
                                            _entries[_count].fileSize = header.file_size;
                                            _entries[_count].gzipped = header.flags & FS_MAPPINGS_FLAGS_GZIPPED;

#if !FS_MAPPING_READ_ONLY

                                            if (extended) {
                                                memcpy(_hashes[_count].hash, header.hash, sizeof(_hashes->hash));
                                            }
#endif
                                        } else {
                                            _debug_printf_P(PSTR("Cannot read index %d/%d"), _count, items);
                                        }
                                    }
#if !FS_MAPPING_READ_ONLY

                                }
                                else {
                                    _debug_printf_P(PSTR("Cannot allocate hashes %d\n"), items);
                                }
#endif
                            }
                            else {
                                _debug_printf_P(PSTR("Cannot allocate entries %d\n"), items);
                            }
                        }
                    }
                    else {
                        _debug_printf_P(PSTR("Cannot read mapping data %d\n"), data_size);
                    }
                }
                else {
                    _debug_printf_P(PSTR("Seek failed offset %d\n"), index_size);
                }
            }
            else {
                _debug_printf_P(PSTR("Cannot allocate mapping buffer %d\n"), data_size);
            }
        }
        else {
            _debug_println(F("Cannot read mapping item count"));
        }
    }

    _debug_printf_P(PSTR("Mappings::_loadMappings(): mappings loaded=%u\n"), _count);

    Scheduler.removeTimer(&_releaseTimer);
    Scheduler.addTimer(&_releaseTimer, 5000, false, [this](EventScheduler::TimerPtr) {
        _debug_printf_P(PSTR("FSMapping auto release\n"));
        _freeMappings();
    });

// #if DEBUG
//     dump(&DEBUG_OUTPUT);
// #endif
}

void Mappings::dump(Print &output)
{
    output.printf_P(PSTR("Total mappings %d\n"), _count);
    for(uint8_t i = 0; i < _count; i++) {
        auto &entry = _entries[i];
        output.printf_P(PSTR("Path %s, mapped %s, mtime %u, size %u\n"), getPath(&entry), getMappedPath(&entry), (uint32_t)entry.modificationTime, (uint32_t)entry.fileSize);
    }
}

#if !FS_MAPPING_READ_ONLY

void Mappings::_storeMappings(bool sort)
{
     _loadMappings(true);
    File file = SPIFFS.open(FSPGM(fs_mapping_file), fs::FileOpenMode::write);
    if (file) {

        mapping_t header;
        uint16_t pathOffset = 0;
        uint16_t mappedOffset;

#if FS_MAPPING_SORTED
// TODO not supported atm
        // if (sort) {
        //     std::sort(_mappings.begin(), _mappings.end(), [](const FSMapping &a, const FSMapping &b) {
        //         return strcmp(a.getPath(), b.getPath()) > 0;
        //     });
        // }
#endif

        // item count
        file.write((uint8_t *)&_count, sizeof(_count));

        // headers
        for(FS_MAPPINGS_COUNTER_TYPE num = 0; num < _count; num++) {
            auto &entry = _entries[num];
            mappedOffset = pathOffset + strlen(getPath(&entry)) + 1;
            header.path_offset = pathOffset;
            header.mapped_path_offset = mappedOffset;
            header.flags = entry.gzipped ? FS_MAPPINGS_FLAGS_GZIPPED : 0;
            header.mtime = entry.modificationTime;
            header.file_size = entry.fileSize;
            memcpy(header.hash, _hashes[num].hash, FS_MAPPINGS_HASH_LENGTH);
            file.write((uint8_t *)&header, sizeof(header));
            pathOffset = mappedOffset + strlen(getMappedPath(&entry)) + 1;
        }

        // files
        for(FS_MAPPINGS_COUNTER_TYPE num = 0; num < _count; num++) {
            auto entry = &_entries[num];
            auto path = getPath(entry);
            auto mappedPath = getMappedPath(entry);
            file.write((uint8_t *)path, strlen(path) + 1);
            file.write((uint8_t *)mappedPath, strlen(mappedPath) + 1);
        }

        file.close();

    }
    _freeMappings();
}

#endif

void Mappings::_freeMappings()
{
    _debug_printf_P(PSTR("Mappings::_freeMappings(): count=%u\n"), _count);
    Scheduler.removeTimer(&_releaseTimer);
    _data.clear();
    if (_entries) {
        free(_entries);
        _entries = nullptr;
    }
#if !FS_MAPPING_READ_ONLY
    if (_hashes) {
        free(_hashes);
        _hashes = nullptr;
    }
#endif
    _count = 0;
}

const File SPIFFSWrapper::open(Dir dir, const char *mode)
{
    auto &map = Mappings::getInstance();
    auto num = map.findEntry(dir.fileName());
    if (map.validEntry(num)) {
        return map.openFile(num, mode);
    }
    return dir.openFile(mode);
}

const File SPIFFSWrapper::open(const char *path, const char *mode)
{
    auto &map = Mappings::getInstance();
    auto num = map.findEntry(path);
    if (map.validEntry(num)) {
        return map.openFile(num, mode);
    }
    return SPIFFS.open(path, mode);
}

bool SPIFFSWrapper::exists(const char *path)
{
    auto &map = Mappings::getInstance();
    auto num = map.findEntry(path);
    if (map.validEntry(num)) {
        return true;
    }
    return SPIFFS.exists(path);
}

bool SPIFFSWrapper::rename(const char* pathFrom, const char* pathTo)
{
#if !FS_MAPPING_READ_ONLY
    auto &map = Mappings::getInstance();
    auto num = map.findEntry(pathFrom);
    if (num != -1) {
        return map.rename(num, pathTo);
    }
#endif
    return SPIFFS.rename(pathFrom, pathTo);
}

bool SPIFFSWrapper::remove(const char *path)
{
#if !FS_MAPPING_READ_ONLY
    auto &map = Mappings::getInstance();
    auto num = map.findEntry(path);
    if (num != -1) {
        return map.remove(num);
    }
#endif
    return SPIFFS.remove(path);
}

Dir SPIFFSWrapper::openDir(const char *path)
{
    return SPIFFS_openDir(path);
}
