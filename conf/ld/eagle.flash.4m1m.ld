/* Flash Layout */
/* sketch    @0x40200000 (1019KB, 1044464) */
/* empty     @0x402FEFF0 (2052KB, 2101264) */
/* fs        @0x40500000 (936KB, 958464) */
/* kfcfw     @0x405EA000 (36KB, 36864) */
/* savecrash @0x405F3000 (32KB, 32768) */
/* eeprom    @0x405FB000 (4KB, 4096) */
/* rfcal     @0x405FC000 (4KB, 4096) */
/* wifi      @0x405FD000 (12KB, 12288) */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                         org = 0x40100000, len = 0x8000
  irom0_0_seg :                         org = 0x40201010, len = 0xfeff0
}

PROVIDE ( _FS_start = 0x40500000 );
PROVIDE ( _FS_end = 0x405E9000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x2000 );
PROVIDE ( _KFCFW_start = 0x405EA000 );
PROVIDE ( _KFCFW_end = 0x405F2000 );
PROVIDE ( _SAVECRASH_start = 0x405F3000 );
PROVIDE ( _SAVECRASH_end = 0x405FA000 );
PROVIDE ( _EEPROM_start = 0x405FB000 );
PROVIDE ( _EEPROM_end = 0x405FB000 );
/* The following symbols are DEPRECATED and will be REMOVED in a future release */
PROVIDE ( _SPIFFS_start = 0x40500000 );
PROVIDE ( _SPIFFS_end = 0x405E9000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x2000 );

INCLUDE "local.eagle.app.v6.common.ld"
