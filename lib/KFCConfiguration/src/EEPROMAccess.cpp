/**
* Author: sascha_lammers@gmx.de
*/

#include <DumpBinary.h>
#include "EEPROMAccess.h"
#if !_MSC_VER
#include <alloca.h>
#endif

#if DEBUG_CONFIGURATION && DEBUG_EEPROM_ENABLE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace ConfigurationHelper;

void EEPROMAccess::begin(uint16_t size)
{
    if (size) {
        if (size > _size && _isInitialized) {
            __LDBG_printf("resize=%u size=%u", size, _size);
            end();
        }
        _size = size;
    }
    if (!_isInitialized) {
        _isInitialized = true;
        __LDBG_printf("size=%u", _size);
        EEPROM.begin(_size);
        __DBG__addFlashReadSize(0, _size);
    }
}

void EEPROMAccess::discard()
{
    if (_isInitialized) {
        static_cast<EEPROMClassEx &>(EEPROM).clearAndEnd();
        _isInitialized = false;
    }
}

void EEPROMAccess::end()
{
    if (_isInitialized) {
        __LDBG_printf("size=%u", _size);
#if DEBUG_CONFIGURATION_GETHANDLE
        bool dirty = static_cast<EEPROMClassEx &>(EEPROM).getDirty();
        auto writeSize = static_cast<EEPROMClassEx &>(EEPROM).getWriteSize();
        bool result = false;
        if (writeSize && (result = EEPROM.commit())) {
            __DBG_printf("EEPROM CHANGED EEPROM CHANGED EEPROM CHANGED EEPROM CHANGED EEPROM CHANGED");
            __DBG__addFlashWriteSize(0, writeSize);
        }
        __DBG_printf("end write_size=%u commit=%u dirty_before/after=%u/%u", writeSize, result, dirty, static_cast<EEPROMClassEx &>(EEPROM).getDirty());
#endif
        EEPROM.end();
        _isInitialized = false;
    }
}

void EEPROMAccess::commit()
{
    if (_isInitialized) {
        __LDBG_printf("size=%u", _size);
#if DEBUG_CONFIGURATION_GETHANDLE
        bool dirty = static_cast<EEPROMClassEx &>(EEPROM).getDirty();
        auto writeSize = static_cast<EEPROMClassEx &>(EEPROM).getWriteSize();
        bool result;
        if ((result = EEPROM.commit())) {
            __DBG_printf("EEPROM CHANGED EEPROM CHANGED EEPROM CHANGED EEPROM CHANGED EEPROM CHANGED");
            __DBG__addFlashWriteSize(0, writeSize);
        }
        __DBG_printf("commit write_size=%u commit=%u dirty_before/after=%u/%u", writeSize, result, dirty, static_cast<EEPROMClassEx &>(EEPROM).getDirty());
#else
        EEPROM.commit();
#endif

        _isInitialized = false;
    }
    else {
        __DBG_panic("EEPROM is not initialized");
    }
}

uint16_t EEPROMAccess::getReadSize(uint16_t offset, uint16_t length) const
{
    if (_isInitialized || length < kTempReadBufferSize - 8) {
        return length;
    }
#if defined(ESP8266)
    uint8_t alignment = ((SECTION_EEPROM_START_ADDRESS - SECTION_FLASH_START_ADDRESS) + offset) & 3;
    return (length + alignment + 3) & ~3; // align read length and add alignment
#else
    return size;
#endif
}

//uint16_t EEPROMAccess::read_with_temporary_on_stack(const uint32_t start_address, const uint16_t readSize, uint8_t *dst, const uint16_t size, const uint16_t maxSize, const uint8_t alignment)
//{
//    uint8_t temp[readSize];
//    return read_with_temporary(start_address, temp, readSize, dst, size, maxSize, alignment);
//}

uint16_t EEPROMAccess::read_with_temporary(const uint32_t start_address, uint8_t *temp, const uint16_t readSize, uint8_t *dst, const uint16_t size, const uint16_t maxSize, const uint8_t alignment)
{
    noInterrupts();
    SpiFlashOpResult result = spi_flash_read(start_address, reinterpret_cast<uint32_t *>(temp), readSize);
    interrupts();

    __LDBG_assert_printf(result == SPI_FLASH_RESULT_OK, "spi_flash_read failed src=%08x dst=%08x size=%u", start_address, temp, readSize);
    __LDBG_printf("spi_flash_read(%08x, %d) = %d, alignment %u", start_address, readSize, result, alignment);
    //__DBG_IF_flashUsage(flashReadSize = readSize);

    if (result == SPI_FLASH_RESULT_OK) {
        const auto src = &temp[alignment];
        uint16_t fill = maxSize - size;
        if (dst != src) {
            // copy/move and fill
            std::fill_n(std::copy_n(src, size, dst), fill, 0);
        }
        else if (fill) {
            // just fill
            std::fill_n(dst + size, fill, 0);
        }
        return size;
    }

    std::fill_n(dst, maxSize, 0);
    return 0;
}

