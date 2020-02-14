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

    int getFreeHeap() {
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

#if _DEBUG && _MSC_VER // MSVC memory debug
    void _enableMSVCMemdebug() {
        // Get current flag
        int tmpFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);

        // Turn on leak-checking bit.
        tmpFlag |= _CRTDBG_ALLOC_MEM_DF|_CRTDBG_DELAY_FREE_MEM_DF|_CRTDBG_CHECK_ALWAYS_DF|_CRTDBG_CHECK_CRT_DF|_CRTDBG_LEAK_CHECK_DF;

        // Turn off CRT block checking bit.
        //tmpFlag &= ~_CRTDBG_CHECK_CRT_DF;

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
};

extern EspClass ESP;


struct os_timer_t;

class ESPTimerThread {
public:
    typedef std::vector<os_timer_t *> TimerVector;

    ESPTimerThread() {
        _handle = nullptr;
    }
    virtual ~ESPTimerThread() {
        end();
    }

    void begin();
    void end();
    TimerVector &getVector();
    void run();

private:
    void _removeEndedTimers();

    HANDLE _handle;
    TimerVector _timers;
};

extern ESPTimerThread timerThread;


typedef void os_timer_func_t(void *timer_arg);

struct os_timer_t {
    int _interval;
    int _repeat;
    os_timer_func_t *_func;
    void *_arg;
    unsigned long _nextCall;
};

void os_timer_setfn(os_timer_t *timer, os_timer_func_t func, void *arg);
void os_timer_arm(os_timer_t *timer, int interval, int repeat);
void os_timer_disarm(os_timer_t *timer);
