/**
* Author: sascha_lammers@gmx.de
*/

#include "Configuration.h"

#if _WIN32
#define ESP8266 1
#define ARDUINO_ESP8266_RELEASE_2_6_3 1
#endif

#define DEBUG_POOL_ENABLE              0
#define DEBUG_EEPROM_ENABLE            0

#if DEBUG_CONFIGURATION && DEBUG_POOL_ENABLE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// #if defined(ESP32)
// PROGMEM_STRING_DEF(EEPROM_partition_name, "eeprom");
// #endif

#if defined(ESP8266)
extern "C" {
#include "spi_flash.h"
}

#if defined(ARDUINO_ESP8266_RELEASE_2_6_3)
extern "C" uint32_t _EEPROM_start;
#elif defined(ARDUINO_ESP8266_RELEASE_2_5_2)
extern "C" uint32_t _SPIFFS_end;
#define _EEPROM_start _SPIFFS_end
#else
#error Check if eeprom start is correct
#endif


#ifdef NO_GLOBAL_EEPROM
// allows to use a different sector in flash memory
#ifndef EEPROM_ADDR
// #define EEPROM_ADDR 0x40200000           // default
#define EEPROM_ADDR 0x40201000           // 1MB/4MB flash size
// #define EEPROM_ADDR 0x40202000           // 4MB
// #define EEPROM_ADDR 0x40203000           // 4MB
#endif
EEPROMClass EEPROM((((uint32_t)&_EEPROM_start - EEPROM_ADDR) / SPI_FLASH_SEC_SIZE));
#else
#define EEPROM_ADDR 0x40200000           // sector of the configuration for direct access
#endif
#endif

ConfigurationHelper::Pool::Pool(uint16_t size) : _ptr(nullptr), _length(0), _size((size + 0xf) & ~0xf), _count(0)
{
}

ConfigurationHelper::Pool::~Pool()
{
    if (_ptr) {
        free(_ptr);
    }
}

ConfigurationHelper::Pool::Pool(Pool &&pool) noexcept
{
    *this = std::move(pool);
}

ConfigurationHelper::Pool &ConfigurationHelper::Pool::operator=(Pool &&pool) noexcept
{
    _ptr = pool._ptr;
    _length = pool._length;
    _size = pool._size;
    _count = pool._count;
    pool._ptr = nullptr;
    return *this;
}


void ConfigurationHelper::Pool::init()
{
    _ptr = (uint8_t *)calloc(_size, 1);
}

bool ConfigurationHelper::Pool::space(uint16_t length) const
{
    return align(length) <= (_size - _length);
}

uint16_t ConfigurationHelper::Pool::available() const
{
    return _size - _length;
}

uint16_t ConfigurationHelper::Pool::size() const
{
    return _size;
}

uint8_t ConfigurationHelper::Pool::count() const
{
    return _count;
}

uint8_t *ConfigurationHelper::Pool::allocate(uint16_t length)
{
    length = align(length);
    auto endPtr = _ptr + _length;
    _length += length;
    _count++;
    return endPtr;
}

uint16_t ConfigurationHelper::Pool::align(uint16_t length)
{
    return (length + 4) & ~3;
}

void ConfigurationHelper::Pool::release(const void *ptr)
{
#if DEBUG_CONFIGURATION
    if (!hasPtr(ptr)) {
        __debugbreak_and_panic_printf_P(PSTR("ptr=%p not in pool\n"), ptr);
    }
#endif
    _count--;
    if (_count == 0) {
        _length = 0;
    }
}

bool ConfigurationHelper::Pool::hasPtr(const void *ptr) const
{
    return (ptr >= _ptr && ptr <= _ptr + _length);
}

void *ConfigurationHelper::Pool::getPtr() const
{
    return _ptr;
}


#if DEBUG_CONFIGURATION && DEBUG_EEPROM_ENABLE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

void ConfigurationHelper::EEPROMAccess::begin(uint16_t size)
{
    if (size) {
        if (size > _size && _isInitialized) {
            _debug_printf_P(PSTR("resize=%u size=%u\n"), size, _size);
            end();
        }
        _size = size;
    }
    if (!_isInitialized) {
        _isInitialized = true;
        _debug_printf_P(PSTR("size=%u\n"), _size);
        EEPROM.begin(_size);
    }
}

void ConfigurationHelper::EEPROMAccess::end()
{
    if (_isInitialized) {
        _debug_printf_P(PSTR("size=%u\n"), _size);
        EEPROM.end();
        _isInitialized = false;
    }
}

void ConfigurationHelper::EEPROMAccess::commit()
{
    if (_isInitialized) {
        _debug_printf_P(PSTR("size=%u\n"), _size);
        EEPROM.commit();
        _isInitialized = false;
    }
    else {
        __debugbreak_and_panic_printf_P(PSTR("EEPROM is not initialized\n"));
    }
}

