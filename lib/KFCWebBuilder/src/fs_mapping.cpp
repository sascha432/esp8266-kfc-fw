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
PROGMEM_STRING_DEF(fs_mapping_listings, "/webui/.listings");

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
