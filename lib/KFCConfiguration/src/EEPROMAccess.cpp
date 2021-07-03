/**
* Author: sascha_lammers@gmx.de
*/

#include <DumpBinary.h>
#include <PrintString.h>
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

bool EEPROMAccess::read(void *dstPtr, uint16_t offset, uint16_t size)
{
    auto dst = reinterpret_cast<uint8_t *>(dstPtr);
    if (_isInitialized) {
        // if the EEPROM is intialized, copy data from memory
        std::copy_n(EEPROM.getConstDataPtr() + offset, size, dst);
        return true;
    }

#if defined(ESP8266)
    auto eeprom_start_address = (SECTION_EEPROM_START_ADDRESS - SECTION_FLASH_START_ADDRESS) + offset;
    if (!ESP.flashRead(eeprom_start_address, dst, size)) {
        __LDBG_printf("read error offset=%u size=%u", eeprom_start_address, size);
        std::fill_n(dst, size, 0);
        return false;
    }

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
    // std::fill(dst + size, endPtr, 0);

#else

    begin();
    memcpy(dst, getConstDataPtr() + offset, size);

#endif
    return true;
}
