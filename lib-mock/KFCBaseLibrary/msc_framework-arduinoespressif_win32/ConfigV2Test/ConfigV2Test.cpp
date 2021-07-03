/**
* Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "EEPROM.h"


namespace FlashStorage {

	static constexpr uint32_t kMagic = 0x208a74e3;

	struct Flags {
		Flags() : _empty(true), _filler(~0U) {}

		bool isEmpty() const {
			return _empty;
		}

		void removeEmpty() {
			_empty = 0;
		}

	private:
		uint16_t _empty: 1;
		uint16_t _filler: 15;
	};

	struct Index {

		Index() : _sector(~0U), _offset(~0U), _size(~0U), _version(~0U) {}

		uint32_t _sector: 12;
		uint32_t _offset: 10;
		uint32_t _size: 10;
		uint32_t _version: 16;
	};

	struct Ident {

		Ident() : _id(~0U), _version(~0U) {}

		uint32_t _id : 22;
		uint32_t _version: 10;
	};

	struct Header {
		Header() : _magic(kMagic), _crc(~0U), _size(~0U) {}

		void setCrc(uint32_t crc) {
			_crc = crc;
		}

		uint32_t getCrc() const {
			return _crc;
		}

		void setSize(uint16_t size) {
			_size = ~size;
		}

		uint16_t size() const {
			return ~_size;
		}

		uint16_t readSize() const {
			return (size() + 3) & ~0x2;
		}

		Flags &getFlags() {
			return _flags;
		}

		const Flags &getFlags() const {
			return _flags;
		}

	protected:
		uint32_t _magic;
		uint32_t _crc;
		uint16_t _size;
		Flags _flags;
	};

	class IndexBlock {
	public:

	private:
		uint32_t _crc;
		Index _index[21];
		Ident _ident[21];
	};

	static constexpr auto kIndexBlockSize = sizeof(IndexBlock);

}


void setup()
{
#if 0
	ESP.flashDump(Serial);

	/*const char *buf3 = "test";
	__dump_binary_to(Serial, buf3, sizeof(buf3));
	*/
	exit(0);
#endif

#if 1
	if (!ESP.flashEraseSector(0)) {
		Serial.println(F("Failed to erase sector 0"));
	}

	const char *buf2 = "test";
	ESP.flashWrite(32, (uint32_t *)&buf2[0], 5);

	const char *buf4 = "cannot change";
	ESP.flashWrite(32, (uint32_t *)buf4, strlen(buf4) + 1);

	uint8_t buf3[1024];
	ESP.flashRead(0, buf3, sizeof(buf3));

	__dump_binary_to(Serial, buf3, sizeof(buf3));

	exit(1);
#endif
#if 0
	int len = 0;

	EEPROM.begin(2);
	EEPROM.write(0, 0x21);
	EEPROM.write(1, 0x43);
	EEPROM.write(2, 0xbb);
	EEPROM.commit();

	EEPROM.begin(4);
	uint32_t buf;
	EEPROM.get(0, buf);

	Serial.printf("%08x\n", buf);

	EEPROM.end();

	exit(0);
#endif
}

void loop()
{
}

