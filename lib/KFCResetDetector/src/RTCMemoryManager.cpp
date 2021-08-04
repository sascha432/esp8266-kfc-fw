/**
* Author: sascha_lammers@gmx.de
*/

/*
+rtcm=set,0x11,0x01014161,0x02024262,0x03034363
+rtcm=set,22,0x4464
+rtcm=set,33,0x4565

+rtcm=get,0x11
+rtcm=clr
+rtcm=dump
+rtcm=qc

+rtcm=clr
+rtcm=set,0x11,0x11111111
+rtcm=set,0x22,0x22222222
+rtcm=set,0x33,0x33333333
+rtcm=set,0x44,0x44444444
+rtcm=set,0x55,0x55555555
+rtcm=set,0x66,0x66666666
+rtcm=dump


*/

#include "RTCMemoryManager.h"
#include <crc16.h>
#include <DumpBinary.h>
#include <Buffer.h>

#if DEBUG_RTC_MEMORY_MANAGER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

#include "user_interface.h"
#include "coredecls.h"

#if ENABLE_DEEP_SLEEP
namespace DeepSleep {
    extern "C" uint64_t _realTimeOffset;
}
#endif


#if RTC_SUPPORT == 0
RTCMemoryManager::RtcTimer RTCMemoryManager::_rtcTimer;
#endif

namespace RTCMemoryManagerNS {

#if defined(ESP8266)

#if DEBUG_RTC_MEMORY_MANAGER

    bool system_rtc_mem_read(uint8_t ofs, void *data, uint16_t len) {
        if (!((ofs * 4) >= 256 && (ofs * 4 + len) <= 512 && len != 0)) {
            __DBG_panic("system_rtc_mem_read ofs=%u len=%d %d-%d", ofs, len, ofs*4, ofs*4+len);
        }
        auto result = ::system_rtc_mem_read(ofs, data, len);
        // __DBG_printf("rtc_read result=%u ofs=%u length=%u data=%p", result, ofs, len, data);
        // PrintString str;
        // DumpBinary dumper(str);
        // dumper.setPerLine(len);
        // dumper.setGroupBytes(4);
        // dumper.dump(data, len);
        // __DBG_printf("rtc_read result=%u ofs=%u length=%u data=%s", result, ofs, len, str.c_str());
        return result;
    }

    bool system_rtc_mem_write(uint8_t ofs, const void *data, uint16_t len) {
        if (!((ofs * 4) >= 256 && (ofs * 4 + len) <= 512 && len != 0)) {
            __DBG_panic("system_rtc_mem_write ofs=%u len=%d %d-%d", ofs, len, ofs*4, ofs*4+len);
        }
        auto result = ::system_rtc_mem_write(ofs, data, len);
        // __DBG_printf("rtc_write result=%u ofs=%u length=%u data=%p", result, ofs, len, data);
        // PrintString str;
        // DumpBinary dumper(str);
        // dumper.setPerLine(len);
        // dumper.setGroupBytes(4);
        // dumper.dump(data, len);
        // __DBG_printf("rtc_write result=%u ofs=%u length=%u data=%s", result, ofs, len, str.c_str());
        return result;
    }

#else

    inline static bool system_rtc_mem_read(uint8_t ofs, void *data, uint16_t len) {
        __DBG_assert_printf(((ofs * 4) >= 256 && (ofs * 4 + len) <= 512 && len != 0), "read OOB ofs=%d len=%d", ofs, len);
        return ::system_rtc_mem_read(ofs, data, len);
    }

    inline static bool system_rtc_mem_write(uint8_t ofs, const void *data, uint16_t len) {
        __DBG_assert_printf(((ofs * 4) >= 256 && (ofs * 4 + len) <= 512 && len != 0), "write OOB ofs=%d len=%d", ofs, len);
        return ::system_rtc_mem_write(ofs, data, len);
    }

#endif

#elif defined(ESP32)

RTC_NOINIT_ATTR uint8_t RTCMemoryManager_allocated_block[RTCMemoryManager::__memorySize];

    bool system_rtc_mem_read(size_t ofs, void *data, size_t len) {
        memcpy(data, RTCMemoryManager_allocated_block + ofs * RTCMemoryManager::kBlockSize, len);
        return true;
    }

    bool system_rtc_mem_write(size_t ofs, void *data, size_t len) {
        memcpy(RTCMemoryManager_allocated_block + ofs * RTCMemoryManager::kBlockSize, data, len);
        return true;
    }

#endif

}

