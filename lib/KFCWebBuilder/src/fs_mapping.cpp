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

#undef DEBUG_FS_MAPPING
#define DEBUG_FS_MAPPING 0

#if DEBUG_FS_MAPPING
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(fs_mapping_file, "/webui/.mappings");
PROGMEM_STRING_DEF(fs_mapping_dir, "/webui/");

Mappings Mappings::_instance;

Mappings &Mappings::getInstance()
{
    // _debug_printf_P(PSTR("Mappings::getInstance(): size=%u\n"), _instance._count);
    if (_instance.empty()) {
        _instance._loadMappings();
    }
    return _instance;
}


Mappings::Mappings() : _count(0), _data(nullptr), _entries(nullptr), _releaseTimeout(0)
{
}

Mappings::~Mappings()
{
    _freeMappings();
}

const FSMappingEntry *Mappings::findEntry(const char *path, compareFunc compare) const
{
    if (path) {
#if FS_MAPPING_SORTED && false
        // probably needs >1000 files to show any performance benefits compared to a loop
        auto first = begin();
        auto iterator = end();
        int count = _count;
        while(count > 0) {
            int step = count / 2;
            iterator = first + step;
            if (compare(getPath(iterator), path) < 0) {
                first = ++iterator;
                count -= step + 1;
            }
            else {
                count = step;
            }
        }
        if (iterator != end() && !compare(getPath(iterator), path)) {
            return iterator;
        }
#else

        for(FS_MAPPINGS_COUNTER_TYPE i = 0; i < _count; i++) {
            if (!compare(getPath(&_entries[i]), path)) {
                return &_entries[i];
            }
        }
#endif
    }
    return nullptr;
}

const char *Mappings::getMappedPath(const FSMappingEntry *entry, char *buf, size_t size) const
{
    if (entry) {
        snprintf_P(buf, size, PSTR(FS_MAPPINGS_FILE_FMT), SPGM(fs_mapping_dir), entry->mappedPathUID);
        // _debug_printf_P(PSTR("Mappings::getMappedPath(): UID=%04x, file=%s\n"), entry->mappedPathUID, buf);
        return buf;
    }
    return nullptr;
}

String Mappings::getMappedPath(const FSMappingEntry *entry) const
{
    String name;
    if (entry && name.reserve(FS_MAPPING_MAX_FILENAME_LEN)) {
        getMappedPath(entry, name.begin(), FS_MAPPING_MAX_FILENAME_LEN);
    }
    return name;
}

const File Mappings::openFile(const FSMappingEntry *entry, const char *mode) const
{
    if (entry) {
        char buf[FS_MAPPING_MAX_FILENAME_LEN];
        return SPIFFS.open(getMappedPath(entry, buf, sizeof(buf)), mode);
    }
    return File();
}

void Mappings::_loadMappings()
{
    _freeMappings();
    if (_readMappings()) {
        _releaseTimeout = millis();
    }
    else {
        _debug_printf_P(PSTR("Mappings::_loadMappings(): failed to load mappings\n"));
        _count = ~0; // indicates read error
        _freeMappings();
    }
}

bool Mappings::_readMappings()
{
    File file = SPIFFS.open(FSPGM(fs_mapping_file), fs::FileOpenMode::read);
    if (file) {
        FS_MAPPINGS_COUNTER_TYPE items;
        if (file.read((uint8_t *)&items, sizeof(items)) == sizeof(items)) {
            uint16_t index_size = items * sizeof(mapping_t);
            uint16_t data_size  = file.size() - index_size - sizeof(items);
            _data = (char *)malloc(data_size);
            _entries = (FSMappingEntry *)malloc(sizeof(FSMappingEntry) * items);
            if (_data && _entries && file.seek(index_size + sizeof(items), SeekSet) && file.read((uint8_t *)_data, data_size) == data_size && file.seek(sizeof(items), SeekSet)) {
                _debug_printf_P(PSTR("Mappings::_loadMappings(): count=%u, data=%u, entries=%u\n"),
                    items,
                    data_size,
                    sizeof(FSMappingEntry) * items
                );
                for(_count = 0; _count < items; _count++) {
                    mapping_t header;
                    if (file.read((uint8_t *)&header, sizeof(header)) == sizeof(header)) {
                        _entries[_count].pathOffset = header.path_offset;
                        _entries[_count].mappedPathUID = header.mapped_path_uid;
                        _entries[_count].fileSize = header.file_size;
                        _entries[_count].modificationTime = header.mtime;
                        _entries[_count].gzipped = header.flags & FS_MAPPINGS_FLAGS_GZIPPED;
                        //_debug_printf_P(PSTR("File %s, UID %04x, size %u\n"), getPath(&_entries[_count]), _entries[_count].mappedPathUID, _entries[_count].fileSize);
                    } else {
                        _debug_printf_P(PSTR("Cannot read index %d/%d"), _count, items);
                        return false;
                    }
                }
                return true;
            }
            else {
                _debug_printf_P(PSTR("Cannot read mapping buffer %d\n"), data_size);
            }
        }
        else {
            _debug_println(F("Cannot read mapping item count"));
        }
    }
    return false;
}

