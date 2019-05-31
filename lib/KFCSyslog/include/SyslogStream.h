/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

enum SyslogProtocol {
    SYSLOG_NONE = 0,
    SYSLOG_UDP,
    SYSLOG_TCP,
    SYSLOG_TCP_TLS,
};

class SyslogQueue {
public:
    SyslogQueue() {}
};

class SyslogStream : public Stream {
public:
    SyslogStream(const char *host, uint16_t port, SyslogProtocol protocol, SyslogQueue &syslogQueue) {

    }

    virtual size_t write(uint8_t) override {

    }
    virtual void flush() override {

    }

    virtual int available() {
        return false;
    }

    virtual int read() {
        return -1;
    }

    virtual int peek() {
        return -1;
    }


};