bool RTCMemoryManager::_readHeader(RTCMemoryManager::Header_t &header) {

    if (RTCMemoryManagerNS::system_rtc_mem_read(kHeaderAddress, &header, sizeof(header))) {
        if (header.length <= kMemoryLimit) {
            return true;
        }
        __LDBG_printf("invalid length=%d limit=%d max=%d", header.length, kMemoryLimit, kMemorySize - header.length);
    }
    else {
        __LDBG_printf("read error ofs=%d size=%d", kHeaderAddress, sizeof(header));
    }
    return false;
}

uint8_t *RTCMemoryManager::_readMemory(Header_t &header, uint16_t extraSize) {

    uint8_t *buf = nullptr;

    while (_readHeader(header)) {
#if defined(ESP32)
        // we can use the RTC memory directly
        memPtr = reinterpret_cast<uint32_t *>(RTCMemoryManager_allocated_block + offset);
        uint16_t crc = crc16_calc((const uint8_t *)memPtr, header.length + sizeof(header) - sizeof(header.crc));
        if (crc != header.crc) {
            __LDBG_printf("CRC mismatch %04x != %04x, length %d", crc, header.crc, header.length);
            return nullptr;
        }
#else
        auto minSize = header.crc_length() + extraSize;
        auto size = minSize;
        if (kBlockSize > 1) {
            auto alignment = size % kBlockSize;
            if (alignment) {
                size += kBlockSize - alignment;
            }
        }
        if (size > kMemorySize) {
            __DBG_printf("malloc failed length=%u max_size=%u", size, kMemorySize);
            break;
        }
        // auto buf = reinterpret_cast<uint8_t *>(calloc(size, 1));
        auto buf = new uint8_t[size]();
        if (!buf) {
            __DBG_printf("malloc failed length=%u", size);
            break;
        }
        __LDBG_printf("allocate size=%u aligned=%u", minSize, size);
        uint16_t crc = static_cast<uint16_t>(~0U);
        if (!RTCMemoryManagerNS::system_rtc_mem_read(header.start_address(), buf, header.crc_length())) {
            __LDBG_printf("failed to read data adrress=%u size=%u", header.start_offset(), header.crc_length());
            break;
        }
        if ((crc = crc16_update(buf, header.crc_length())) != header.crc) {
            __LDBG_printf("CRC mismatch %04x != %04x, size=%u crclen=%u", crc, header.crc, header.length, header.crc_length());
            break;
        }
#endif
        __LDBG_printf("read: address=%u[%u-%u] length=%u crc=0x%04x", header.start_address(), kBaseAddress, kLastAddress, header.length, header.crc);
        return buf;
    }
    release(buf);
    return nullptr;
}

uint8_t RTCMemoryManager::read(RTCMemoryId id, void *dataPtr, uint8_t maxSize)
{
    Header_t header __attribute__((aligned(4)));
    Entry_t entry;
    uint8_t *data;
    std::fill_n(reinterpret_cast<uint8_t *>(dataPtr), maxSize, 0);
    auto memPtr = _read(data, header, entry, id);
    if (!memPtr) {
        __LDBG_printf("read = nullptr");
        return 0;
    }
    auto copyLen = std::min(entry.length, maxSize);
    memcpy(dataPtr, data, copyLen);
    release(memPtr);
    return copyLen;
}

uint8_t *RTCMemoryManager::read(RTCMemoryId id, uint8_t &length)
{
    Header_t header __attribute__((aligned(4)));
    Entry_t entry;
    uint8_t *data;
    auto memPtr = _read(data, header, entry, id);
    if (!memPtr) {
        length = 0;
        __LDBG_printf("read = nullptr");
        return nullptr;
    }
    memmove(memPtr, data, entry.length);
    length = entry.length;
    return memPtr;
}

void RTCMemoryManager::release(uint8_t *buffer)
{
    if (buffer) {
        delete[] buffer;
    }
}

uint8_t *RTCMemoryManager::_read(uint8_t *&data, Header_t &header, Entry_t &entry, RTCMemoryId id)
{
    auto memPtr = _readMemory(header, 0);
    if (!memPtr) {
        __LDBG_printf("read = nullptr");
        return nullptr;
    }
    auto ptr = header.begin(memPtr);
    auto endPtr = header.end(memPtr);
    while(ptr + sizeof(Entry_t) < endPtr) {
        entry = Entry_t(ptr);
        if (!entry) {
            __LDBG_printf("read eof id=0x00 offset=%u", header.distance(memPtr, ptr));
            break;
        }
        ptr += sizeof(entry);
        if (ptr >= endPtr) {
            __LDBG_printf("read eof id=0x%02x length=%d offset=%u", entry.mem_id, entry.length, header.distance(memPtr, ptr));
            break;
        }
        __LDBG_printf("read id=%u len=%u", entry.mem_id, entry.length);
        if (entry.mem_id == static_cast<decltype(entry.mem_id)>(id)) {
            data = ptr;
            return memPtr;
        }
        ptr += entry.length;
    }
    release(memPtr);
    return nullptr;
}

