/**
* Author: sascha_lammers@gmx.de
*/

#if _MSC_VER

// using ESP class for writing flash

#pragma once

#include <Arduino_compat.h>
#include <EEPROM.h>

#pragma warning(push)
#pragma warning(disable : 26812)

#define SPI_FLASH_SEC_SIZE 4096
#ifndef SECTION_EEPROM_START_ADDRESS
#define SECTION_EEPROM_START_ADDRESS ((uintptr_t)&_EEPROM_start)
#endif

typedef enum {
	SPI_FLASH_RESULT_OK,
	SPI_FLASH_RESULT_ERR,
	SPI_FLASH_RESULT_TIMEOUT
} SpiFlashOpResult;

typedef struct {
	uint32_t	deviceId;
	uint32_t	chip_size;    // chip size in byte
	uint32_t	block_size;
	uint32_t	sector_size;
	uint32_t	page_size;
	uint32_t	status_mask;
} SpiFlashChip;

extern SpiFlashChip *flashchip; // in ram ROM-BIOS

#define EEPROM_getDataPtr() const_cast<uint8_t *>(EEPROM.getConstDataPtr())

inline uint32_t spi_flash_get_id(void) {
	return 0;
}

inline SpiFlashOpResult spi_flash_erase_sector(uint16_t sec) {
	return ESP.flashEraseSector(sec) ? SPI_FLASH_RESULT_OK : SPI_FLASH_RESULT_ERR;
}

inline SpiFlashOpResult spi_flash_write(uint32_t des_addr, uint32_t *src_addr, uint32_t size) {
	return ESP.flashWrite(des_addr, src_addr, size) ? SPI_FLASH_RESULT_OK : SPI_FLASH_RESULT_ERR;
}

inline SpiFlashOpResult spi_flash_read(uint32_t src_addr, uint32_t *des_addr, uint32_t size)
{
	return ESP.flashRead(src_addr, des_addr, size) ? SPI_FLASH_RESULT_OK : SPI_FLASH_RESULT_ERR;
}

#pragma warning(pop)

#endif
