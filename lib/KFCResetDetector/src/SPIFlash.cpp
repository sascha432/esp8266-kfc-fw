/**
 * Author: sascha_lammers@gmx.de
 */

#include "SPIFlash.h"

#if defined(ESP8266)
#include "coredecls.h"
#include "user_interface.h"
#endif

#if DEBUG_RD_SPIFLASH
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if DEBUG_RD_SPIFLASH

static  SpiFlashOpResult _spi_flash_erase_sector(uint16 sec)
{
    auto result = spi_flash_erase_sector(sec);
    __LDBG_printf("erase sector=%04x result=%d", sec, result);
    return result;

}
static SpiFlashOpResult _spi_flash_write(uint32 des_addr, uint32 *src_addr, uint32 size)
{
    auto result = spi_flash_write(des_addr, src_addr, size);
    __LDBG_printf("write sector=%04x address=%08x size=%u result=%d", des_addr / SPI_FLASH_SEC_SIZE, des_addr, size, result);
    return result;
}

static SpiFlashOpResult _spi_flash_read(uint32 src_addr, uint32 *des_addr, uint32 size)
{
    auto result = spi_flash_read(src_addr, des_addr, size);
    __LDBG_printf("read sector=%04x address=%08x size=%u result=%d", src_addr / SPI_FLASH_SEC_SIZE, src_addr, size, result);
    return result;
}

#else

#define _spi_flash_read                 spi_flash_read
#define _spi_flash_write                spi_flash_write
#define _spi_flash_erase_sector         spi_flash_erase_sector

#endif


namespace SPIFlash {

    FindResultVector FlashStorage::find(uint16_t fromSector, uint16_t toSector, uint16_t minSpace, bool sort, uint8_t limit) const
    {
        FindResultVector results;
        for(auto i = fromSector; i <= toSector && limit; i++) {
            FlashHeader header;
            auto rc = _spi_flash_read(FlashResult::headerOffset(i), reinterpret_cast<uint32_t *>(&header), sizeof(header));
            if (rc == SPI_FLASH_RESULT_OK) {
                if (!header || header.isEmpty()) {
                    results.emplace_back(i, kFlashEmpty16);
                    limit--;
                }
                else {
                    if (kSectorMaxSize - header.size() >= minSpace) {
                        if (_crc(FlashResult::dataOffset(i), header.size()) == header._crc32) {
                            if (!results.empty()) { // do not count until we have at least one free sector
                                limit--;
                            }
                            results.emplace_back(i, header._size, header._crc32);
                        }
                        else {
                            __DBG_printf("crc mismatch sector=0x%04x size=%u", i, header.size());
#if 0
                            // re-use sectors with crc error
                            results.emplace_back(i, kFlashEmpty16);
                            limit--;
#endif
                        }
                    }
                }
            }
            else {
                __DBG_printf("read error sector=0x%04x", i);
            }
        }
        if (sort) {
            std::sort(results.begin(), results.end(), [](const FindResult &a, const FindResult &b) {
                return b.space() < a.space();
            });
        }
        return results;
    }

    FlashResult FlashStorage::read(uint32_t *data, uint16_t maxSize, uint16_t sector, uint16_t offset)
    {
        auto header = FlashHeader();
        auto rc = _spi_flash_read(FlashResult::headerOffset(sector), reinterpret_cast<uint32_t *>(&header), sizeof(header));
        if (rc != SPI_FLASH_RESULT_OK) {
            __LDBG_printf("read failed offset=0x%08x", sector * SPI_FLASH_SEC_SIZE);
            return FlashResult(FlashResultType::READ, sector);
        }
        uint32_t crc;
        if ((crc = _crc(FlashResult::dataOffset(sector), header._size)) != header._crc32) {
            __LDBG_printf("crc mismatch %08x!=%08x", crc, header._crc32);
            // return FlashResult(FlashResultType::VALIDATE_CRC, sector);
        }
        auto read = std::min<uint16_t>(maxSize, std::max(0, header._size - offset));
        if (!read) {
            __LDBG_printf("offset=%u >= size=%u", header._size, offset);
            return FlashResult(FlashResultType::OUT_OF_RANGE, sector);
        }
        rc = _spi_flash_read(FlashResult::dataOffset(sector) + offset, data, read);
        if (rc != SPI_FLASH_RESULT_OK) {
            __LDBG_printf("read failed offset=0x%08x size=%u", FlashResult::dataOffset(sector) + offset, read);
            return FlashResult(FlashResultType::READ, sector);
        }
        return FlashResult(sector, header, read);
    }

