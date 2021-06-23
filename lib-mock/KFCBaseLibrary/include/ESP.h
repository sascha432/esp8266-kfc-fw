/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

class EspClass {
public:
    EspClass() {
        memset(&resetInfo, 0, sizeof(resetInfo));
        _rtcMemory = nullptr;
        _readRtcMemory();
    }
    ~EspClass() {
        free(_rtcMemory);
    }

    bool rtcUserMemoryRead(uint32_t offset, uint32_t *data, size_t size)  {
        offset = (offset * rtcMemoryBlkSize) + rtcMemoryReserved;
        if (size + offset > rtcMemorySize) {
            return false;
        } else {
            memcpy(data, _rtcMemory + offset, size);
            return true;
        }
    }

    bool rtcUserMemoryWrite(uint32_t offset, uint32_t *data, size_t size)    {
        offset = (offset * rtcMemoryBlkSize) + rtcMemoryReserved;
        if (size + offset > rtcMemorySize) {
            return false;
        } else {
            memcpy(_rtcMemory + offset, data, size);
            _writeRtcMemory();
            return true;
        }
    }

    uint32_t getFreeHeap() {
        PROCESS_MEMORY_COUNTERS pmc;
        GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
        return pmc.WorkingSetSize;
    }

    void rtcMemDump();
    void rtcClear();

    String getResetReason(void) {
        return "Unknown";
    }

    String getResetInfo(void) {
        return "Info 0";
    }

    struct rst_info *getResetInfoPtr(void) {
        return resetInfo;
    }

    void wdtFeed() {}

#if _DEBUG && _MSC_VER // MSVC memory debug
    void _enableMSVCMemdebug() {
        // Get current flag
        int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

        // Turn on leak-checking bit.
        tmpFlag |= _CRTDBG_ALLOC_MEM_DF|_CRTDBG_DELAY_FREE_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_CHECK_CRT_DF|_CRTDBG_LEAK_CHECK_DF;

        // Turn off CRT block checking bit.
        tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;

        // Set flag to the new value.
        _CrtSetDbgFlag(tmpFlag);
    }
#else
    void _enableMSVCMemdebug() {}
#endif


private:
    void _readRtcMemory() {
        if (_rtcMemory) {
            free(_rtcMemory);
        }
        _rtcMemory = (uint8_t *)calloc(rtcMemorySize, 1);
        
        FILE* fp = fopen("rtcmemory.bin", "rb");
        if (fp) {
            fread(_rtcMemory, 1, rtcMemorySize, fp);
            fclose(fp);
        }
    }
    void _writeRtcMemory() {
        if (!_rtcMemory) {
            return;
        }
        FILE *fp = fopen("rtcmemory.bin", "wb");
        if (fp) {
            fwrite(_rtcMemory, 1, rtcMemorySize, fp);
            fclose(fp);
        }
    }

    static const uint16_t rtcMemoryReserved = 256;
    static const uint16_t rtcMemoryUser = 512;
    static const uint16_t rtcMemorySize = rtcMemoryReserved + rtcMemoryUser;
    static const uint8_t rtcMemoryBlkSize = 4;
public:
    uint8_t *_rtcMemory;

    struct rst_info *resetInfo;

public:
    static constexpr uint32_t kFlashStartAddress = 0x40200000U;
    static constexpr uint32_t kFlashEndAddress = 0x402FEFF0U;

    static bool flashEraseSector(uint32_t sector);
    static bool flashWrite(uint32_t address, const uint32_t *data, size_t size);
    static bool flashWrite(uint32_t address, const uint8_t *data, size_t size);
    static bool flashRead(uint32_t address, uint32_t *data, size_t size);
    static bool flashRead(uint32_t address, uint8_t *data, size_t size);

};

extern EspClass ESP;