void Mappings::_freeMappings()
{
    if (_count) {
        _debug_printf_P(PSTR("Mappings::_freeMappings(): count=%d\n"), _count);
        if (_data) {
            free(_data);
            _data = nullptr;
        }
        if (_entries) {
            free(_entries);
            _entries = nullptr;
        }
        _count = 0;
    }
    _releaseTimeout = 0;
}

void Mappings::_gc()
{
    if (get_time_diff(_releaseTimeout, millis()) > FS_MAPPING_AUTO_RELEASE_MEMORY) {
        _debug_printf_P(PSTR("Mappings::_gc(): timeout=%u\n"), get_time_diff(_releaseTimeout, millis()));
        _freeMappings();
    }
}

void Mappings::gc()
{
    if (_instance._releaseTimeout) {
        _instance._gc();
    }
}

void Mappings::dump(Print &output)
{
    output.printf_P(PSTR("Total mappings %d in %s\n"), _count, SPGM(fs_mapping_file));
    char buf[FS_MAPPING_MAX_FILENAME_LEN];
    for(uint8_t i = 0; i < _count; i++) {
        auto &entry = _entries[i];
        output.printf_P(PSTR("Path %s, mapped %s, mtime %u, size %u, gzipped %d\n"), getPath(&entry), getMappedPath(&entry, buf, sizeof(buf)), (uint32_t)entry.modificationTime, (uint32_t)entry.fileSize, (int)entry.gzipped);
    }
}

const File SPIFFSWrapper::open(Dir dir, const char *mode)
{
    auto &map = Mappings::getInstance();
    auto entry = map.findEntry(dir.fileName());
    if (entry) {
        return map.openFile(entry, mode);
    }
    return dir.openFile(mode);
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
    auto &map = Mappings::getInstance();
    auto entry = map.findEntry(path);
    if (entry) {
        return map.openFile(entry, mode);
    }
    return SPIFFS.open(path, mode);
}

