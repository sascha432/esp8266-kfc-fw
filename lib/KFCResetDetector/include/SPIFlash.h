/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// copy on write for SPI flash without buffering the entire sector in ram
//
// - erase sector
// - initialize sector (erase + create)
// - copy and append data to sector
// - validate (crc32 check)
//
// requires 64-128byte stack space

#include <Arduino_compat.h>

// #define DEBUG_RD_SPIFLASH                           1

#ifndef DEBUG_RD_SPIFLASH
#define DEBUG_RD_SPIFLASH                           0
#endif

namespace SPIFlash {

    static constexpr uint32_t kFlashMagic = 0x208a74e2;
    static constexpr uint32_t kFlashEmpty = 0xffffffff;
    static constexpr uint16_t kFlashEmpty16 = 0xffff;
    static constexpr uint16_t kFlashEmpty8 = 0xff;
    static constexpr uint32_t kInitialCrc32 = ~0;

    // default buffer size in byte
    static constexpr size_t kBufferSize = 16 * 4;

    struct FlashHeader {
        uint32_t _magic;
        uint32_t _crc32;
        uint16_t _size;
        uint16_t __reserved; // dword alignment

        FlashHeader() :
            _magic(kFlashEmpty),
            __reserved(kFlashEmpty8)
        {
        }

        FlashHeader(uint32_t crc32, uint16_t size) :
            _magic(kFlashMagic),
            _crc32(crc32),
            _size(size),
            __reserved(kFlashEmpty8)
        {
        }


        FlashHeader(uint32_t crc32, size_t size) :
            FlashHeader(crc32, static_cast<uint16_t>(size))
        {
        }


        operator bool() const {
            return isValid();
        }

        bool isValid() const {
            return _magic == kFlashMagic;
        }

        bool isEmpty() const {
            return _crc32 == kInitialCrc32 && _size == kFlashEmpty16;
        }

        uint16_t size() const {
            return _size == kFlashEmpty16 ? 0 : _size;
        }
    };

    static constexpr uint16_t kFlashHeaderSize = sizeof(FlashHeader);
    static_assert(sizeof(FlashHeader) % 4 == 0, "not dword aligned");

    static constexpr uint16_t kSectorMaxSize = SPI_FLASH_SEC_SIZE - sizeof(FlashHeader);

    enum class FlashResultType {
        NONE = 0,
        SUCCESS,
        // errors
        READ,
        WRITE,
        ERASE,
        EMPTY,
        INVALID,
        NO_SPACE,
        OUT_OF_RANGE,
        VALIDATE_READ,
        VALIDATE_CRC,
        VALIDATE_EMPTY,
        NOT_EMPTY,
        FINALIZED
    };

    struct FindResult {
        uint16_t _sector;
        uint16_t _size;
        uint32_t _crc32;

        FindResult() :
            _sector(0),
            _size(kFlashEmpty16)
        {}

        FindResult(uint32_t sector, uint16_t size = kFlashEmpty16, uint32_t crc32 = kInitialCrc32) :
            _sector(sector),
            _size(size),
            _crc32(crc32)
        {}

        operator bool() const {
            return _sector != 0;
        }

        bool isEmpty() const {
            return _size == kFlashEmpty16;
        }

        uint16_t size() const {
            return _size == kFlashEmpty16 ? 0 : _size;
        }

        uint16_t space() const {
            return kSectorMaxSize - size();
        }
    };

    using FindResultVector = std::vector<FindResult>;

    struct FlashResult {

        FlashResultType _result;        // the result is for _sector
        FlashHeader _header;            // on success it contains an empty header
        uint32_t _crc;                  // crc of the existing data
        uint16_t _sector;               // and points to the sector ready for append()
        uint16_t _size;                   // size of the data currently stored

        FlashResult() :
            _result(FlashResultType::NONE)
        {}

