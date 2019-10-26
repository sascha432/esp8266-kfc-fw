/**
 * Author: sascha_lammers@gmx.de
 */

#if SERIAL2TCP

#pragma once

#include <Arduino_compat.h>
#include <Buffer.h>
#if defined(ESP32)
#include "SyncClient.h"
#else
#include <SoftwareSerial.h>
#include <ESPAsyncTCP.h>
#endif
#include "serial2tcp.h"

#define NVT_IAC                 0xff    // 255
#define NVT_DONT                0xfe    // 254
#define NVT_DO                  0xfd    // 253
#define NVT_WONT                0xfc    // 252
#define NVT_WILL                0xfb    // 251
#define NVT_SB                  0xfa    // 250
#define NVT_GA                  0xf9    // 249
#define NVT_EL                  0xf8    // 248
#define NVT_EC                  0xf7    // 247
#define NVT_AYT                 0xf6    // 246
#define NVT_AO                  0xf5    // 245
#define NVT_IP                  0xf4    // 244
#define NVT_BRK                 0xf3    // 243
#define NVT_DM                  0xf2    // 242
#define NVT_NOP                 0xf1    // 241
#define NVT_SE                  0xf0    // 240

#define NVT_SB_ECHO             0x01    // 1
#define NVT_SB_CPO              0x2c    // 44

#define NVT_CPO_SET_BAUDRATE    0x01    // 1
#define NVT_CPO_SET_DATASIZE    0x02    // 2
#define NVT_CPO_SET_PARITY      0x03    // 3
#define NVT_CPO_SET_STOPSIZE    0x04    // 4
#define NVT_CPO_SET_CONTROL     0x05    // 5

class Serial2TcpConnection {
public:
    typedef int8_t error_t;
    typedef int32_t size_t;

    Serial2TcpConnection(AsyncClient *client, bool isAuthenticated);
    virtual ~Serial2TcpConnection();

    void close();

    bool isAuthenticated() const;
    void setClient(AsyncClient *client);
    AsyncClient *getClient() const;
    Buffer &getTxBuffer();
    Buffer &getRxBuffer();
    Buffer &getNvtBuffer();

private:
    uint8_t _isAuthenticated: 1;
    AsyncClient *_client;
    Buffer _txBuffer;
    Buffer _rxBuffer;
    Buffer _nvt;
};

#endif
