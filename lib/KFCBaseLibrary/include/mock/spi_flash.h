/**
* Author: sascha_lammers@gmx.de
*/

#if _WIN32

// SPI EEPROM emulation

#pragma once

#include <Arduino_compat.h>
#include <EEPROM.h>

#pragma warning(push)
#pragma warning(disable : 26812)

#define SPI_FLASH_SEC_SIZE 4096
#ifndef EEPROM_ADDR
#define EEPROM_ADDR 0x40200000
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

extern "C" uint32_t _EEPROM_start;


inline uint32_t spi_flash_get_id(void) {
	return 0;
}

inline SpiFlashOpResult spi_flash_erase_sector(uint16_t sec) {
	if (sec == (((uint32_t)&_EEPROM_start - EEPROM_ADDR) / SPI_FLASH_SEC_SIZE)) {
		if (EEPROM.begin()) {
			memset(EEPROM.getDataPtr(), 0, SPI_FLASH_SEC_SIZE);
			EEPROM.commit();
			return SPI_FLASH_RESULT_OK;
		}
	}
	return SPI_FLASH_RESULT_ERR;
}

inline SpiFlashOpResult spi_flash_write(uint32_t des_addr, uint32_t *src_addr, uint32_t size) {
	auto eeprom_start_address = ((uint32_t)&_EEPROM_start - EEPROM_ADDR);
	auto eeprom_ofs = (uint32_t)(src_addr - eeprom_start_address);
	if (eeprom_ofs + size <= SPI_FLASH_SEC_SIZE && (size % 4 == 0) && (((uint32_t)&des_addr) % 4 == 0)) {
		if (EEPROM.begin()) {
			memcpy(EEPROM.getDataPtr() + eeprom_ofs, src_addr, size);
			EEPROM.commit();
			return SPI_FLASH_RESULT_OK;
		}
	}
	return SPI_FLASH_RESULT_ERR;
}

inline SpiFlashOpResult spi_flash_read(uint32_t src_addr, uint32_t *des_addr, uint32_t size) 
{
	auto eeprom_start_address = ((uint32_t)&_EEPROM_start - EEPROM_ADDR);
	auto eeprom_ofs = (uint32_t)(src_addr - eeprom_start_address);
	if (eeprom_ofs + size <= SPI_FLASH_SEC_SIZE && (size % 4 == 0) && (((uint32_t)&des_addr) % 4 == 0)) {
		if (EEPROM.begin()) {
			memcpy(des_addr, EEPROM.getConstDataPtr() + eeprom_ofs, size);
			return SPI_FLASH_RESULT_OK;
		}
	}
	return SPI_FLASH_RESULT_ERR;
}

#pragma warning(pop)

#endif