    FlashResult FlashStorage::erase(uint16_t sector) const
    {
        auto rc = _spi_flash_erase_sector(sector);
        if (rc != SPI_FLASH_RESULT_OK) {
            __LDBG_printf("erase failed sector=0x%04x", sector);
            return FlashResult(FlashResultType::ERASE, sector);
        }
        return FlashResult(FlashResultType::SUCCESS, sector);
    }

    FlashResult FlashStorage::init(uint16_t sector) const
    {
        auto rc = _spi_flash_erase_sector(sector);
        if (rc != SPI_FLASH_RESULT_OK) {
            __LDBG_printf("erase failed sector=0x%04x", sector);
            return FlashResult(FlashResultType::ERASE, sector);
        }
        auto header = FlashHeader(kInitialCrc32, kFlashEmpty16);
        rc = _spi_flash_write(FlashResult::headerOffset(sector), reinterpret_cast<uint32_t *>(&header), sizeof(header));
        if (rc != SPI_FLASH_RESULT_OK) {
            __LDBG_printf("write failed offset=0x%08x", FlashResult::headerOffset(sector));
            return FlashResult(FlashResultType::WRITE, sector);
        }
        __LDBG_printf("init done sector=0x%04x", sector);
        return FlashResult(sector, header, 0);
    }

    FlashResult FlashStorage::copy(uint16_t fromSector, uint16_t toSector) const
    {
        // read header
        auto header = FlashHeader();
        auto rc = _spi_flash_read(FlashResult::headerOffset(fromSector), reinterpret_cast<uint32_t *>(&header), sizeof(header));
        if (rc != SPI_FLASH_RESULT_OK) {
            __LDBG_printf("read failed offset=0x%08x", FlashResult::headerOffset(fromSector));
            return FlashResult(FlashResultType::READ, fromSector);
        }
        // create new sector
        auto result = init(toSector);
        if (!result) {
            return result;
        }
        // copy data if it exists
        if (header && !header.isEmpty()) {
            if (!_copySector(fromSector, toSector, header.size(), result)) {
                return result;
            }
        }
        return result;
    }

    bool FlashStorage::append(const uint32_t *data, uint16_t size, FlashResult &result) const
    {
        return _copy(data, FlashResult::dataOffset(result._sector) + result._size, size, result);
    }

    bool FlashStorage::_copySector(uint16_t fromSector, uint16_t toSector, uint16_t size, FlashResult &result) const
    {
        if (!result._header.isEmpty()) {
            result = FlashResult(FlashResultType::FINALIZED, result._sector);
            __LDBG_printf("sector finalized sector=%u crc=0x%08x size=%u", result._sector, result._header._crc32, result._header._size);
            return false;
        }
        uint32_t srcAddr = FlashResult::dataOffset(fromSector);
        uint32_t dstAddr = FlashResult::dataOffset(toSector) + result._size;
        uint32_t buf[kBufferSize / 4];
        while(size) {
            auto read = std::min<uint16_t>(kBufferSize, size);
            auto rc = _spi_flash_read(srcAddr, buf, read);
            if (rc != SPI_FLASH_RESULT_OK) {
                result = FlashResult(FlashResultType::READ, srcAddr / SPI_FLASH_SEC_SIZE);
                __LDBG_printf("read error offset=0x%08x size=%u", srcAddr, read);
                return false;
            }
            if (!_copy(buf, dstAddr, read, result)) {
                return false;
            }
            size -= read;
            srcAddr += read;
            dstAddr += read;
        }
        return true;
    }

