/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <iostream>
#include <fstream>
#include "section_defines.h"
#include "ESP.h"

namespace FlashMemory {

	static constexpr uint32_t kSectorSize = 0x1000;
	static constexpr uint32_t kBufferSize = 0x100;
	static constexpr uint32_t kBufferSize32 = 0x100 / 4;
	static constexpr uint32_t kMaxSize = FLASH_MEMORY_STORAGE_MAX_SIZE;
	static constexpr uint32_t kMaxSectors = kMaxSize / kSectorSize;

	enum class ErrorType {
		SUCCESS,
		INVALID_ADDRESS,
		INVALID_INDEX_SIZE,
		INVALID_DATA_SIZE,
		CRC_ERROR,
		READ_PROTECTED,
		WRITE_PROTECTED,
		READ_EOF,
		WRITE_EOF,
		READ_INDEX,
		WRITE_INDEX,
		READ_DATA,
		WRITE_DATA
	};

	struct Sector {
		std::array<uint32_t, kSectorSize / sizeof(uint32_t)> data;

		Sector() {
			clear();
		}

		void clear() {
			data.fill(~0U);
		}

		operator const char *() const {
			return reinterpret_cast<const char *>(data.data());
		}

		operator char *() {
			return reinterpret_cast<char *>(data.data());
		}

		// in bytes
		constexpr size_t size() const {
			return kSectorSize;
		}

		uint32_t crc() const {
			return crc32(data.data(), size());
		}
	};

	struct Flags {
		static constexpr uint32_t kEmpty = 0x01;

		union {
			uint32_t _flags;
			struct {
				uint32_t empty : 1;
				uint32_t readProtected : 1;
				uint32_t writeProtected : 1;
			};
		};
		Flags() : _flags(kEmpty) {}

		void clear() {
			_flags = kEmpty;
		}
	};

	struct Index {
		uint32_t address;
		uint32_t crc;
		uint32_t cycles;
		Flags flags;

		Index(uint32_t _address = 0) : address(_address), crc(~0U), cycles(0) {
		}

		uint32_t getOffset(uint32_t sectorNum) const {
			return sectorNum * kSectorSize;
		}

		bool seekg(std::fstream &stream, uint32_t sectorNum) const;
		bool seekp(std::fstream &stream, uint32_t sectorNum) const;

		bool read(std::fstream &stream) {
			stream.clear();
			stream.read(reinterpret_cast<char *>(this), sizeof(*this));
			return (stream.rdstate() & std::ifstream::failbit) == 0;
		}

		bool write(std::fstream &stream) {
			stream.clear();
			stream.write(reinterpret_cast<char *>(this), sizeof(*this));
			return (stream.rdstate() & std::ifstream::failbit) == 0;
		}

		void clear() {
			crc = ~0U;
			cycles++;
			flags.clear();
		}
	};

	struct FindSector {
		Index index;
		uint32_t sectorNum;
		ErrorType error;

		FindSector(ErrorType _error = ErrorType::INVALID_ADDRESS) :
			sectorNum(0),
			error(_error)
		{ }

		FindSector(const Index &_index, uint32_t _sectorNum) :
			index(_index),
			sectorNum(_sectorNum),
			error(ErrorType::SUCCESS)
		{ }

		operator bool() const {
			return error == ErrorType::SUCCESS;
		}

		uint32_t getFileOffset() const;
	};

	struct Buffer : std::array<uint32_t, kBufferSize32> {
		Buffer() {
			fill(~0U);
		}

		operator const char *() const {
			return reinterpret_cast<const char *>(data());
		}

		operator char *() {
			return reinterpret_cast<char *>(data());
		}

		// byte size
		constexpr size_t size() const {
			return kBufferSize;
		}

		uint32_t updateCrc(uint32_t crc = ~0U) const {
			return crc32(data(), size(), crc);
		}
	};

	class Storage {
	public:
		Storage(uint32_t baseAddress = 0) : 
			_baseAddress(baseAddress) 
		{ }

		~Storage() {
			_close();
		}

		std::ios::iostate getState() const {
			return _fs.rdstate();
		}

		int32_t read(uint32_t address, void *data, uint32_t len) {
			auto sector = _findSector(address);
			if (!sector) {
				return -1;
			}
			return 0;
		}

		int32_t write(uint32_t address, const void *data, uint32_t len) {
			auto sector = _findSector(address);
			if (!sector) {
				return -1;
			}
			return 0;
		}

		bool eraseSector(uint32_t sectorNum) {
			auto sector = _findSector((sectorNum * kSectorSize) + _baseAddress);
			if (!sector) { 
				return false;
			}
			return _eraseSector(sector);
		}

		// open with index check and auto format
		bool open() {
			if (_open() && indexCheck()) {
				return true;
			}
			return _format();
		}

		bool indexCheck() {
			return _check(false) == ErrorType::SUCCESS;
		}

		bool fullCheck() {
			return _check(true) == ErrorType::SUCCESS;
		}

		bool flush() {
			_fs.clear();
			_fs.flush();
			return (_fs.rdstate() & std::ifstream::failbit) == 0;
		}

	private:
		friend FindSector;
		friend Index;

		bool _open(bool truncate = false) {
			if (!_fs.is_open()) {
				using namespace std;
				auto mode = ios::binary | ios::in | ios::out;
				if (truncate) {
					mode |= ios::trunc;
				}
				_fs.open(FLASH_MEMORY_STORAGE_FILE, mode);
				if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
					return false;
				}
			}
			return true;
		}