void ConfigurationHelper::EEPROMAccess::read(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size)
{
#if defined(ESP8266)
    // if the EEPROM is not intialized, copy data from flash directly
    if (_isInitialized) {
        memcpy(dst, EEPROM.getConstDataPtr() + offset, length); // data is already in RAM
        return;
    }

    auto eeprom_start_address = ((uint32_t)&_EEPROM_start - EEPROM_ADDR) + offset;

    uint8_t alignment = eeprom_start_address % 4;
    uint16_t readSize = (length + alignment + 3) & ~3; // align read length and add alignment
    eeprom_start_address -= alignment; // align start address

    _debug_printf_P(PSTR("dst=%p ofs=%d len=%d align=%u read_size=%u\n"), dst, offset, length, alignment, readSize);

    // _debug_printf_P(PSTR("flash: %08X:%08X (%d) aligned: %08X:%08X (%d)\n"),
    //     eeprom_start_address + alignment, eeprom_start_address + length + alignment, length,
    //     eeprom_start_address, eeprom_start_address + readSize, readSize
    // );

    SpiFlashOpResult result = SPI_FLASH_RESULT_ERR;
    if (readSize > size) { // does not fit into the buffer
        if (readSize <= 64) {
            uint8_t buf[64];
            noInterrupts();
            result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t *>(buf), readSize);
            interrupts();
            if (result == SPI_FLASH_RESULT_OK) {
                memcpy(dst, buf + alignment, length); // copy to destination
            }
        }
        else {
            auto ptr = reinterpret_cast<uint8_t *>(malloc(readSize));
            // large read operation should have an aligned address or enough extra space to avoid memory allocations
            _debug_printf_P(PSTR("allocating read buffer read_size=%d len=%d size=%u ptr=%p\n"), readSize, length, size, ptr);
            if (ptr) {
                noInterrupts();
                result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t *>(ptr), readSize);
                interrupts();
                if (result == SPI_FLASH_RESULT_OK) {
                    memcpy(dst, ptr + alignment, length); // copy to destination
                }
                ::free(ptr);
            }
        }
    }
    else {
        noInterrupts();
        result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t *>(dst), readSize);
        interrupts();
        if (result == SPI_FLASH_RESULT_OK && alignment) { // move to beginning of the destination
            memmove(dst, dst + alignment, length);
        }
        memset(dst + length, 0, readSize - length); // clear the rest
    }
    if (result != SPI_FLASH_RESULT_OK) {
        memset(dst, 0, length);
    }
    _debug_printf_P(PSTR("spi_flash_read(%08x, %d) = %d, offset %u\n"), eeprom_start_address, readSize, result, offset);

#if DEBUG_CONFIGURATION_VERIFY_DIRECT_EEPROM_READ
    auto hasBeenInitialized = _isInitialized;
    begin();
    auto cmpResult = memcmp(dst, getConstDataPtr() + offset, length);
    if (cmpResult) {
        __debugbreak_and_panic_printf_P(PSTR("res=%d dst=%p ofs=%d size=%u len=%d align=%u read_size=%u memcpy() failed=%d\n"), result, dst, offset, size, length, alignment, readSize, cmpResult);
    }
    // for (uint16_t i = length; i < size; i++) {
    //     if (dst[i] != 0) {
    //         __debugbreak_and_panic_printf_P(PSTR("res=%d dst=%p ofs=%d size=%u len=%d align=%u read_size=%u not zero @dst[%u]\n"), result, dst, offset, size, length, alignment, readSize, i);
    //     }
    // }

    if (!hasBeenInitialized) {
        end();
    }
#endif


#elif defined(ESP32)

    begin();
    memcpy(dst, getConstDataPtr() + offset, length);

#elif defined(ESP32) && false

    // TODO the EEPROM class is not using partitions anymore but NVS blobs

    if (_eepromInitialized) {
        if (!EEPROM.readBytes(offset, dst, length)) {
            memset(dst, 0, length);
        }
    }
    else {
        if (!_partition) {
            _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, SPGM(EEPROM_partition_name));
        }
        _debug_printf_P(PSTR("ofs= %d, len=%d: using esp_partition_read(%p)\n"), offset, length, _partition);
        if (esp_partition_read(_partition, offset, (void *)dst, length) != ESP_OK) {
            memset(dst, 0, length);
        }
    }

#else

    begin();
    memcpy(dst, getConstDataPtr() + offset, length);

#endif

#if DEBUG_CONFIGURATION && 0
    _debug_printf_P(PSTR("ofs=%u len=%u size=%u data=%s"), offset, length, size, Configuration::__debugDumper(dst, length).c_str());
#endif
}

void ConfigurationHelper::EEPROMAccess::dump(Print &output, bool asByteArray, uint16_t offset, uint16_t length)
{
    if (length == 0) {
        length = _size;
    }
    end();
    begin(offset + length);

    output.printf_P(PSTR("Dumping EEPROM %d:%d\n"), offset, length);
    if (asByteArray) {
        uint16_t pos = offset;
        while (pos < _size) {
            output.printf_P(PSTR("0x%02x, "), EEPROM.read(pos));
            if (++pos % (76 / 6) == 0) {
                output.println();
            }
            delay(1);
        }
        output.println();
        output.printf_P(PSTR("%d,\n"), _size);
    }
    else {
        DumpBinary dumper(output);
        dumper.dump(EEPROM.getConstDataPtr() + offset, length);
//#if 1
//        output.printf_P(PSTR("Dumping flash (spi_read) %d:%d\n"), offset, length);
//        endEEPROM();
//        uint16_t size = (length + 4) & ~3;
//        uint8_t *buffer = (uint8_t *)malloc(size);
//        getEEPROM(buffer, offset, length, size);
//        dumper.dump(buffer, length);
//        free(buffer);
//#endif
//#endif
    }
    end();
}
