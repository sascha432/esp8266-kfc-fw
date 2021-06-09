/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

// library for handling flash memory / block storage
//
// - copy on write without buffering the entire sector in RAM
// - integrity check per sector
//
// - erase sector (makes it available to write)
// - initialize sector (erase + create - stores data on it)
// - copy and append data to sector (copies existing data from one sector to another one and appends new data)
// - validate (crc32 check)
//
//
// requires 64-128byte stack space

#include <Arduino_compat.h>

namespace SPIFlash {

    // values for erased flash memory
    static constexpr uint32_t kFlashEmpty = 0xffffffff;
    static constexpr uint32_t kFlashEmpty32 = 0xffffffff;
    static constexpr uint16_t kFlashEmpty16 = 0xffff;
    static constexpr uint8_t kFlashEmpty8 = 0xff;

    static constexpr uint32_t kFlashMagic = 0x208a74e2;
    static constexpr uint32_t kInitialCrc32 = ~0;
    static constexpr uint32_t kFlashMemoryStartAddress = SECTION_FLASH_START_ADDRESS;
    static constexpr uint16_t kFlashMemorySectorSize = SPI_FLASH_SEC_SIZE;

    // default buffer size in byte
    static constexpr uint16_t kBufferSize = 16 * 4;

    // get offset from adress stored in memory as const char *
    inline static uint32_t getOffset(const char address[]) {
        return (uint32_t)&address[0] - kFlashMemoryStartAddress;
    }

    // get offset from adress stored in memory as uint32_t
    inline static uint32_t getOffset(uint32_t *address) {
        return (uint32_t)address - kFlashMemoryStartAddress;
    }

    // get sector from address in memory
    inline static uint16_t getSector(const char address[]) {
        return getOffset(address) / kFlashMemorySectorSize;
    }

    inline static uint16_t getSector(uint32_t *address) {
        return getOffset(address) / kFlashMemorySectorSize;
    }

    struct FlashHeader {
        uint32_t _magic;
        uint32_t _crc32;
        union {
            struct {
                uint16_t _size;
                uint16_t _versioning: 1;
                uint16_t __reserved: 3;
            };
            uint32_t _filler;
        };
        uint32_t _version;

        FlashHeader() : _magic(kFlashMagic), _crc32(0), _filler(~0), _version(~0)
        {
        }

        FlashHeader(uint32_t crc32, uint16_t size = 0xffff) : FlashHeader()
        {
            _crc32 = crc32;
            _size = size;
        }

        FlashHeader(uint32_t crc32, size_t size) : FlashHeader(crc32, static_cast<uint16_t>(size))
        {
        }

        operator bool() const {
            return _magic == kFlashMagic;
        }

        // data has been written to the sector and cannot be changed anymore
        bool isFinal() const {
            return _crc32 != kInitialCrc32 || _version != kFlashEmpty32 || /*_size != kFlashEmpty16 || */_filler != kFlashEmpty32;
        }

        // sector contains no or invalid data
        // an erase cycle is required to reuse it
        bool canReuse() const {
            return (_magic != kFlashMagic) || (_size == kFlashEmpty16) || (_size == 0) || (_crc32 == kInitialCrc32);
        }

        uint16_t size() const {
            return *this ? (_size == kFlashEmpty16 ? 0 : _size) : 0;
        }

        uint16_t space() const {
            return *this ? ((SPI_FLASH_SEC_SIZE - sizeof(*this)) - size()) : 0;
        }

        void setPattern(uint8_t pattern) {
            std::fill_n(reinterpret_cast<uint8_t *>(this), pattern, sizeof(*this));
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

    enum class ClearStorageType : uint8_t {
        NONE = 0,
        ERASE,                      // erase all sectors
        FORMAT = ERASE,             // alias
        REMOVE_MAGIC,               // overwrite magic and invalidate sector
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

        // eturn true if valid
        inline operator bool() const {
            return _sector != 0;
        }

        // return true if valid and not empty
        inline bool hasData() const {
            return size() != 0;
        }

        // return true if valid and is any free space left
        inline bool canWrite() const {
            return space();
        }

        // return true if valid and available space is greater is equal size
        inline bool canWrite(uint16_t size) const {
            return space() >= size;
        }

        // return size or 0 if empty or invalid
        uint16_t size() const {
            return *this ? (_size == kFlashEmpty16 ? 0 : _size) : 0;
        }

        // return available space or 0 if invalid
        uint16_t space() const {
            return *this ? (kSectorMaxSize - size()) : 0;
        }
    };

    using FindResultVector = std::vector<FindResult>;
    using FindResultArray = std::array<FindResult, 2>;

    struct FlashResult {

        FlashResultType _result;        // the result is for _sector
        FlashHeader _header;            // on success it contains an empty header
        uint32_t _crc;                  // crc of the existing data
        uint16_t _sector;               // and points to the sector ready for append()
        uint16_t _size;                 // size of the data currently stored

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

        inline operator bool() const {
            return (_result == FlashResultType::SUCCESS) && (_sector != 0) && (_size != kFlashEmpty16);
        }

        inline operator FlashResultType() const {
            return _result;
        }

        inline bool hasData() const {
            return size() != 0;
        }

        inline bool canRead() const {
            return readSize() != 0;
        }

        inline bool canWrite() const {
            return space();
        }

        inline bool canWrite(uint16_t size) const {
            return space() >= size;
        }

        uint16_t size() const {
            return *this ? (_size == kFlashEmpty16 ? 0 : _size) : 0;
        }

        inline uint16_t readSize() const {
            return size();
        }

        uint16_t space() const {
            return *this ? (kSectorMaxSize - size()) : 0;
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

        // clear storage memory
        bool clear(ClearStorageType type, uint32_t options = 0) const;

        // return a list of sectors with at least minSpace free space
        // sort=true: sorts by empty sectors, then descending by space
        // limits: limit number of results
        FindResultVector find(uint16_t fromSector, uint16_t toSector, uint16_t minSpace = 0, bool sort = false, uint16_t limit = ~0) const;

        inline FindResultVector find(uint16_t minSpace = 0, bool sort = false, uint16_t limit = ~0) const {
            return find(_firstSector, _lastSector, minSpace, sort, limit);
        }

        // find 1 empty and 1 writable sector or 2 empty ones
        // result = 0: not found
        // result = 1: empty sector found
        // result = 2: empty and writable(or another empty) sector found
        // the empty sector is stored at the first position
        uint8_t find(uint16_t fromSector, uint16_t toSector, FindResultArray &dst, uint16_t minSpace = 0) const;

        // read data from flash into memory
        // result.readSize() is the number of bytes read into data
        // if result is not nullptr, it will be used as cache instead of reading the header for every read() call
        // the cache is sector based and reset if it does not match the sector of the read call
        FlashResult read(uint32_t *data, uint16_t maxSize, uint16_t sector, uint16_t offset, FlashResult *result = nullptr);

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
