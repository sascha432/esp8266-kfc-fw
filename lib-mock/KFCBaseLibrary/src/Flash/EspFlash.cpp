/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <iostream>
#include <fstream>
#include "section_defines.h"
#include "ESP.h"

#define DEBUG_ESP_FLASH_MEMORY 0

EspClass _ESP;
EspClass &ESP = _ESP;

EspClass::EspClass() 
{
	memset(&resetInfo, 0, sizeof(resetInfo));
	_rtcMemory = nullptr;
	_readRtcMemory();
}

namespace FlashMemory {

	static constexpr uint32_t kSectorSize = 0x1000;
	static constexpr uint32_t kBufferSize = 0x100;
	static constexpr uint32_t kBufferSize32 = 0x100 / 4;
	static constexpr uint32_t kMaxSize = FLASH_MEMORY_STORAGE_MAX_SIZE;
	static constexpr uint32_t kMaxSectors = kMaxSize / kSectorSize;
	static constexpr uint32_t kEmptyCrc = 0xaf19d570;

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

		const char *c_str() const {
			return reinterpret_cast<const char *>(data.data());
		}

		char *c_str() {
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

		Index(uint32_t _address = 0) : address(_address), crc(kEmptyCrc), cycles(0) {
		}

		uint32_t getOffset(uint32_t sectorNum) const {
			return sectorNum * kSectorSize;
		}

		bool seekg(std::fstream &stream, uint32_t sectorNum) const;
		bool seekp(std::fstream &stream, uint32_t sectorNum) const;

		bool read(std::fstream &stream) {
			stream.read(reinterpret_cast<char *>(this), sizeof(*this));
			return (stream.rdstate() & std::ifstream::failbit) == 0;
		}

		bool write(std::fstream &stream, uint32_t sectorNum) {
			if (!seekp(stream, sectorNum)) {
				return false;
			}
			if (!write(stream)) {
				return false;
			}
			stream.flush();
			return (stream.rdstate() & std::ifstream::failbit) == 0;
		}

		bool read(std::fstream &stream, uint32_t sectorNum) {
			return seekg(stream, sectorNum) && read(stream);
		}

		bool write(std::fstream &stream) {
			if ((stream.rdstate() & std::ifstream::failbit) != 0) {
#if DEBUG_ESP_FLASH_MEMORY
				::printf("FLASH: write error address=0x%08x\n", address);
#endif
				return false;
			}
#if DEBUG_ESP_FLASH_MEMORY
			::printf("FLASH: write address=0x%08x index=%u offset=%u\n", address, (uint32_t)stream.tellp() / sizeof(*this), (uint32_t)stream.tellp());
#endif
			stream.write(reinterpret_cast<char *>(this), sizeof(*this));
			return (stream.rdstate() & std::ifstream::failbit) == 0;
		}

		void clear() {
			crc = kEmptyCrc;
			cycles++;
			flags.clear();
		}

		bool validateCrc(uint32_t dataCrc) const {
			if (flags.empty) {
				return crc == kEmptyCrc;
			}
			return crc == dataCrc;
		}
	};

	struct FindSector {
		Index index;
		uint32_t sectorNum;
		uint32_t offset;
		ErrorType error;

		FindSector(ErrorType _error = ErrorType::INVALID_ADDRESS) :
			sectorNum(~0U),
			offset(~0U),
			error(_error)
		{ }

		FindSector(const Index &_index, uint32_t _sectorNum, uint32_t _absOffset) :
			index(_index),
			sectorNum(_sectorNum),
			offset(_absOffset - (_sectorNum * kSectorSize)),
			error(ErrorType::SUCCESS)
		{ }

		operator bool() const {
			return error == ErrorType::SUCCESS;
		}

		bool next(std::fstream &stream) {
			if (error != ErrorType::SUCCESS) {
				return false;
			}
			if (sectorNum >= kMaxSectors) {
				error = ErrorType::READ_EOF;
				return false;
			}
			sectorNum++;
			offset = 0;
			return (index.seekg(stream, sectorNum) && index.read(stream));
		}

