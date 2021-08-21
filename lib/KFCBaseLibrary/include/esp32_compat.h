/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if defined(ESP32)

#if USE_LITTLEFS
#include <LittleFS.h>
#else
#include "../src/SPIFFS.h"
#include <FS.h>
#endif

#include <esp_timer.h>
#include <esp_err.h>

#ifndef ARDUINO_ESP32_RELEASE
#include <esp_arduino_version.h>
#define ARDUINO_ESP32_RELEASE _STRINGIFY(ESP_ARDUINO_VERSION_MAJOR) "." _STRINGIFY(ESP_ARDUINO_VERSION_MINOR) "." _STRINGIFY(ESP_ARDUINO_VERSION_PATCH)
#endif

#define isFlashInterfacePin(p) false

#if ESP32
#define U_FS U_SPIFFS
#endif

// TODO
enum rst_reason {
    REASON_DEFAULT_RST      = 0,    /* normal startup by power on */
    REASON_WDT_RST          = 1,    /* hardware watch dog reset */
    REASON_EXCEPTION_RST    = 2,    /* exception reset, GPIO status won’t change */
    REASON_SOFT_WDT_RST     = 3,    /* software watch dog reset, GPIO status won’t change */
    REASON_SOFT_RESTART     = 4,    /* software restart ,system_restart , GPIO status won’t change */
    REASON_DEEP_SLEEP_AWAKE = 5,    /* wake up from deep-sleep */
    REASON_EXT_SYS_RST      = 6     /* external system reset */
};

struct rst_info {
    uint32_t reason;
    uint32_t exccause;
    uint32_t epc1;
    uint32_t epc2;
    uint32_t epc3;
    uint32_t excvaddr;
    uint32_t depc;
};

inline uint64_t micros64()
{
    return esp_timer_get_time();
}

inline void *memmove_P(void *dst, const void *src, size_t len)
{
    return memmove(dst, src, len);
}

typedef esp_timer_cb_t os_timer_func_t_ptr;
typedef struct esp_timer os_timer_t;

#ifndef OS_TIMER_DEBUG
#   define OS_TIMER_DEBUG 0
#endif

#include <esp_wifi.h>

inline bool wifi_get_country(wifi_country_t *country) {
    return esp_wifi_get_country(country) == ESP_OK;
}

#include <esp_wifi_types.h>

using WiFiMode = wifi_mode_t;

typedef wifi_err_reason_t WiFiDisconnectReason;

enum RFMode {
    RF_DEFAULT = 0
};

#ifndef WL_MAC_ADDR_LENGTH
#   define WL_MAC_ADDR_LENGTH 6
#endif

#define WIFI_DISCONNECT_REASON_AUTH_EXPIRE WIFI_REASON_AUTH_EXPIRE

typedef struct {
    String ssid;
    uint8_t bssid[WL_MAC_ADDR_LENGTH];
    uint8_t channel;
} WiFiEventStationModeConnected;

typedef struct {
    String ssid;
    uint8_t bssid[WL_MAC_ADDR_LENGTH];
    WiFiDisconnectReason reason;
} WiFiEventStationModeDisconnected;

typedef struct {
    IPAddress ip;
    IPAddress mask;
    IPAddress gw;
} WiFiEventStationModeGotIP;

typedef struct {
    uint8_t aid;
    uint8_t mac[WL_MAC_ADDR_LENGTH];
} WiFiEventSoftAPModeStationConnected;

typedef struct {
    uint8_t aid;
    uint8_t mac[WL_MAC_ADDR_LENGTH];
} WiFiEventSoftAPModeStationDisconnected;

#define WiFi_isHidden(num)                  (WiFi.SSID(num).length() == 0)

namespace fs {

    class Dir : public File {
    public:
        using File::File;

        Dir(const File &file) : _file(file) {
        }
        Dir() {
        }

        bool rewind() {
            rewindDirectory();
            return true;
        }

        bool next() {
            return false;
        }

        const char *fileName() const {
            return name();
        }

        size_t fileSize() const {
            return size();
        }

        bool isFile() {
            return !isDirectory();
        }

    private:
        File _file;
    };

};

using Dir = fs::Dir;

// namespace fs {

//     enum OpenMode {
//         OM_DEFAULT = 0,
//         OM_CREATE = 1,
//         OM_APPEND = 2,
//         OM_TRUNCATE = 4
//     };

//     enum AccessMode {
//         AM_READ = 1,
//         AM_WRITE = 2,
//         AM_RW = AM_READ | AM_WRITE
//     };

//     class DirImpl {
//     public:
//         virtual ~DirImpl() { }
//         virtual FileImplPtr openFile(OpenMode openMode, AccessMode accessMode) = 0;
//         virtual const char* fileName() = 0;
//         virtual size_t fileSize() = 0;
//         virtual bool isFile() const = 0;
//         virtual bool isDirectory() const = 0;
//         virtual bool next() = 0;
//         virtual bool rewind() = 0;
//     };


//     class Dir : public DirImpl {
//     public:
//         Dir() {
//         }
//         Dir(const File &root) {
//             _root = root;
//         }

//         virtual FileImplPtr openFile(OpenMode openMode, AccessMode accessMode) {
//             return FileImplPtr(nullptr);
//         }
//         virtual String fileName() {
//             return _file.name();
//         }
//         virtual size_t fileSize() {
//             return _file.size();
//         }
//         virtual bool isFile() const {
//             return false;
//         }
//         virtual bool isDirectory() const {
//             return false;
//         }
//         virtual bool next()  {
//             return false;
//         }
//         virtual bool rewind() {
//             return false;
//         }


//     private:
//         File _root;
//         File _file;
//     };

// };

// typedef fs::Dir Dir;

// typedef struct {
//     size_t totalBytes;
//     size_t usedBytes;
//     int blockSize;
//     int maxOpenFiles;
//     int maxPathLength;
//     int pageSize;
// } FSInfo;


// //TODO change to KFCFS.info()
// #define SPIFFS_info(info) \
//     memset(&info, 0, sizeof(info)); \
//     info.totalBytes = SPIFFS.totalBytes(); \
//     info.usedBytes = SPIFFS.usedBytes();


inline void ets_timer_arm_new(ETSTimer *timer, uint32_t tmout, bool repeat, bool millis) {
    if (millis) {
        ets_timer_arm(timer, tmout, repeat);
    } else {
        ets_timer_arm_us(timer, tmout, repeat);
    }
}

#define PWMRANGE 1023

void analogWrite(uint8_t pin, uint16_t value);

inline void panic() {
    for(;;) {
        *((int *) 0) = 0;
    }
}

using BoolCB = std::function<void(bool)>;
using TrivialCB = std::function<void()>;

void settimeofday_cb(const BoolCB &cb);
void settimeofday_cb(const TrivialCB &cb);

extern "C" {

    // emulation of callback
    void settimeofday_cb (void (*cb)(void));

    uint32_t crc32_le(uint32_t crc, uint8_t const *buf, uint32_t len);

    bool can_yield();

}

inline uint32_t crc32(const void *buf, size_t len, uint32_t crc = ~0)
{
    return crc32_le(crc, reinterpret_cast<const uint8_t *>(buf), len);
}

#define UART0    0
#define UART1    1
#define UART_NO -1

#define SPI_FLASH_RESULT_OK ESP_OK

using SpiFlashOpResult = esp_err_t;

#endif
