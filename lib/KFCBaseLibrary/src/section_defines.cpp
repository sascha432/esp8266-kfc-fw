/**
  Author: sascha_lammers@gmx.de
*/

#include "section_defines.h"

#if ESP32

#include <Arduino_compat.h>
#include <esp_partition.h>

struct PartitionAddress {
    uint32_t _start;
    uint32_t _end;
    PartitionAddress(uint32_t start, uint32_t end) : _start(start), _end(end) {}
};

PartitionAddress get_partition_address_start(const char *label)
{
    auto part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, label);
    if (!part) {
        __DBG_panic("data partition label=%s not found", label);
    }
    return PartitionAddress(part->address + SECTION_FLASH_START_ADDRESS, part->address + part->size + SECTION_FLASH_START_ADDRESS - 1);
}

// uint32_t &_irom0_text_start;
// uint32_t &_irom0_text_end;
// char &_heap_start[];
// uint32_t &_FS_start;
// uint32_t &_FS_end;
// uint32_t &_KFCFW_start;
// uint32_t &_KFCFW_end;
static auto P_SAVECRASH_start = get_partition_address_start("savecrash");
static auto P_EEPROM_start = get_partition_address_start("eeprom");

uint32_t &_SAVECRASH_start = P_SAVECRASH_start._start;
uint32_t &_SAVECRASH_end = P_SAVECRASH_start._end;
uint32_t &_EEPROM_start = P_EEPROM_start._start;
uint32_t &_EEPROM_end = P_EEPROM_start._end;

#endif

