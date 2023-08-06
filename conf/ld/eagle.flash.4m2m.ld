/* Flash Layout flash=4M fs=2M (480 sectors to keep "empty" bigger as the sketch for OTA) */
/* sketch    @0x40200000 (1019KB, 1044464, 0xfeff0) */
/* empty     @0x402FEFF0 (1072KB, 1097744, 0x10c010) */
/* fs        @0x4040B000 (1920KB, 1966080, 0x1e0000) */
/* nvs       @0x405EB000 (32KB, 32768, 0x8000) */
/* savecrash @0x405F3000 (32KB, 32768, 0x8000) */
/* eeprom    @0x405FB000 (4KB, 4096, 0x1000) */
/* rfcal     @0x405FC000 (4KB, 4096, 0x1000) */
/* wifi      @0x405FD000 (12KB, 12288, 0x3000) */

MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  irom0_0_seg :                         org = 0x40201010, len = 0xfeff0
}

PROVIDE ( _FS_start = 0x4040B000 );
PROVIDE ( _FS_end = 0x405EA000 );
PROVIDE ( _FS_page = 0x100 );
PROVIDE ( _FS_block = 0x2000 );
PROVIDE ( _NVS_start = 0x405EB000 );
PROVIDE ( _NVS_end = 0x405F2000 );
PROVIDE ( _SAVECRASH_start = 0x405F3000 );
PROVIDE ( _SAVECRASH_end = 0x405FA000 );
PROVIDE ( _EEPROM_start = 0x405FB000 );
PROVIDE ( _EEPROM_end = 0x405FB000 );
/* The following symbols are DEPRECATED and will be REMOVED in a future release */
PROVIDE ( _SPIFFS_start = 0x4040B000 );
PROVIDE ( _SPIFFS_end = 0x405EA000 );
PROVIDE ( _SPIFFS_page = 0x100 );
PROVIDE ( _SPIFFS_block = 0x2000 );

INCLUDE "local.eagle.app.v6.common.ld"