bool RTCMemoryManager::write(RTCMemoryId id, const void *dataPtr, uint8_t dataLength)
{
    if (id == RTCMemoryId::NONE) {
        dataLength = 0;
    }
    __LDBG_printf("write id=%u data=%p len=%u", id, dataPtr, dataLength);

    Header_t header __attribute__((aligned(4)));
    uint16_t newLength = sizeof(header);
    uint8_t *outPtr;
    auto memUnqiuePtr = std::unique_ptr<uint8_t[]>(_readMemory(header, dataLength));
    auto memPtr = memUnqiuePtr.get();

    // auto memPtr = _readMemory(header, dataLength);
    if (memPtr) {
        // copy existing items
        auto ptr = header.begin(memPtr);
        auto endPtr = header.end(memPtr);
        outPtr = memPtr;
        while(ptr + sizeof(Entry_t) < endPtr) {
            Entry_t entry(ptr);
            __LDBG_printf("read id=%u len=%u distance=%d", entry.mem_id, entry.length, header.distance(memPtr, ptr));
            if (!entry) { // invalid id, NUL bytes for alignment
                __LDBG_printf("id 0x00 distance=%d", header.distance(memPtr, ptr));
                break;
            }
            ptr += sizeof(entry);
            if (ptr >= endPtr) {
                __LDBG_printf("memory full id=0x%02x length=%d distance=%u", entry.mem_id, entry.length, header.distance(memPtr, ptr));
                return false;
            }
            if (entry.mem_id != static_cast<decltype(entry.mem_id)>(id)) {
                memcpy(outPtr, &entry, sizeof(entry));
                outPtr += sizeof(entry);
                memmove(outPtr, ptr, entry.length);
                outPtr += entry.length;
                newLength += entry.length + sizeof(entry);
                __LDBG_printf("copy id=%u len=%u distance=%d nl=%u", entry.mem_id, entry.length, header.distance(outPtr, ptr), newLength);
            }
            else {
                __LDBG_printf("skip id=%u len=%u distance=%u nl=%u", entry.mem_id, entry.length, header.distance(outPtr, ptr), newLength);
            }
            ptr += entry.length;
        }
        __LDBG_printf("rewritten header=%u new_header=%u", header.length, newLength);
        header.length = newLength;
    }
    else {
        auto size = dataLength + kBlockSize + sizeof(header) + sizeof(Entry_t);
        if (size > kMemorySize) {
            __DBG_printf("malloc failed length=%u max_size=%u", size, kMemorySize);
            return false;
        }
        memUnqiuePtr.reset(new uint8_t[size]());
        memPtr = memUnqiuePtr.get();
        if (!memUnqiuePtr) {
            __DBG_printf("malloc failed length=%u", size);
            return false;
        }
        outPtr = memPtr;
        header.crc = static_cast<uint16_t>(~0U);
    }

    if (dataLength) {
        // append new data
        Entry_t entry(id, dataLength);
        __LDBG_printf("new id=%u len=%u", entry.mem_id, entry.length);

        memcpy(outPtr, &entry, sizeof(entry));
        outPtr += sizeof(entry);
        memcpy(outPtr, dataPtr, entry.length);
        outPtr += entry.length;

        newLength += entry.length + sizeof(entry);
    }
    header.length = newLength;

    __LDBG_printf("unaligned length=%u", header.length);

    // align before adding header
    while (!_isAligned(header.length)) {
        *outPtr++ = 0;
        header.length++;
    }
    header.crc = static_cast<uint16_t>(~0U);

    __LDBG_printf("aligned length=%u", header.length);
    memcpy(outPtr, &header, sizeof(header));

    // update CRC in newData and store
    header.crc = *reinterpret_cast<uint16_t *>(outPtr + offsetof(Header_t, crc)) = crc16_update(memPtr, header.crc_length());

    __LDBG_printf("write: address=%u[%u-%u] length=%u crc=0x%04x", header.start_address(), kBaseAddress, kLastAddress, header.length, header.crc);
    return RTCMemoryManagerNS::system_rtc_mem_write(header.start_address(), reinterpret_cast<uint32_t *>(memPtr), header.length);
}

	bool RTCMemoryManager::clear() {
#if defined(ESP8266)

    // clear 16 blocks including header
    static constexpr auto kNumBlocks = 16;
    static constexpr auto kAddress = (kMemorySize - (kNumBlocks * kBlockSize)) / kBlockSize;
    uint32_t data[kNumBlocks];
    memset(data, 0xff, sizeof(data));
    __LDBG_printf("clear: address=%u[%u-%u] length=%u", kBaseAddress + kAddress, kBaseAddress, kLastAddress, sizeof(data));
    if (!RTCMemoryManagerNS::system_rtc_mem_write(kBaseAddress + kAddress, &data, sizeof(data))) {
        return false;
    }

    return write(RTCMemoryId::NONE, nullptr, 0);

#elif defined(ESP32)

    // clear entire block
    memset(RTCMemoryManager_allocated_block, 0xff, sizeof(RTCMemoryManager_allocated_block));
    return true;

#endif
}

