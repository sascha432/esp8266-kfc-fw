/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>

#if _MSC_VER

#ifndef FLASH_MEMORY_STORAGE_FILE
#define FLASH_MEMORY_STORAGE_FILE                           "EspFlashMemory.4m2m.dat"
#endif

#ifndef FLASH_MEMORY_STORAGE_MAX_SIZE
#define FLASH_MEMORY_STORAGE_MAX_SIZE						(4096 * 1024)	// 4MByte
#endif

#endif

#if _MSC_VER

// linker address emulation

#include <eagle_soc.h>

#define EAGLE_SOC_ADDRESSchar(name) extern "C" char *&name;
#define EAGLE_SOC_ADDRESSuint32_t(name)  extern "C" uint32_t &name;
#define EAGLE_SOC_ADDRESS(type, name) EAGLE_SOC_ADDRESS##type(name)

#else

#define EAGLE_SOC_ADDRESSchar(name) extern "C" char name[];
#define EAGLE_SOC_ADDRESSuint32_t(name)  extern "C" uint32_t name;
#define EAGLE_SOC_ADDRESS(type, name) EAGLE_SOC_ADDRESS##type(name)

#endif

EAGLE_SOC_ADDRESS(uint32_t, _irom0_text_start);
EAGLE_SOC_ADDRESS(uint32_t, _irom0_text_end);
EAGLE_SOC_ADDRESS(char, _heap_start);
EAGLE_SOC_ADDRESS(uint32_t, _FS_start);
EAGLE_SOC_ADDRESS(uint32_t, _FS_end);
EAGLE_SOC_ADDRESS(uint32_t, _KFCFW_start);
EAGLE_SOC_ADDRESS(uint32_t, _KFCFW_end);
EAGLE_SOC_ADDRESS(uint32_t, _SAVECRASH_start);
EAGLE_SOC_ADDRESS(uint32_t, _SAVECRASH_end);
EAGLE_SOC_ADDRESS(uint32_t, _EEPROM_start);
EAGLE_SOC_ADDRESS(uint32_t, _EEPROM_end);

#define SECTION_FLASH_START_ADDRESS                         0x40200000U
#define SECTION_FLASH_END_ADDRESS                           0x402FEFF0U
#define SECTION_IROM0_TEXT_START_ADDRESS                    ((uint32_t)&_irom0_text_start)
#define SECTION_IROM0_TEXT_END_ADDRESS                      ((uint32_t)&_irom0_text_end)
#define SECTION_HEAP_START_ADDRESS                          ((uint32_t)&_heap_start[0])
#define SECTION_HEAP_END_ADDRESS                            0x3fffc000U
#define SECTION_EEPROM_START_ADDRESS                        ((uint32_t)&_EEPROM_start)
#define SECTION_EEPROM_END_ADDRESS                          ((uint32_t)&_EEPROM_end)
#define SECTION_SAVECRASH_START_ADDRESS                     ((uint32_t)&_SAVECRASH_start)
#define SECTION_SAVECRASH_END_ADDRESS                       ((uint32_t)&_SAVECRASH_end)
#define SECTION_KFCFW_START_ADDRESS                         ((uint32_t)&_KFCFW_start)
#define SECTION_KFCFW_END_ADDRESS                           ((uint32_t)&_KFCFW_end)
#define SECTION_FS_START_ADDRESS                            ((uint32_t)&_FS_start)
#define SECTION_FS_END_ADDRESS                              ((uint32_t)&_FS_start)

// #endif
