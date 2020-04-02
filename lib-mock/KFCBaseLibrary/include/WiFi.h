/**
* Author: sascha_lammers@gmx.de
*/

#pragma once


enum WiFiDisconnectReason
{
    WIFI_DISCONNECT_REASON_UNSPECIFIED              = 1,
    WIFI_DISCONNECT_REASON_AUTH_EXPIRE              = 2,
    WIFI_DISCONNECT_REASON_AUTH_LEAVE               = 3,
    WIFI_DISCONNECT_REASON_ASSOC_EXPIRE             = 4,
    WIFI_DISCONNECT_REASON_ASSOC_TOOMANY            = 5,
    WIFI_DISCONNECT_REASON_NOT_AUTHED               = 6,
    WIFI_DISCONNECT_REASON_NOT_ASSOCED              = 7,
    WIFI_DISCONNECT_REASON_ASSOC_LEAVE              = 8,
    WIFI_DISCONNECT_REASON_ASSOC_NOT_AUTHED         = 9,
    WIFI_DISCONNECT_REASON_DISASSOC_PWRCAP_BAD      = 10,  /* 11h */
    WIFI_DISCONNECT_REASON_DISASSOC_SUPCHAN_BAD     = 11,  /* 11h */
    WIFI_DISCONNECT_REASON_IE_INVALID               = 13,  /* 11i */
    WIFI_DISCONNECT_REASON_MIC_FAILURE              = 14,  /* 11i */
    WIFI_DISCONNECT_REASON_4WAY_HANDSHAKE_TIMEOUT   = 15,  /* 11i */
    WIFI_DISCONNECT_REASON_GROUP_KEY_UPDATE_TIMEOUT = 16,  /* 11i */
    WIFI_DISCONNECT_REASON_IE_IN_4WAY_DIFFERS       = 17,  /* 11i */
    WIFI_DISCONNECT_REASON_GROUP_CIPHER_INVALID     = 18,  /* 11i */
    WIFI_DISCONNECT_REASON_PAIRWISE_CIPHER_INVALID  = 19,  /* 11i */
    WIFI_DISCONNECT_REASON_AKMP_INVALID             = 20,  /* 11i */
    WIFI_DISCONNECT_REASON_UNSUPP_RSN_IE_VERSION    = 21,  /* 11i */
    WIFI_DISCONNECT_REASON_INVALID_RSN_IE_CAP       = 22,  /* 11i */
    WIFI_DISCONNECT_REASON_802_1X_AUTH_FAILED       = 23,  /* 11i */
    WIFI_DISCONNECT_REASON_CIPHER_SUITE_REJECTED    = 24,  /* 11i */

    WIFI_DISCONNECT_REASON_BEACON_TIMEOUT           = 200,
    WIFI_DISCONNECT_REASON_NO_AP_FOUND              = 201,
    WIFI_DISCONNECT_REASON_AUTH_FAIL                = 202,
    WIFI_DISCONNECT_REASON_ASSOC_FAIL               = 203,
    WIFI_DISCONNECT_REASON_HANDSHAKE_TIMEOUT        = 204,
};

enum rst_reason {
    REASON_DEFAULT_RST      = 0,    /* normal startup by power on */
    REASON_WDT_RST          = 1,    /* hardware watch dog reset */
    REASON_EXCEPTION_RST    = 2,    /* exception reset, GPIO status won�t change */
    REASON_SOFT_WDT_RST     = 3,    /* software watch dog reset, GPIO status won�t change */
    REASON_SOFT_RESTART     = 4,    /* software restart ,system_restart , GPIO status won�t change */
    REASON_DEEP_SLEEP_AWAKE = 5,    /* wake up from deep-sleep */
    REASON_EXT_SYS_RST      = 6     /* external system reset */
};

enum RFMode {
    RF_DEFAULT = 0, // RF_CAL or not after deep-sleep wake up, depends on init data byte 108.
    RF_CAL = 1,      // RF_CAL after deep-sleep wake up, there will be large current.
    RF_NO_CAL = 2,   // no RF_CAL after deep-sleep wake up, there will only be small current.
    RF_DISABLED = 4 // disable RF after deep-sleep wake up, just like modem sleep, there will be the smallest current.
};

class IPAddress {
public:
    struct _address {
        uint8_t byte[4];
    };

    IPAddress() {
        _addr = 0;
    }
    IPAddress(uint32_t addr) {
        _addr = addr;
    }
    IPAddress(const String &addr) {
        fromString(addr);
    }
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
        ((_address *)&_addr)->byte[0] = a;
        ((_address *)&_addr)->byte[1] = b;
        ((_address *)&_addr)->byte[2] = c;
        ((_address *)&_addr)->byte[3] = d;
    }

    operator uint32_t() const {
        return *(uint32_t *)&_addr;
    }

    IPAddress &operator=(uint32_t addr) {
        _addr = addr;
        return *this;
    }
    bool operator==(const IPAddress &addr) {
        return _addr = addr._addr;
    }

    bool fromString(const String &addr) {
        inet_pton(AF_INET, addr.c_str(), &_addr);
        return _addr != 0;
    }
    String toString() const {
        //in_addr a;
        //a.S_un.S_addr = _addr;
        char buffer[64];
        return inet_ntop(AF_INET, &_addr, buffer, sizeof(buffer));
    }

    uint8_t *raw_address() const {
        return (uint8_t *)&_addr;
    }

private:
    uint32_t _addr;
};

class ESP8266WiFiClass {
public:
    ESP8266WiFiClass() {
        _isConnected = true;
    }

    bool isConnected() {
        return _isConnected;
    }
    void setIsConnected(bool connected) {
        _isConnected = connected;
    }

    int hostByName(const char* aHostname, IPAddress& aResult) {
        return hostByName(aHostname, aResult, 10000);
    }
    int hostByName(const char* aHostname, IPAddress& aResult, uint32_t timeout_ms);

private:
    bool _isConnected;
};

extern ESP8266WiFiClass WiFi;
