/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <section_defines.h>
#include <memory>

uint32_t &_irom0_text_start = *(uint32_t *)0x40201010;
uint32_t &_irom0_text_end = *(uint32_t *)0x40201010;
char *&_heap_start = *(char **)0x3FFE8000;
uint32_t &_FS_start = *(uint32_t *)0x402FEFF0;
uint32_t &_FS_end = *(uint32_t *)0x402FEFF0;
uint32_t &_KFCFW_start = *(uint32_t *)0x405AB000;
uint32_t &_KFCFW_end = *(uint32_t *)0x405BA000;
uint32_t &_SAVECRASH_start = *(uint32_t *)0x405bb000;
uint32_t &_SAVECRASH_end = *(uint32_t *)0x405FA000;
uint32_t &_EEPROM_start = *(uint32_t *)0x405FB000;
uint32_t &_EEPROM_end = *(uint32_t *)0x405FB000;