		uint32_t getFileOffset() const;

		uint32_t getOffset() const {
			return offset;
		}

		size_t space() const {
			if (offset < 0 || offset >= kSectorSize) {
				return 0;
			}
			return kSectorSize - offset;
		}

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
			auto sector = _findSector(address + _baseAddress);
			if (!sector) {
				return -1;
			}
			auto ptr = reinterpret_cast<char *>(data);
			uint32_t read = 0;
			while (len) {
				if (!sector.space()) {
					::printf("FLASH: invalid read operation address=0x%08x len=%u sector=%u offset=%u\n", address, len, sector.sectorNum, sector.offset);
					exit(1);
				}
				auto maxRead = std::min(sector.space(), len);
				Sector sectorData;
				// clear errors
				_fs.clear();
				// read sector data
				_fs.seekg(sector.getFileOffset());
				_fs.read(sectorData.c_str(), sectorData.size());
				if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
#if DEBUG_ESP_FLASH_MEMORY
					::printf("FLASH: read failed address=0x%08x I/O error while reading\n", sector.index.address);
#endif
					return -1;
				}
				// verify crc
				if (!sector.index.validateCrc(sectorData.crc())) {
#if DEBUG_ESP_FLASH_MEMORY
					::printf("FLASH: read failed address=0x%08x crc error while reading\n", sector.index.address);
#endif
					return -1;
				}
				// copy new data and advance ptr to next sector
				memcpy(ptr, sectorData.c_str() + sector.offset, maxRead);
				read += maxRead;
				ptr += maxRead;
				len -= maxRead;

				// no data left
				if (len == 0) {
					break;
				}

				// read index of next sector
				if (!sector.next(_fs)) {
#if DEBUG_ESP_FLASH_MEMORY
					::printf("FLASH: read failed address=0x%08x index error while reading\n", sector.index.address);
#endif
					return -1;
				}
			}
#if DEBUG_ESP_FLASH_MEMORY
			::printf("FLASH: data read address=0x%08x len=%u\n", address, read);
#endif
			return read;
		}

		int32_t write(uint32_t address, const void *data, uint32_t len) {
			auto sector = _findSector(address + _baseAddress);
			if (!sector) {
				return -1;
			}
			auto ptr = reinterpret_cast<const char *>(data);
			uint32_t written = 0;
			while (len) {
				if (!sector.space()) {
					::printf("FLASH: invalid write operation address=0x%08x len=%u sector=%u offset=%u\n", address, len, sector.sectorNum, sector.offset);
					exit(1);
				}
				auto maxWrite = std::min(sector.space(), len);
				Sector sectorData;
				// clear errors
				_fs.clear();
				// read sector data
				_fs.seekg(sector.getFileOffset());
				_fs.read(sectorData.c_str(), sectorData.size());
				if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
#if DEBUG_ESP_FLASH_MEMORY
					::printf("FLASH: write failed address=0x%08x I/O error while reading\n", sector.index.address);
#endif
					return -1;
				}
				// verify crc
				if (!sector.index.validateCrc(sectorData.crc())) {
#if DEBUG_ESP_FLASH_MEMORY
					::printf("FLASH: write failed address=0x%08x crc error while reading\n", sector.index.address);
#endif
					return -1;
				}
				// copy new data and advance ptr to next sector
				auto dst = sectorData.c_str() + sector.offset;
				auto endPtr = dst + maxWrite;
				while (dst < endPtr) {
					// only remove bits
					// erase must be used to reset the data
					*dst = static_cast<uint8_t>(*ptr++) & static_cast<uint8_t>(*dst);
					dst++;
				}
				// memcpy(sectorData.c_str() + sector.offset, ptr, maxWrite);
				// ptr += maxWrite;
				written += maxWrite;
				len -= maxWrite;

				// store sector data
				_fs.seekp(sector.getFileOffset());
#if DEBUG_ESP_FLASH_MEMORY
				::printf("FLASH: write sector address=0x%08x offset=0x%08x\n", sector.index.address, (uint32_t)sector.getFileOffset());
#endif
				_fs.write(sectorData.c_str(), sectorData.size());

				// update index crc
				sector.index.crc = sectorData.crc();
				sector.index.flags.empty = false;
				if (!sector.index.write(_fs, sector.sectorNum)) {
#if DEBUG_ESP_FLASH_MEMORY
					::printf("FLASH: write failed address=0x%08x I/O error while writing\n", sector.index.address);
#endif
					return -1;
				}

				// no data left
				if (len == 0) {
					break;
				}

				// read index of next sector
				if (!sector.next(_fs)) {
#if DEBUG_ESP_FLASH_MEMORY
					::printf("FLASH: write failed address=0x%08x index error while reading\n", sector.index.address);
#endif
					return -1;
				}
			}