    bool FlashStorage::_copy(const uint32_t *data, uint32_t dstAddr, uint16_t size, FlashResult &result) const
    {
        if (!result._header.isEmpty()) {
            result = FlashResult(FlashResultType::FINALIZED, result._sector);
            __LDBG_printf("sector finalized sector=%u crc=0x%08x size=%u", result._sector, result._header._crc32, result._header._size);
            return false;
        }
        if (size + result._size > kSectorMaxSize) {
            result = FlashResult(FlashResultType::NO_SPACE, result._sector);
            __LDBG_printf("no space left sector=0x%04x size=%u", result._sector, size);
            return false;
        }
        auto rc = _spi_flash_write(dstAddr, const_cast<uint32_t *>(data), size);
        if (rc != SPI_FLASH_RESULT_OK) {
            result = FlashResult(FlashResultType::WRITE, result._sector);
            __LDBG_printf("write failed offset=0x%08x size=%u", dstAddr, size);
            return false;
        }
        result._crc = crc32(data, size, result._crc);
        result._size += size;
        return true;
    }

    bool FlashStorage::_finalize(FlashResult &result) const
    {
        if (!result._header.isEmpty()) {
            __LDBG_printf("sector finalized sector=%u crc=0x%08x size=%u", result._sector, result._header._crc32, result._header._size);
            result = FlashResult(FlashResultType::FINALIZED, result._sector);
            return false;
        }
        // finalize by copying size, crc and writing it to the flash sector
        result._header._crc32 = result._crc;
        result._header._size = result._size;
        if (result._header._size > kSectorMaxSize) {
            result = FlashResult(FlashResultType::OUT_OF_RANGE, result._sector);
            __LDBG_printf("size %u > max size=%u", result._header._size, kSectorMaxSize);
            return false;
        }
        // auto rc = _spi_flash_write(
        //     FlashResult::headerOffset(result._sector) + offsetof(FlashHeader, _crc32),
        //     reinterpret_cast<uint32_t *>(&result._header._crc32),
        //     sizeof(result._header) - offsetof(FlashHeader, _crc32)
        // );
        auto rc = _spi_flash_write(
            FlashResult::headerOffset(result._sector),
            reinterpret_cast<uint32_t *>(&result._header),
            sizeof(result._header)
        );
        __LDBG_printf("finalize sector=%04x size=%u crc=0x%08x update_header=%u", result._sector, result._header._size, result._header._crc32, rc);
        if (rc != SPI_FLASH_RESULT_OK) {
            result = FlashResult(FlashResultType::WRITE, result._sector);
            __LDBG_printf("write failed offset=0x%08x size=%u", result._sector * SPI_FLASH_SEC_SIZE + sizeof(result._header._magic), sizeof(result._header) - sizeof(result._header._magic));
            return false;
        }
#if DEBUG_RD_SPIFLASH
        {
            auto header = FlashHeader();
            rc = _spi_flash_read(FlashResult::headerOffset(result._sector),  reinterpret_cast<uint32_t *>(&header), sizeof(header));
            if (rc != SPI_FLASH_RESULT_OK || memcmp(&header, &result._header, sizeof(header)) != 0) {
                __LDBG_printf("written = %s", printable_string(&result._header, sizeof(result._header)).c_str());
                __LDBG_printf("read    = %s", printable_string(&header, sizeof(header)).c_str());

            }
        }
#endif
        result._result = FlashResultType::SUCCESS;
        return true;
    }

