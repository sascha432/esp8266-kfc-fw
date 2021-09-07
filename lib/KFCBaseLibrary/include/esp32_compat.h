/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if defined(ESP32)

#if USE_LITTLEFS
#    include <LittleFS.h>
#else
#    include "../src/SPIFFS.h"
#    include <FS.h>
#endif

#include <esp_timer.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <esp_sntp.h>
#include <sdkconfig.h>
#include <sys/queue.h>
#include <freertos/portmacro.h>

extern "C" {
    #include <sys/unistd.h>
    #include <sys/stat.h>
    #include <dirent.h>
}
#include <FSImpl.h>

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


struct esp_timer {
    uint64_t alarm;
    uint64_t period:56;
    uint64_t flags:8;
    union {
        esp_timer_cb_t callback;
        uint32_t event_id;
    };
    void* arg;
#if WITH_PROFILING
    const char* name;
    size_t times_triggered;
    size_t times_armed;
    size_t times_skipped;
    uint64_t total_callback_run_time;
#endif // WITH_PROFILING
    LIST_ENTRY(esp_timer) list_entry;
};

typedef esp_timer_cb_t os_timer_func_t_ptr;
typedef struct esp_timer os_timer_t;

inline bool wifi_get_country(wifi_country_t *country)
{
    return esp_wifi_get_country(country) == ESP_OK;
}

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

#define WiFi_isHidden(num) (WiFi.SSID(num).length() == 0)

namespace fs {

    // ESP8266 compat
    class Dir : public fs::FS {
    public:
        Dir() :
            FS(nullptr),
            _dir(nullptr),
            _entry(nullptr)
        {
        }

        Dir(const String &path) :
            Dir(path.c_str())
        {
        }

        Dir(const char *path) :
            #if USE_LITTLEFS
                FS(LittleFS),
            #else
                FS(SPIFFS),
            #endif
            _path(_impl->mountpoint()),
            _entry(nullptr),
            _stats({})
        {
            if (!_path.endsWith('/') && *path != '/') {
                _path += '/';
            }
            // start of the path without the mount point
            _pathStart = _path.length();
            _path += path;
            if (!_path.endsWith('/')) {
                _path += '/';
            }
            _dir = opendir(_path.c_str());
        }

        ~Dir() {
            if (_dir) {
                closedir(_dir);
            }
        }

        operator bool() const {
            return _dir != nullptr && _entry != nullptr;
        }

        bool rewind() {
            _reset();
            if (!_dir) {
                return false;
            }
            rewinddir(_dir);
            return true;
        }

        bool rewindDirectory() {
            return rewind();
        }

        bool next() {
            _reset();
            if (!_dir) {
                return false;
            }
            _entry = readdir(_dir);
            if (_entry) {
                _fullname = _path.c_str();
                _fullname += _entry->d_name;
                return true;
            }
            return false;
        }

        size_t fileSize() {
            if (!isFile()) {
                return 0;
            }
            if (_getStat()) {
                return _stats.st_size;
            }
            return 0;
        }

        size_t size() {
            return fileSize();
        }

        time_t fileTime() {
            if (_getStat()) {
                return _stats.st_mtim.tv_sec;
            }
            return 0;
        }

        time_t dirTime() {
            return fileTime();
        }

        bool isDirectory() const {
            if (!_entry) {
                return false;
            }
            return _entry->d_type == DT_DIR;
        }

        bool isFile() const {
            if (!_entry) {
                return false;
            }
            return !isDirectory();
        }

        const char *fullName() const {
            if (!_entry) {
                return nullptr;
            }
            return _fullname.c_str() + _pathStart;
        }

        const char *name() const {
            if (!_entry) {
                return nullptr;
            }
            return _entry->d_name;
        }

        const char *path() const {
            if (!_entry) {
                return nullptr;
            }
            return _path.c_str() + _pathStart;
        }

        const char *fileName() const {
            return name();
        }

        File openFile(const char *mode) {
            if (!isFile()) {
                return File();
            }
            return open(fullName(), mode);
        }

    private:
        bool _getStat() {
            // stats cached
            if (_stats.st_mode) {
                return true;
            }
            if (stat(_fullname.c_str(), &_stats) == 0) {
                // Serial.printf("filename=%s uid=%u dev=%u ctime=%u mtime=%u size=%d ino=%d mode=%u\n",
                //     _fullname.c_str(), _stats.st_uid, _stats.st_dev, (uint32_t)_stats.st_ctim.tv_sec, (uint32_t)_stats.st_mtim.tv_sec,
                //     (int)_stats.st_size, _stats.st_ino, _stats.st_mode
                // );
                return true;
            }
            _stats.st_mode = 0;
            return false;
        }

        void _reset() {
            // remove filename
            _fullname = String();
            // reset stats
            _stats.st_mode = 0;
            // remove entry
            _entry = nullptr;
        }

    private:
        String _path;
        size_t _pathStart;
        String _fullname;
        DIR *_dir;
        struct dirent *_entry;
        struct stat _stats;
    };

};

#ifndef FS_NO_GLOBALS
using fs::Dir;
#endif

inline void ets_timer_arm_new(ETSTimer *timer, uint32_t tmout, bool repeat, bool millis)
{
    if (millis) {
        ets_timer_arm(timer, tmout, repeat);
    }
    else {
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

using settimeofday_cb_t = sntp_sync_time_cb_t;
using settimeofday_cb_args_t = struct timeval *;

inline void settimeofday_cb(settimeofday_cb_t cb)
{
    sntp_set_time_sync_notification_cb(cb);
}

extern "C" {

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
