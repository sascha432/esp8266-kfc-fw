/**
  Author: sascha_lammers@gmx.de
*/

#include "nvs_common.h"
#include <string.h>
#include <Esp.h>

#include <Arduino_compat.h>
#if DEBUG_NVS_FLASH
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

/*

custom implementation that supports 2 NVS partitions only! An NVS partition requires at least 3 sectors, 6 or more are recommended for wear leveling.

more information can be found here: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html

NVS partition 1 will be initialized automatically (at least 3 sectors are required)
NVS partition 2 must be initialized before use (0 sectors can be used if not required)

eagle.ld example

// nvs       @0x405AB000 (32KB, 32768)
// nvs2      @0x405B3000 (160KB, 163840)

PROVIDE ( _NVS_start = 0x405B3000 );
PROVIDE ( _NVS_end = 0x405DA000 );
PROVIDE ( _NVS2_start = 0x405B3000 );
PROVIDE ( _NVS2_end = 0x405DA000 );


*/

#define SECTION_FLASH_START_ADDRESS 0x40200000U
#define SECTION_NVS_START_ADDRESS   ((uint32_t)&_NVS_start)
#define SECTION_NVS_END_ADDRESS     ((uint32_t)&_NVS_end)
#define SECTION_NVS2_START_ADDRESS  ((uint32_t)&_NVS2_start)
#define SECTION_NVS2_END_ADDRESS    ((uint32_t)&_NVS2_end)

extern "C" uint32_t _NVS_start;
extern "C" uint32_t _NVS_end;
extern "C" uint32_t _NVS2_start;
extern "C" uint32_t _NVS2_end;

#define NVS_PART_LABEL_1 "nvs"
#define NVS_PART_LABEL_2 "nvs2"

static const esp_partition_t nvs_partitions[2] = {
    { ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, SECTION_NVS_START_ADDRESS - SECTION_FLASH_START_ADDRESS, (SECTION_NVS_END_ADDRESS - SECTION_NVS_START_ADDRESS), NVS_PART_LABEL_1, false },
    { ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, SECTION_NVS2_START_ADDRESS - SECTION_FLASH_START_ADDRESS, (SECTION_NVS2_END_ADDRESS - SECTION_NVS2_START_ADDRESS), NVS_PART_LABEL_2, false }
};

extern "C" const esp_partition_t* esp_partition_find_first(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label)
{
    __LDBG_printf("type=%u subtype=%u label=%s", type, subtype, __S(label));
    if (type != ESP_PARTITION_TYPE_DATA && (subtype != ESP_PARTITION_SUBTYPE_DATA_NVS && subtype != ESP_PARTITION_SUBTYPE_ANY)) {
        return nullptr;
    }
    if (label) {
        if (!strcmp_P(label, PSTR(NVS_PART_LABEL_1))) {
            return &nvs_partitions[0];
        }
        else if (!strcmp_P(label, PSTR(NVS_PART_LABEL_2))) {
            return &nvs_partitions[1];
        }
        else {
            return nullptr;
        }
    }
    return &nvs_partitions[0];
}


extern "C" esp_partition_iterator_t esp_partition_find(esp_partition_type_t type, esp_partition_subtype_t subtype, const char* label)
{
    __LDBG_printf("type=%u subtype=%u label=%s", type, subtype, __S(label));
    if (type != ESP_PARTITION_TYPE_DATA && (subtype != ESP_PARTITION_SUBTYPE_DATA_NVS && subtype != ESP_PARTITION_SUBTYPE_ANY)) {
        return nullptr;
    }
    esp_partition_iterator_opaque_ *iterator = nullptr;
    if (label) {
        if (!strcmp_P(label, PSTR(NVS_PART_LABEL_1))) {
            iterator = new esp_partition_iterator_opaque_();
            iterator->partition = &nvs_partitions[0];
            iterator->next = nullptr;
        }
        else if (!strcmp_P(label, PSTR(NVS_PART_LABEL_2))) {
            iterator = new esp_partition_iterator_opaque_();
            iterator->partition = &nvs_partitions[1];
            iterator->next = nullptr;
        }
        else {
            return nullptr;
        }
    }
    else {
        iterator = new esp_partition_iterator_opaque_();
        iterator->partition = &nvs_partitions[0];;
        iterator->next = &nvs_partitions[1];;
    }
    return iterator;
}

