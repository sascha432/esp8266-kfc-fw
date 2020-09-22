/**
* Author: sascha_lammers@gmx.de
*/

#include <DumpBinary.h>
#include "EEPROMAccess.h"

#if DEBUG_CONFIGURATION && DEBUG_EEPROM_ENABLE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if _MSC_VER
#define ESP8266 1
#include "spi_flash.h"
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
    return length;
#endif
}

uint16_t EEPROMAccess::read(uint8_t *dst, uint16_t offset, uint16_t length, uint16_t size) // __DBG_IF_flashUsage(, size_t &flashReadSize))
{
    //__DBG_IF_flashUsage(flashReadSize = 0);

#if defined(ESP8266)
    // if the EEPROM is not intialized, copy data from flash directly
    if (_isInitialized) {
        // memcpy(dst, EEPROM.getConstDataPtr() + offset, length); // data is already in RAM
        __DBG_assert(dst + length <= dst + size);
        std::copy_n(EEPROM.getConstDataPtr() + offset, length, dst);
        return length;
    }

    auto eeprom_start_address = (SECTION_EEPROM_START_ADDRESS - SECTION_FLASH_START_ADDRESS) + offset;

    uint8_t alignment = eeprom_start_address & 3;
    uint16_t readSize = (length + alignment + 3) & ~3; // align read length and add alignment
    eeprom_start_address -= alignment; // align start address

    __LDBG_printf("dst=%p ofs=%d len=%d align=%u read_size=%u", dst, offset, length, alignment, readSize);

    // __LDBG_printf("flash: %08X:%08X (%d) aligned: %08X:%08X (%d)",
    //     eeprom_start_address + alignment, eeprom_start_address + length + alignment, length,
    //     eeprom_start_address, eeprom_start_address + readSize, readSize
    // );

    SpiFlashOpResult result = SPI_FLASH_RESULT_ERR;
    if (readSize > size) { // does not fit into the buffer
        if (readSize <= kTempReadBufferSize) {
            uint8_t buf[kTempReadBufferSize];
            noInterrupts();
            result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t *>(buf), readSize);
            interrupts();
            __LDBG_assert_printf(result == SPI_FLASH_RESULT_OK, "spi_flash_read failed src=%08x dst=%08x size=%u", eeprom_start_address, buf, readSize);
            if (result == SPI_FLASH_RESULT_OK) {
                // memcpy(dst, buf + alignment, length); // copy to destination
                __DBG_assert(dst + length <= dst + size);
                std::copy_n(buf + alignment, length, dst);
            }
        }
        else {
            auto ptr = __LDBG_new_array(readSize, uint8_t);
            // large read operation should have an aligned address or enough extra space to avoid memory allocations
            __LDBG_assert_printf(false, "allocating read buffer read_size=%d len=%d size=%u ptr=%p", readSize, length, size, ptr);
            if (ptr) {
                noInterrupts();
                result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t *>(ptr), readSize);
                interrupts();
                __LDBG_assert_printf(result == SPI_FLASH_RESULT_OK, "spi_flash_read failed src=%08x dst=%08x size=%u", eeprom_start_address, ptr, readSize);
                if (result == SPI_FLASH_RESULT_OK) {
                    // memcpy(dst, ptr + alignment, length); // copy to destination
                    __DBG_assert(dst + length <= dst + size);
                    std::copy_n(ptr + alignment, length, dst);
                }
                __LDBG_delete_array(ptr);
            }
        }
    }
    else {
        noInterrupts();
        result = spi_flash_read(eeprom_start_address, reinterpret_cast<uint32_t *>(dst), readSize);
        interrupts();
        __LDBG_assert_printf(result == SPI_FLASH_RESULT_OK, "spi_flash_read failed src=%08x dst=%08x size=%u", eeprom_start_address, dst, readSize);
        if (result == SPI_FLASH_RESULT_OK && alignment) { // move to beginning of the destination
            // memmove(dst, dst + alignment, length);
            __DBG_assert(dst + length <= dst + size);
            std::copy_n(dst + alignment, length, dst);
        }
    }
    if (result == SPI_FLASH_RESULT_OK) {
        std::fill(dst + length, dst + size, 0);
    } else {
        std::fill(dst, dst + size, 0);
    }
    __LDBG_printf("spi_flash_read(%08x, %d) = %d, offset %u", eeprom_start_address, readSize, result, offset);
    //__DBG_IF_flashUsage(flashReadSize = readSize);

#if DEBUG_CONFIGURATION_VERIFY_DIRECT_EEPROM_READ
    auto hasBeenInitialized = _isInitialized;
    begin();
    auto cmpResult = memcmp(dst, getConstDataPtr() + offset, length);
    if (cmpResult) {
        __DBG_panic("res=%d dst=%p ofs=%d size=%u len=%d align=%u read_size=%u memcpy() failed=%d", result, dst, offset, size, length, alignment, readSize, cmpResult);
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
        __LDBG_printf("ofs= %d, len=%d: using esp_partition_read(%p)", offset, length, _partition);
        if (esp_partition_read(_partition, offset, (void *)dst, length) != ESP_OK) {
            memset(dst, 0, length);
        }
    }

#else

    begin();
    memcpy(dst, getConstDataPtr() + offset, length);

#endif

#if DEBUG_CONFIGURATION && 0
    __LDBG_printf("ofs=%u len=%u size=%u data=%s", offset, length, size, Configuration::__debugDumper(dst, length).c_str());
#endif
    return std::min(length, readSize);
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
        dumper.dump(EEPROM.getDataPtr() + offset, length);
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
