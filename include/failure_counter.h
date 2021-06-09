/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <global.h>

#if SECURITY_LOGIN_ATTEMPTS

#ifndef DEBUG_LOGIN_FAILURES
#define DEBUG_LOGIN_FAILURES                0
#endif

#include <Arduino_compat.h>
#include <time.h>
#include <vector>

#include <push_pack.h>

typedef struct __attribute__packed__ FailureCounterFileRecordStruct  {
    uint32_t addr;
    uint16_t counter;
    uint32_t firstFailure;
    uint32_t lastFailure;
} FailureCounterFileRecord_t;

class FailureCounterContainer;

class FailureCounter  {
public:
    enum {
        MAX_COUNTER = 0xfffe,
        INVALID_DATA = 0xffff
    };

    FailureCounter(FailureCounterContainer &container);
    FailureCounter(FailureCounterContainer &container, const FailureCounterFileRecord_t &record);
    FailureCounter(FailureCounterContainer &container, const IPAddress &addr);

    FailureCounter(FailureCounter &&move) :
        _addr(move._addr),
        _counter(move._counter),
        _firstFailure(move._firstFailure),
        _lastFailure(move._lastFailure),
        _container(move._container)
    {
    }

    FailureCounter &operator=(FailureCounter &&move) {
        ::new(static_cast<void *>(this)) FailureCounter(std::move(move));
        return *this;
    }

    bool operator !() const;
    operator bool() const;

    uint32_t getTimeframe() const;
    bool isBlocked(const IPAddress &addr) const;
    bool isMatch(const IPAddress &addr) const;
    String getFirstFailure() const;
    uint32_t getCounter() const;
    const IPAddress &getIPAddress() const;

    void addFailure();
    void write(File &file) const;

private:
    IPAddress _addr;
    uint16_t _counter;
    uint32_t _firstFailure;
    uint32_t _lastFailure;
    FailureCounterContainer &_container;
};

class FailureCounterContainer {
public:
    FailureCounterContainer();
    void clear() {
        _failures.clear();
    }

    const FailureCounter &addFailure(const IPAddress &addr);
    bool isAddressBlocked(const IPAddress &addr);
    void readFromFS();
    void rewriteStorageFile();

private:
    friend FailureCounter;

    void _removeOldRecords();

private:
    std::vector<FailureCounter> _failures;
    time_t _lastRewrite;
    uint32_t _rewriteInterval;
    uint32_t _storageTimeframe;
    uint16_t _checkTimeframe;
    uint8_t _attempts;
};

#include <pop_pack.h>

#endif