uint16_t EEPROMAccess::read(uint8_t *dst, uint16_t offset, uint16_t size, uint16_t maxSize) // __DBG_IF_flashUsage(, size_t &flashReadSize))
{
    //__DBG_IF_flashUsage(flashReadSize = 0);

#if defined(ESP8266)
    // if the EEPROM is not intialized, copy data from flash directly
    if (_isInitialized) {
        // memcpy(dst, EEPROM.getConstDataPtr() + offset, length); // data is already in RAM
        __LDBG_assert_printf(dst + size <= dst + maxSize, "dst + size <= dst + maxSize");
        std::copy_n(EEPROM.getConstDataPtr() + offset, size, dst);
        return size;
    }

    auto eeprom_start_address = (SECTION_EEPROM_START_ADDRESS - SECTION_FLASH_START_ADDRESS) + offset;

    uint8_t alignment = eeprom_start_address & 3;
    uint16_t readSize = (size + alignment + 3) & ~3; // align read length and add alignment
    eeprom_start_address -= alignment; // align start address

    __LDBG_printf("dst=%p ofs=%d len=%d align=%u read_size=%u", dst, offset, size, alignment, readSize);
    __LDBG_assert_printf(size <= maxSize, "length=%u > size=%u", size, maxSize);

    // __LDBG_printf("flash: %08X:%08X (%d) aligned: %08X:%08X (%d)",
    //     eeprom_start_address + alignment, eeprom_start_address + length + alignment, length,
    //     eeprom_start_address, eeprom_start_address + readSize, readSize
    // );

    uint16_t result;
    if (readSize <= maxSize) {
        result = read_with_temporary(eeprom_start_address, dst, readSize, dst, size, maxSize, alignment);
    }
    else if (readSize <= kTempReadBufferSize) {
#if _MSC_VER
        uint8_t *temp = reinterpret_cast<uint8_t *>(alloca(readSize));
#elif defined(ESP8266) || defined(ESP32)
        uint8_t temp[readSize];
#else
        uint8_t *temp = reinterpret_cast<uint8_t *>(alloca(readSize));
#endif
        result = read_with_temporary(eeprom_start_address, temp, readSize, dst, size, maxSize, alignment);
    }
    else {
        auto temp = __LDBG_new_array(readSize, uint8_t);
        // large read operation should have an aligned address or enough extra space to avoid memory allocations
        __LDBG_assert_printf(false, "allocating read buffer read_size=%d len=%d size=%u ptr=%p", readSize, size, maxSize, temp);
        if (temp) {
            result = read_with_temporary(eeprom_start_address, temp, readSize, dst, size, maxSize, alignment);
            __LDBG_delete_array(temp);
        }
        else {
            result = 0;
        }
    }

#if DEBUG_CONFIGURATION_VERIFY_DIRECT_EEPROM_READ
    auto hasBeenInitialized = _isInitialized;
    begin();
    auto cmpResult = memcmp(dst, getConstDataPtr() + offset, size);
    if (cmpResult) {
        __DBG_panic("res=%d dst=%p ofs=%d size=%u len=%d align=%u read_size=%u memcpy() failed=%d", result, dst, offset, size, size, alignment, readSize, cmpResult);
    }
    // for (uint16_t i = length; i < size; i++) {
    //     if (dst[i] != 0) {
    //         __DBG_panic("res=%d dst=%p ofs=%d size=%u len=%d align=%u read_size=%u not zero @dst[%u]", result, dst, offset, size, length, alignment, readSize, i);
    //     }
    // }

    if (!hasBeenInitialized) {
        discard();
    }
#endif


#elif defined(ESP32)

    begin();
    memcpy(dst, getConstDataPtr() + offset, size);

#elif defined(ESP32) && false

    // TODO the EEPROM class is not using partitions anymore but NVS blobs

    if (_eepromInitialized) {
        if (!EEPROM.readBytes(offset, dst, size)) {
            memset(dst, 0, size);
        }
    }
    else {
        if (!_partition) {
            _partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, SPGM(EEPROM_partition_name));
        }
        __LDBG_printf("ofs= %d, len=%d: using esp_partition_read(%p)", offset, size, _partition);
        if (esp_partition_read(_partition, offset, (void *)dst, size) != ESP_OK) {
            memset(dst, 0, size);
        }
    }

#else

    begin();
    memcpy(dst, getConstDataPtr() + offset, size);

#endif

    return result;
}

void EEPROMAccess::dump(Print &output, bool asByteArray, uint16_t offset, uint16_t length)
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
#if ESP32
        dumper.dump(EEPROM.getDataPtr() + offset, size);
#else
        dumper.dump(EEPROM.getConstDataPtr() + offset, length);
#endif
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
