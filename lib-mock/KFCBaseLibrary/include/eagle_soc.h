/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

namespace FlashMemory {

	struct Uint32Address {

		uint32_t _address;

		Uint32Address(uint32_t address = 0) : _address(address) {}

		uint32_t operator &() const {
			return (uint32_t)(_address);
		}
	};

	struct CharAddress {

		uint32_t _address;

		CharAddress(uint32_t address = 0) : _address(address) {}

		char *operator &() const {
			return (char *)(_address);
		}
	};

	class AddressContainer {
	public:
		AddressContainer();

		static AddressContainer &getInstance();

	};
}