#if DEBUG_ESP_FLASH_MEMORY
			::printf("FLASH: data written address=0x%08x len=%u\n", address, written);
#endif
			return written;
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

		void dump(Stream &stream) {
			uint32_t num = 0;
			uint32_t cycles = 0;
			for (uint32_t i = 0; i < kMaxSectors; i++) {
				Index index;
				if (index.read(_fs, i)) {
					if (!index.flags.empty) {
						stream.printf("[%08x]: sector=% 4u (%04x) crc=0x%08x cycles=%u index=%u data=%u\n", index.address, i, i, index.crc, index.cycles, i * sizeof(index) + _getIndexOffset(), index.getOffset(i) + _getDataOffset());
						cycles += index.cycles;
						num++;
					}
				}
				else {
					stream.printf("index read error sector=%u\n", i);
					return;
				}
			}
			stream.printf("%u sectors used, %u cycles, total size %u\n", num, cycles, num * kSectorSize);
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
#if DEBUG_ESP_FLASH_MEMORY
				::printf("FLASH: cannot read index\n");
#endif
				return ErrorType::READ_INDEX;
			}
			auto size = static_cast<uint32_t>(_fs.tellg());
			if (size < _getDataOffset()) {
#if DEBUG_ESP_FLASH_MEMORY
				::printf("FLASH: invalid index size=%u\n", size);
#endif
				return ErrorType::INVALID_INDEX_SIZE;
			}
			if (size < kMaxSize + _getDataOffset()) {
#if DEBUG_ESP_FLASH_MEMORY
				::printf("FLASH: invalid data size=%u\n", size);
#endif
				return ErrorType::INVALID_DATA_SIZE;
			}
			_fs.seekg(0, std::ios::beg);
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
#if DEBUG_ESP_FLASH_MEMORY
				::printf("FLASH: index seek=0 failed\n");
#endif
				return ErrorType::READ_INDEX;
			}

#if DEBUG_ESP_FLASH_MEMORY
			::printf("FLASH: checking index max_sectors=%u\n", kMaxSectors);
#endif
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
					if (!index.validateCrc(crc)) {
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
#if DEBUG_ESP_FLASH_MEMORY
				uint32_t ofs = (uint32_t)_fs.tellp() - sizeof(index);
				::printf("FLASH: format address=0x%08x index=%u offset=%u\n", address, ofs / sizeof(index), ofs);
#endif
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
#if DEBUG_ESP_FLASH_MEMORY
				::printf("FLASH: format address=0x%08x sector=%u offset=%u\n", address, i, (uint32_t)_fs.tellp() - (uint32_t)sector.size());
#endif
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
			return FindSector(index, sector, address - _baseAddress);
		}

		bool _eraseSector(FindSector sector) {
			_fs.clear();
			_fs.seekp(sector.getFileOffset());
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
				return false;
			}
			Sector sectorData;
			sector.index.cycles++;
			sector.index.clear();
			_fs.write(sectorData.c_str(), sectorData.size());
			if ((_fs.rdstate() & std::ifstream::failbit) != 0) {
				return false;
			}
			return sector.index.write(_fs, sector.sectorNum);
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

void EspClass::flashDump(Stream &output)
{
	if (!FlashMemory::flashStorage.open()) {
		output.println("failed to open flash storage");
	}
	FlashMemory::flashStorage.dump(output);
}
