/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <eagle_soc.h>
#include <memory>

uint32_t_f _irom0_text_start(0x40201010);
uint32_t_f _irom0_text_end(0x402FEFF0);
char_f _heap_start[1]{ (0x40400000) };
uint32_t_f _FS_start(0x40400000);
uint32_t_f _FS_end(0x405AA000);
uint32_t_f _KFCFW_start(0x405AB000);
uint32_t_f _KFCFW_end(0x405BA000);
uint32_t_f _SAVECRASH_start(0x405bb000);
uint32_t_f _SAVECRASH_end(0x405FA000);
uint32_t_f _EEPROM_start(0x405FB000);
uint32_t_f _EEPROM_end(0x405FB000);
