/* Flash Layout */
/* sketch    @0x40200000 (1019KB, 1044464) */
/* empty     @0x402FEFF0 (1028KB, 1052688) */
/* fs        @0x40400000 (1708KB, 1748992) */
/* nvs       @0x405AB000 (32KB, 32768) */
/* nvs2      @0x405B3000 (160KB, 163840) */
/* savecrash @0x405DB000 (128KB, 131072) */
/* eeprom    @0x405FB000 (4KB, 4096) */
/* rfcal     @0x405FC000 (4KB, 4096) */
/* wifi      @0x405FD000 (12KB, 12288) */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :                         org = 0x40201010, len = 0xfeff0
}

PROVIDE ( _FS_start = 0x40400000 );
PROVIDE ( _FS_end = 0x405AA000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x2000 );
PROVIDE ( _NVS_start = 0x405AB000 );
PROVIDE ( _NVS_end = 0x405B2000 );
PROVIDE ( _NVS2_start = 0x405B3000 );
PROVIDE ( _NVS2_end = 0x405DA000 );
PROVIDE ( _SAVECRASH_start = 0x405DB000 );
PROVIDE ( _SAVECRASH_end = 0x405FA000 );
PROVIDE ( _EEPROM_start = 0x405FB000 );
PROVIDE ( _EEPROM_end = 0x405FB000 );
/* The following symbols are DEPRECATED and will be REMOVED in a future release */
PROVIDE ( _SPIFFS_start = 0x40400000 );
PROVIDE ( _SPIFFS_end = 0x405AA000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x2000 );

INCLUDE "local.eagle.app.v6.common.ld"