		void _close() {
			if (_fs.is_open()) {
				_fs.close();
			}
		}

		// check index and size
		ErrorType _check(bool checkCrc = false) {
			// get file size
			_fs.clear();
			_fs.seekg(0, std::ios::end);
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
				return ErrorType::READ_INDEX;
			}
			auto size = _fs.tellg();
			if (size < _getDataOffset()) {
				return ErrorType::INVALID_INDEX_SIZE;
			}
			if (size < kMaxSize + _getDataOffset()) {
				return ErrorType::INVALID_DATA_SIZE;
			}
			_fs.seekg(0, std::ios::beg);
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
				return ErrorType::READ_INDEX;
			}
			Index index;
			for (uint32_t i = 0; i < kMaxSectors; i++) {
				if (!index.seekg(_fs, i) || !index.read(_fs)) {
					return ErrorType::READ_INDEX;
				}
				if (index.address != _baseAddress + (i * kSectorSize)) {
					return ErrorType::INVALID_ADDRESS;
				}
				if (checkCrc) {
					// check crc of each sector
					_fs.seekg(index.getOffset(i) + _getDataOffset());
					uint32_t crc = ~0U;
					Buffer buffer;
					auto count = kSectorSize / buffer.size();
					while (count--) {
						_fs.read(buffer, buffer.size());
						if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
							return ErrorType::READ_DATA;
						}
						crc = buffer.updateCrc(crc);
					}
					if (!index.flags.empty && crc != index.crc) {
						return ErrorType::CRC_ERROR;
					}
				}
			}
			return ErrorType::SUCCESS;			
		}

		bool _format() {
			_close();
			if (!_open(true)) {
				return false;
			}
			uint32_t address = _baseAddress;
			for (uint32_t i = 0; i < kMaxSectors; i++) {
				Index index;
				if (index.seekg(_fs, i) && index.read(_fs)) {
					index.clear();
				}
				index.address = address;
				if (!index.seekp(_fs, i) || !index.write(_fs)) {
					return false;
				}
				address += kSectorSize;
			}
			_fs.seekp(_getDataOffset());
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
				return false;
			}
			for (uint32_t i = 0; i < kMaxSectors; i++) {
				Sector sector;
				_fs.write(sector, sector.size());
				if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
					return false;
				}
			}
			_fs.flush();
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
				return false;
			}
			return true;
		}

		FindSector _findSector(uint32_t address) {
			uint32_t sector = (address - _baseAddress) / kSectorSize;
			if (sector >= kMaxSectors) {
				return FindSector(ErrorType::INVALID_ADDRESS);
			}
			Index index;
			if (!index.seekg(_fs, sector) || !index.read(_fs)) {
				return FindSector(ErrorType::READ_INDEX);
			}
			return FindSector(index, sector);
		}

		bool _eraseSector(FindSector sector) {
			_fs.clear();
			_fs.seekp(sector.getFileOffset());
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
				return false;
			}
			Buffer buffer;
			auto count = kSectorSize / buffer.size();
			sector.index.cycles++;
			sector.index.clear();
			while (count--) {
				_fs.write(buffer, buffer.size());
				if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
					return false;
				}
			}
			return sector.index.seekp(_fs, sector.sectorNum) && sector.index.write(_fs);
		}

		static constexpr uint32_t _getIndexOffset() {
			return 0;
		}

		static constexpr uint32_t _getDataOffset() {
			return (kMaxSectors * sizeof(Index)) + _getIndexOffset();
		}

	private:
		std::fstream _fs;
		uint32_t _baseAddress;
	};

	inline bool Index::seekg(std::fstream &stream, uint32_t sectorNum) const
	{
		stream.clear();
		stream.seekg(Storage::_getIndexOffset() + (sectorNum * sizeof(*this)));
		return (stream.rdstate() & std::ifstream::failbit) == 0;
	}

	inline bool Index::seekp(std::fstream &stream, uint32_t sectorNum) const
	{
		stream.clear();
		stream.seekp(Storage::_getIndexOffset() + (sectorNum * sizeof(*this)));
		return (stream.rdstate() & std::ifstream::failbit) == 0;
	}

	inline uint32_t FindSector::getFileOffset() const
	{
		return Storage::_getDataOffset() + (sectorNum * kSectorSize);
	}

	Storage flashStorage(SECTION_FLASH_START_ADDRESS);

}

bool EspClass::flashEraseSector(uint32_t sector)
{
	if (!FlashMemory::flashStorage.open()) {
		return false;
	}
	return FlashMemory::flashStorage.eraseSector(sector);
}

bool EspClass::flashWrite(uint32_t address, const uint32_t *data, size_t size)
{
	return flashWrite(address, reinterpret_cast<const uint8_t *>(data), size);
}

bool EspClass::flashWrite(uint32_t address, const uint8_t *data, size_t size)
{
	if (!FlashMemory::flashStorage.open()) {
		return false;
	}
	return FlashMemory::flashStorage.write(address, data, size);
}

bool EspClass::flashRead(uint32_t address, uint32_t *data, size_t size)
{
	return flashRead(address, reinterpret_cast<uint8_t *>(data), size);
}

bool EspClass::flashRead(uint32_t address, uint8_t *data, size_t size)
{
	if (!FlashMemory::flashStorage.open()) {
		return false;
	}
	return FlashMemory::flashStorage.read(address, data, size);
}
