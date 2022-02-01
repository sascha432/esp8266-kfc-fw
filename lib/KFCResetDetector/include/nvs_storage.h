/**
 * Author: sascha_lammers@gmx.de
 */

// ESP8266 stores data in the file system, appending a checksum. future implementation might store data on flash
// ESP32 stores data in nvs memory
// this class supports a single read/write operation only

#pragma once

#include <Arduino_compat.h>
#include <misc.h>
#include <memory>

#if ESP8266
#include "FileOpenMode.h"
#define NVS_STORAGE_DIRECTORY "/.nvs/"
#endif

#if ESP32
#include <nvs.h>
#define NVS_STORAGE_NAMESPACE "kfcfw_nvs"
#endif

class NVSStorage {
public:
    enum class OpenMode {
        READ = 0,
        WRITE = 1,
    };

public:
    // name is limited to 15 characters
    NVSStorage(const String &name);
    NVSStorage(const __FlashStringHelper *name);
    ~NVSStorage();

    bool open(OpenMode mode);
    void close();

    bool write(const uint8_t *data, size_t size);

    template<typename _Type>
    bool write(const _Type &data) {
        return write(reinterpret_cast<const uint8_t *>(std::addressof(data)), sizeof(data));
    }

    bool read(uint8_t *data, size_t size);

    template<typename _Type>
    bool read(_Type &data) {
        return read(reinterpret_cast<uint8_t *>(std::addressof(data)), sizeof(data));
    }

    bool isEqual(uint8_t *data, size_t size);

    template<typename _Type>
    bool isEqual(_Type &data) {
        return isEqual(reinterpret_cast<uint8_t *>(std::addressof(data)), sizeof(data));
    }

private:
    struct AutoClose {
        AutoClose(NVSStorage *storage) :
            _storage(*storage)
        {}

        ~AutoClose() {
            _storage.close();
        }

        NVSStorage &_storage;
    };

private:
    #if ESP8266
        File _file;
    #endif
    #if ESP32
        nvs_handle _handle;
    #endif
    String _name;
};

inline NVSStorage::NVSStorage(const String &name) :
    #if ESP32
        _handle(0),
    #endif
    _name(name)
{
}

inline NVSStorage::NVSStorage(const __FlashStringHelper *name) :
    #if ESP32
        _handle(0),
    #endif
    _name(name)
{
}

inline NVSStorage::~NVSStorage()
{
    close();
}

inline bool NVSStorage::open(OpenMode mode)
{
    close();
    #if ESP8266
        String filename = F(NVS_STORAGE_DIRECTORY);
        filename += _name;
        _file = createFileRecursive(filename, mode == OpenMode::WRITE ? fs::FileOpenMode::write : fs::FileOpenMode::read);
        return _file;
    #endif
    #if ESP32
        if (_handle) {
            close();
            if (nvs_open(NVS_STORAGE_NAMESPACE, mode == OpenMode::WRITE ? nvs_open_mode_t::NVS_READWRITE : nvs_open_mode_t::NVS_READONLY, &_handle) != ESP_OK) {
                _handle = 0;
                return false;
            }
        }
        return true;
    #endif
}

inline void NVSStorage::close()
{
    #if ESP8266
        if (_file) {
            _file.close();
        }
    #endif
    #if ESP32
        if (_handle) {
            nvs_close(_handle);
            _handle = 0;
        }
    #endif
}

inline bool NVSStorage::write(const uint8_t *data, size_t size)
{
    AutoClose close(this);
    #if ESP8266
        if (_file.write(data, size) != size) {
            return false;
        }
        uint32_t crc = crc32(data, size);
        auto result = _file.write(reinterpret_cast<uint8_t *>(&crc), sizeof(crc)) == sizeof(crc);
        return result;
    #endif
    #if ESP32
        if (!_handle) {
            return false;
        }
        if (nvs_set_blob(_handle, _name.c_str(), data, size) != ESP_OK) {
            return false;
        }
        return true;
    #endif
}

inline bool NVSStorage::read(uint8_t *data, size_t size)
{
    AutoClose close(this);
    #if ESP8266
        if (static_cast<size_t>(_file.read(data, size)) != size) {
            return false;
        }
        uint32_t crc;
        if (_file.read(reinterpret_cast<uint8_t *>(&crc), sizeof(crc)) != sizeof(crc)) {
            return false;
        }
        return (crc == crc32(data, size));
    #endif
    #if ESP32
        if (!_handle) {
            return false;
        }
        size_t readSize = size;
        if (nvs_get_blob(_handle, _name.c_str(), data, &readSize) != ESP_OK) {
            return false;
        }
        else if (size != readSize) {
            return false;
        }
        return true;
    #endif
}

inline bool NVSStorage::isEqual(uint8_t *data, size_t size)
{
    auto storedData = std::unique_ptr<uint8_t[]>(new uint8_t[size]);
    if (!storedData) {
        return false;
    }
    if (!read(storedData.get(), size)) {
        return false;
    }
    return memcmp(storedData.get(), data, size) == 0;
}