bool SPIFFSWrapper::exists(const char *path)
{
    auto &map = Mappings::getInstance();
    if (map.findEntry(path)) {
        return true;
    }
    return SPIFFS.exists(path);
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

Dir SPIFFSWrapper::openDir(const char *path)
{
    return Dir(DirImplPtr(new FSMappingDirImpl(SPIFFS, path)));
}

FSMappingDirImpl::FSMappingDirImpl(FS &fs, const String &dirName) : _fs(fs), _dirName(dirName)
{
    append_slash(_dirName);
    _debug_printf_P(PSTR("FSMappingDirImpl::FSMappingDirImpl(): dirName=%s\n"), _dirName.c_str());
#if LOGGER
    if (_dirName.equals(FSPGM(slash))) {
        _logger.getLogs(_logs);
        _logsIterator = _logs.begin();
    }
#endif
    rewind();
}

FileImplPtr FSMappingDirImpl::openFile(OpenMode openMode, AccessMode accessMode)
{
    _debug_printf_P(PSTR("FSMappingDirImpl::openFile(): %s\n"), _fileName.c_str());
    String mode;
    if (openMode == OM_APPEND) {
        mode = 'a';
    }
    else if (openMode == OM_TRUNCATE) {
        mode = 'w';
    }
    else {
        mode = 'r';
    }
    if (accessMode == AM_RW) {
        mode += '+';
    }
    return FileImplPtr(new FSMappingFileImp(SPIFFSWrapper::open(_fileName, mode.c_str())));
}

size_t FSMappingDirImpl::fileSize()
{
    if (_isValid == FILE) {
        if (_iterator != _end && Mappings::endIterator() == _end) {
            return _iterator->fileSize;
        }
        return SPIFFSWrapper::open(_fileName, fs::FileOpenMode::read).size();
    }
    return 0;
}

bool FSMappingDirImpl::rewind()
{
    _debug_println(F("FSMappingDirImpl::rewind()"));
    _dirs.clear();
    _dirs.push_back(_dirName);
    _dir = _fs.openDir(FSPGM(slash));
    _isValid = _dir.next() ? FIRST_DIR : INVALID;
    auto &map = Mappings::getInstance();
    _iterator = map.begin();
    _end = map.end();
    return _isValid != INVALID;
}


bool FSMappingDirImpl::next()
{
    if (_isValid == INVALID) {
        return false;
    }

    if (_end != Mappings::endIterator()) { // mappings were released or have been modified and all iterators have become invalid
        _isValid = INVALID;
        return false;
    }

    auto &map = Mappings::getInstance();
    if (_iterator != _end) { // go through virtual files first
        while(_iterator != _end) {
            String filename = _virtualRoot + map.getPath(_iterator);
            ++_iterator;
            if (_validate(filename)) {
                _debug_printf(PSTR("next() = %s %d V%c\n"), _fileName.c_str(), true, isFile() ? 'F' : 'D');
                return true;
            }
        }
        _isValid = FIRST_DIR; // reset to intial state once done
    }

    // check _dir
    bool isValid;
    bool result = false;
    do {
        if (_isValid == FIRST_DIR) { // first next() call, we have the result already
            _isValid = INVALID;
            result = true;
        }
        else {
            result = _dir.next();
            if (!result) {
                break;
            }
        }
        isValid = _validate(_dir.fileName());
        if (isValid) { // check if this file is mapped and hide it
            char buf[FS_MAPPING_MAX_FILENAME_LEN];
            for (auto it = map.begin(); it != _end; ++it) {
                if (_dir.fileName().equals(map.getMappedPath(it, buf, sizeof(buf)))) {
                    isValid = false;
                }
            }
        }
    } while (!isValid);

#if LOGGER
    if (!result) {
        _isValid = INVALID;
        if(_logsIterator != _logs.end()) {
            result = true;
            _fileName = _dirName + *_logsIterator;
            _isValid = FILE;
            ++_logsIterator;
        }
    }
#endif

    _debug_printf(PSTR("next() = %s %d %c\n"), _fileName.c_str(), result, isFile() ? 'F' : 'D');
    return result;
}

bool FSMappingDirImpl::_validate(const String &path)
{
    _debug_printf_P(PSTR("FSMappingDirImpl::_validate(%s)\n"), path.c_str());
    if (!path.startsWith(_dirName)) { // not a match
        _isValid = INVALID;
        _debug_printf_P(PSTR("_validate(%s): startWith()=false\n"), path.c_str());
        return false;
    }
    int pos = _dirName.length(); // points to the first character of the file or subdirectory
    int pos2 = path.indexOf('/', pos + 1);
    if (pos2 != -1) { // sub directory pos - pos2
        String fullpath = path.substring(0, pos2);
        if (std::find(_dirs.begin(), _dirs.end(), fullpath) != _dirs.end()) { // do we have it already?
            return false;
        }
        _dirs.push_back(fullpath);
        _isValid = DIR;
        _fileName = fullpath;
        _debug_printf_P(PSTR("_validate(%s): DIR: %s\n"), path.c_str(), _fileName.c_str());

    }
    else if (pos2 == -1) { // filename or "."
        auto name = path.c_str() + pos;
        if (*name == '.' && *(name + 1) == 0) {
            return false;
        }
        _isValid = FILE;
        _fileName = path;
        _debug_printf_P(PSTR("_validate(%s): FILE: %s\n"), path.c_str(), _fileName.c_str());
    }
    else {
        _isValid = INVALID;
        debug_printf_P(PSTR("_validate(%s): INVALID\n"), path.c_str());
        return false;
    }
    return true;
}