#if DEBUG

#include "plugins.h"

bool RTCMemoryManager::dump(Print &output, RTCMemoryId displayId) {

    Header_t header __attribute__((aligned(4)));
    auto memPtr = _readMemory(header, 0);
    if (!memPtr) {
        output.println(F("RTC data not set or invalid"));
        return false;
    }

#if RTC_SUPPORT == 0
    output.printf_P(PSTR("RTC memory time: %u\n"), RTCMemoryManager::readTime().getTime());
#endif

    output.printf_P(PSTR("RTC data length: %u\n"), header.data_length());

    bool result = false;
    DumpBinary dumper(output);
    auto ptr = header.begin(memPtr);
    auto endPtr = header.end(memPtr);
    while(ptr + sizeof(Entry_t) < endPtr) {
        Entry_t entry(ptr);
        __LDBG_printf("read id=%u len=%u", entry.mem_id, entry.length);
        if (!entry) {
            break;
        }
#if DEBUG_RTC_MEMORY_MANAGER
        {
            PrintString out;
            DumpBinary dumper(out);
            dumper.dump(ptr + sizeof(entry), entry.length);
            dumper.setPerLine(entry.length);
            dumper.setGroupBytes(4);
            __LDBG_printf("rtcm=%d id=0x%02x length=%u data=%s", header.distance(memPtr, ptr), entry.mem_id, entry.length, out.c_str());
        }
#endif
        ptr += sizeof(entry);
        if (ptr >= endPtr) {
            __LDBG_printf("entry length exceeds total size. id=0x%02x entry_length=%u size=%u", entry.mem_id, entry.length, header.length);
            break;
        }
        if (displayId == RTCMemoryId::NONE || displayId == static_cast<RTCMemoryId>(entry.mem_id)) {
            if (entry.length) {
                result = true;
            }
#if HAVE_KFC_PLUGINS
            output.printf_P(PSTR("id: 0x%02x (%s), length %d "), entry.mem_id, PluginComponent::getMemoryIdName(entry.mem_id), entry.length);
#else
            output.printf_P(PSTR("id: 0x%02x, length %d "), entry.mem_id, entry.length);
#endif
            dumper.setGroupBytes(4);
            dumper.setPerLine(entry.length);
            dumper.dump(ptr, entry.length);
        }
        ptr += entry.length;
    }
    release(memPtr);
    return result;
}

#endif

RTCMemoryManager::RtcTime RTCMemoryManager::_readTime()
{
    RtcTime time;
#if RTC_SUPPORT == 0
    if (read(RTCMemoryId::RTC, &time, sizeof(time)) == sizeof(time)) {
        __LDBG_printf("read time=%u status=%s", time.getTime(), time.getStatus());
        return time;
    }
    __DBG_printf("invalid RtcTime");
#endif
    return RtcTime();
}

void RTCMemoryManager::_writeTime(const RtcTime &time)
{
#if RTC_SUPPORT == 0
    __LDBG_printf("write time=%u status=%s", time.getTime(), time.getStatus());
    write(RTCMemoryId::RTC, &time, sizeof(time));
#endif
}

#if RTC_SUPPORT == 0

void RTCMemoryManager::setupRTC()
{
    _rtcTimer.startTimer(1000, true);
}

void RTCMemoryManager::updateTimeOffset(uint32_t offset)
{
    __LDBG_printf("update time offset=%ums", offset);
    offset /= 1000;
    if (offset) {
        auto rtc = _readTime();
        rtc.time += offset;
        _writeTime(rtc);
    }
}

#endif

void RTCMemoryManager::_clearTime()
{
    __LDBG_print("clearTime");
    remove(RTCMemoryId::RTC);
}
