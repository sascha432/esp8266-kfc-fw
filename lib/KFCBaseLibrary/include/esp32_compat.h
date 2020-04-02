/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if defined(ESP32)

#include "../src/SPIFFS.h"
#include <FS.h>

#include <esp_timer.h>

typedef esp_timer_cb_t os_timer_func_t_ptr;
typedef struct esp_timer os_timer_t;

#ifndef OS_TIMER_DEBUG
#define OS_TIMER_DEBUG                      0
#endif

#include <esp_wifi.h>

inline bool wifi_get_country(wifi_country_t *country) {
    return esp_wifi_get_country(country) == ESP_OK;
}

#include <esp_wifi_types.h>

typedef wifi_err_reason_t WiFiDisconnectReason;

enum RFMode {
    RF_DEFAULT = 0
};

#ifndef WL_MAC_ADDR_LENGTH
#define WL_MAC_ADDR_LENGTH                  6
#endif

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

enum dhcp_status {
    DHCP_STOPPED,
    DHCP_STARTED
};

enum dhcp_status wifi_station_dhcpc_status(void);

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

#define SPIFFS_openDir(dirname)             Dir(SPIFFS.open(dirname))

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

typedef struct {
    size_t totalBytes;
    size_t usedBytes;
    int blockSize;
    int maxOpenFiles;
    int maxPathLength;
    int pageSize;
} FSInfo;


#define SPIFFS_info(info) \
    memset(&info, 0, sizeof(info)); \
    info.totalBytes = SPIFFS.totalBytes(); \
    info.usedBytes = SPIFFS.usedBytes();


inline void ets_timer_arm_new(ETSTimer *timer, uint32_t tmout, bool repeat, bool millis) {
    if (millis) {
        ets_timer_arm(timer, tmout, repeat);
    } else {
        ets_timer_arm_us(timer, tmout, repeat);
    }
}

void analogWrite(uint8_t pin, uint16_t value);

inline void panic() {
    for(;;) {
        *((int *) 0) = 0;
    }
}

// emulation of callback
extern "C" {
    void settimeofday_cb (void (*cb)(void));
}

#endif