    FlashResult FlashStorage::_validate(const FlashResult &result) const
    {
        if (result._result != FlashResultType::SUCCESS) {
            __LDBG_printf("result type is not SUCCESS=%u", result._result);
            return result;
        }
        FlashHeader header;
        auto rc = _spi_flash_read(result._sector * SPI_FLASH_SEC_SIZE, reinterpret_cast<uint32_t *>(&header), sizeof(header));
        if (rc != SPI_FLASH_RESULT_OK) {
            __LDBG_printf("read failed offset=0x%08x size=%u", result._sector * SPI_FLASH_SEC_SIZE, sizeof(header));
            return FlashResult(FlashResultType::VALIDATE_READ, result._sector);
        }
        if(!header || header.isEmpty()) {
            __LDBG_printf("empty sector sector=0x%04x", result._sector);
            return FlashResult(FlashResultType::VALIDATE_EMPTY, result._sector);
        }
        if (header._size > kSectorMaxSize) {
            __LDBG_printf("size %u > max size=%u", header._size, kSectorMaxSize);
            return FlashResult(FlashResultType::OUT_OF_RANGE, result._sector);
        }
        auto crc = _crc(FlashResult::dataOffset(result._sector), header._size);
        __LDBG_printf("validate sector=0x%04x offset=0x%08x size=%u crc=0x%08x header.crc=0x%08x", result._sector, FlashResult::dataOffset(result._sector), header._size, crc, header._crc32);
        if (crc != header._crc32) {
            __LDBG_printf("crc failed offset=0x%08x size=%u crc=0x%08x",FlashResult::dataOffset(result._sector), header._size, header._crc32);
            return FlashResult(FlashResultType::VALIDATE_CRC, result._sector);
        }
        return FlashResult(result._sector, header, header._size, header._crc32);
    }

    uint32_t FlashStorage::_crc(uint32_t srcAddr, uint16_t size, uint32_t crc) const
    {
        if (size > kSectorMaxSize) {
            __LDBG_printf("size %u > max size=%u", size, kSectorMaxSize);
            return kInitialCrc32;
        }
        uint32_t buf[kBufferSize / 4];
        while(size) {
            auto read = std::min<uint16_t>(kBufferSize, size);
            auto rc = _spi_flash_read(srcAddr, buf, read);
            if (rc != SPI_FLASH_RESULT_OK) {
                __LDBG_printf("read failed offset=0x%08x size=%u", srcAddr, read);
                return kInitialCrc32;
            }
            crc = crc32(&buf, read, crc);
            size -= read;
            srcAddr += read;
        }
        return crc;
    }

    bool FlashStorage::format() const
    {
        FlashHeader hdr;
        for(auto i = _firstSector; i <= _lastSector; i++) {
            auto rc = _spi_flash_read(FlashResult::headerOffset(i), reinterpret_cast<uint32_t *>(&hdr), sizeof(hdr));
            if (rc != SPI_FLASH_RESULT_OK) {
                __LDBG_printf("read failed offset=0x%08x size=%u", FlashResult::headerOffset(i), sizeof(hdr));
                return false;
            }
            // erase only if the magic can be found or the sector contains any data
            if (hdr._magic == kFlashMagic) {

                memset(&hdr, 0, sizeof(hdr));
                // try to overwrite the flash memory
                rc = _spi_flash_write(FlashResult::headerOffset(i), reinterpret_cast<uint32_t *>(&hdr), sizeof(hdr));
                if (rc == SPI_FLASH_RESULT_OK) {
                    rc = _spi_flash_read(FlashResult::headerOffset(i), reinterpret_cast<uint32_t *>(&hdr), sizeof(hdr));
                    if (rc == SPI_FLASH_RESULT_OK && hdr._magic != kFlashMagic) {
                        continue;
                    }
                }

                // erase entire sector if the magic cannot be erased
                rc = _spi_flash_erase_sector(i);
                if (rc != SPI_FLASH_RESULT_OK) {
                    __LDBG_printf("erase failed sector=0x%04x failed", i);
                    return false;
                }
            }
        }
        return true;
    }


}
