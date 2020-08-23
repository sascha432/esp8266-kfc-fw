/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <stdint.h>

#if _MSC_VER

#define SECTION_FLASH_START_ADDRESS                         0x40200000
#define SECTION_IROM0_TEXT_START_ADDRESS                    0
#define SECTION_IROM0_TEXT_END_ADDRESS                      UINTPTR_MAX
#define SECTION_HEAP_START_ADDRESS                          0
#define SECTION_HEAP_END_ADDRESS                            UINTPTR_MAX
#define SECTION_EEPROM_START_ADDRESS                        SECTION_FLASH_START_ADDRESS

#else

extern "C" uint32_t _irom0_text_start;
extern "C" uint32_t _irom0_text_end;
extern "C" char _heap_start[];
extern "C" uint32_t _FS_start;
extern "C" uint32_t _FS_end;
extern "C" uint32_t _KFCFW_start;
extern "C" uint32_t _KFCFW_end;
extern "C" uint32_t _ESPSAVECRASH_start;
extern "C" uint32_t _EEPROM_start;


#define SECTION_FLASH_START_ADDRESS                         0x40200000
#define SECTION_IROM0_TEXT_START_ADDRESS                    ((uint32_t)&_irom0_text_start)
#define SECTION_IROM0_TEXT_END_ADDRESS                      ((uint32_t)&_irom0_text_end)
#define SECTION_HEAP_START_ADDRESS                          ((uint32_t)&_heap_start[0])
#define SECTION_HEAP_END_ADDRESS                            0x3fffc000U
#define SECTION_EEPROM_START_ADDRESS                        ((uint32_t)&_EEPROM_start)
#define SECTION_ESPSAVECRASH_START_ADDRESS                  ((uint32_t)&_ESPSAVECRASH_start)
#define SECTION_KFCFW_START_ADDRESS                         ((uint32_t)&_KFCFW_start)
#define SECTION_KFCFW_END_ADDRESS                           ((uint32_t)&_KFCFW_end)
#define SECTION_FS_START_ADDRESS                            ((uint32_t)&_FS_start)
#define SECTION_FS_END_ADDRESS                              ((uint32_t)&_FS_start)

#endif