extern "C" void esp_partition_iterator_release(esp_partition_iterator_t iterator)
{
    __LDBG_printf("iterator=%p", iterator);
    if (iterator) {
        delete iterator;
    }
}

extern "C" esp_partition_iterator_t esp_partition_next(esp_partition_iterator_t iterator)
{
    __LDBG_printf("iterator=%p part=%p next=%p", iterator, iterator ? iterator->partition : nullptr, iterator ? iterator->next : nullptr);
    if (iterator) {
        if (iterator->next) {
            iterator->partition = iterator->next;
            iterator->next = nullptr;
            return iterator;
        }
        else {
            iterator->partition = nullptr;
        }
    }
    return nullptr;
}

extern "C" const esp_partition_t* esp_partition_get(esp_partition_iterator_t iterator)
{
    __LDBG_printf("iterator=%p part=%p next=%p", iterator, iterator ? iterator->partition : nullptr, iterator ? iterator->next : nullptr);
    if (!iterator || !iterator->partition) {
        return nullptr;
    }
    return iterator->partition;
}

// check bounds and write absolute start address into "start_address"
static esp_err_t esp_partition_check_bounds(const esp_partition_t* partition, size_t offset, size_t size, uint32_t &start_address)
{
    start_address = partition->address + offset;
    uint32_t end = partition->address + partition->size;
    // __LDBG_printf("label=%s addr=%x size=%u ofs=%u len=%u range=%u-%u e1=%d e2=%d", partition->label, partition->address, partition->size, offset, size, start_address, end, (start_address >= end), (start_address + size > end));
    if (start_address >= end) {
        return ESP_ERR_INVALID_ARG;
    }
    if (start_address + size > end) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

extern "C" esp_err_t esp_partition_read(const esp_partition_t* partition, size_t src_offset, void *dst, size_t size)
{
    // __LDBG_printf("label=%s src_ofs=%u size=%u", partition->label, src_offset, size);
    esp_err_t err;
    uint32_t start;
    if ((err = esp_partition_check_bounds(partition, src_offset, size, start)) != ESP_OK) {
        return err;
    }
    return ESP.flashRead(start, reinterpret_cast<uint8_t *>(dst), size) ? ESP_OK : ESP_ERR_FLASH_BASE;
}

extern "C" esp_err_t esp_partition_write(const esp_partition_t* partition, size_t dst_offset, const void *src, size_t size)
{
    __LDBG_printf("label=%s src_ofs=%u size=%u", partition->label, dst_offset, size);
    esp_err_t err;
    uint32_t start;
    if ((err = esp_partition_check_bounds(partition, dst_offset, size, start)) != ESP_OK) {
        return err;
    }
    return ESP.flashWrite(start, reinterpret_cast<const uint8_t *>(src), size) ? ESP_OK : ESP_ERR_FLASH_BASE;
}

extern "C" esp_err_t esp_partition_erase_range(const esp_partition_t* partition, uint32_t start_addr, uint32_t size)
{
    __LDBG_printf("label=%s src_ofs=%u size=%u", partition->label, start_addr, size);
    esp_err_t err;
    uint32_t start;
    if ((err = esp_partition_check_bounds(partition, start_addr, size, start)) != ESP_OK) {
        return err;
    }
    if ((start % SPI_FLASH_SEC_SIZE) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((size % SPI_FLASH_SEC_SIZE) != 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    start /= SPI_FLASH_SEC_SIZE;
    size /= SPI_FLASH_SEC_SIZE;
    while(size--) {
        if (!ESP.flashEraseSector(start++)) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}
