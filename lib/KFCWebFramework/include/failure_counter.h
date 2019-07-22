/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if SECURITY_LOGIN_ATTEMPTS

#include <Arduino_compat.h>
#include <time.h>
#include <vector>

#include <push_pack.h>

typedef struct __attribute__packed__ FailureCounterFileRecordStruct  {
    uint32_t addr;
    uint16_t counter;
    time_t firstFailure;
    time_t lastFailure;
} FailureCounterFileRecord_t;

class FailureCounter  {
public:
    enum {
        MAX_COUNTER = 0xfffe,
        INVALID_DATA = 0xffff
    };

    FailureCounter() {
        _counter = FailureCounter::INVALID_DATA;
    }
    FailureCounter(const FailureCounterFileRecord_t &record);
    FailureCounter(const IPAddress &addr);

    bool operator !() const;
    operator bool() const;

    const time_t getTimeframe() const;
    const bool isBlocked(const IPAddress &addr) const;
    const bool isMatch(const IPAddress &addr) const;
    String getFirstFailure() const;
    const uint32_t getCounter() const;
    const IPAddress &getIPAddress() const;

    void addFailure();
    void copyToRecord(FailureCounterFileRecord_t &record) const;

private:
    IPAddress _addr;
    uint16_t _counter;
    time_t _firstFailure;
    time_t _lastFailure;
};

class FailureCounterContainer {
public:
    FailureCounterContainer() {
    }
    void clear() {
        _failures.clear();
    }

    const FailureCounter &addFailure(const IPAddress &addr);
    bool isAddressBlocked(const IPAddress &addr);
    void readFromSPIFFS();
    void rewriteSPIFFSFile();

private:
    std::vector<FailureCounter> _failures;
};

#include <pop_pack.h>

#endif