        // create sector with header
        FlashResult(uint16_t sector, const FlashHeader &header, uint16_t size = 0, uint32_t crc = kInitialCrc32) :
            _result(FlashResultType::SUCCESS),
            _header(header),
            _crc(crc),
            _sector(sector),
            _size(size)
        {}

        // report an error
        FlashResult(FlashResultType result, uint16_t sector) :
            _result(result),
            _crc(kFlashEmpty),
            _sector(sector),
            _size(kFlashEmpty16)
        {}

        operator bool() const {
            return (_result == FlashResultType::SUCCESS) && (_sector != 0) && (_size != kFlashEmpty16);
        }

        uint16_t size() const {
            return _size;
        }

        uint16_t readSize() const {
            return _size == kFlashEmpty16 ? 0 : _size;
        }

        uint16_t space() const {
            if (!*this) {
                return 0;
            }
            return kSectorMaxSize - _size;
        }

        inline static uint32_t headerOffset(uint16_t sector) {
            return sector * SPI_FLASH_SEC_SIZE;
        }

        inline static uint32_t dataOffset(uint16_t sector) {
            return headerOffset(sector) + sizeof(_header);
        }
    };

    // provides copy on write interface with CRC32
    class FlashStorage {
    public:
        // first and last sector to use
        // there must be at least one empty sector to be able to append data
        // due to the nature of COW the sector used for copying changes every time
        FlashStorage(uint16_t firstSector, uint16_t lastSector) :
            _firstSector(firstSector),
            _lastSector(lastSector)
        {}

        // format/erase all flash sectors
        bool format() const;

        // return a list of sectors with at least minSpace free space
        FindResultVector find(uint16_t fromSector, uint16_t toSector, uint16_t minSpace = 0, bool sort = false, uint8_t limit = 0xff) const;

        inline FindResultVector find(uint16_t minSpace = 0, bool sort = false, uint8_t limit = 0xff) const {
            return find(_firstSector, _lastSector, minSpace, sort, limit);
        }

        // read data from flash into memory
        // result.readSize() is the number of bytes read into data
        FlashResult read(uint32_t *data, uint16_t maxSize, uint16_t sector, uint16_t offset);

        // erase sector. use this method to release a sector after copying
        FlashResult erase(uint16_t sector) const;

        // erase sector and prepare for append()
        FlashResult init(uint16_t sector) const;

        // copies data from one sector to another leaving blanks for size, crc, ...
        // append data with append(), finish writing with finalize() and verify the written
        // data with validate(). after validating the original sector should be erased
        // with erase() to be available again
        FlashResult copy(uint16_t fromSector, uint16_t toSector) const;

        // append data to result from init() or copy()
        bool append(const uint32_t *data, uint16_t size, FlashResult &result) const;

        // the address of arg must be dword aligned
        template<typename _Ta>
        bool append(const _Ta &arg, FlashResult &result) const {
            return append(reinterpret_cast<const uint32_t *>(std::addressof(arg)), sizeof(arg), result);
        }

        // finalize sector and update crc/size
        bool finalize(FlashResult &result) const {
            return _finalize(result);
        }

        // finalize sector and update crc/size
        FlashResult validate(const FlashResult &result) const {
            return _validate(result);
        }

    protected:
        friend FlashResult;

        // copy/append data from a sector
        bool _copySector(uint16_t fromSector, uint16_t toSector, uint16_t size, FlashResult &result) const;

        // copy/append data from memory
        bool _copy(const uint32_t *data, uint32_t dstAddr, uint16_t size, FlashResult &result) const;

        // finalize sector and update crc/size
        bool _finalize(FlashResult &result) const;

        // compare written results
        FlashResult _validate(const FlashResult &result) const;

        // read from flash and calculate crc
        uint32_t _crc(uint32_t srcAddr, uint16_t size, uint32_t crc = kInitialCrc32) const;

    public:
        uint16_t begin() const {
            return _firstSector;
        }
        uint16_t end() const {
            return _lastSector;
        }

    private:
        uint16_t _firstSector;
        uint16_t _lastSector;
    };

}
