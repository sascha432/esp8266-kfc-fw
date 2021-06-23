/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>

namespace FlashMemory {

	struct Address {

		uint32_t _address;

		Address(uint32_t address = 0) : _address(address) {}

		uint32_t get() const {
			return (uint32_t)(&_address);
		}
		char *getCharArray() const {
			return (char *)(&_address);
		}
	};

}


uint32_t _irom0_text_start = FlashMemory::Address(0x40201010).get();
uint32_t _irom0_text_end = FlashMemory::Address(0x402FEFF0).get();
char _heap_start[];
uint32_t _FS_start = FlashMemory::Address(0x40400000).get();
uint32_t _FS_end = FlashMemory::Address(0x405AA000).get();
uint32_t _KFCFW_start = FlashMemory::Address(0x405AB000).get();
uint32_t _KFCFW_end = FlashMemory::Address(0x405BA000).get();
uint32_t _SAVECRASH_start = FlashMemory::Address(0x405bb000).get();
uint32_t _SAVECRASH_end = FlashMemory::Address(0x405FA000).get();
uint32_t _EEPROM_start = FlashMemory::Address(0x405FB000).get();
uint32_t _EEPROM_end = FlashMemory::Address(0x405FB000).get();
