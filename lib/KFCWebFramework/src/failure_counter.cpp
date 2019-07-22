/**
 * Author: sascha_lammers@gmx.de
 */

#include <KFCTimezone.h>
#include "failure_counter.h"

#if SECURITY_LOGIN_ATTEMPTS

#ifdef SECURITY_LOGIN_FILE
const char _login_failure_file[] PROGMEM = { SECURITY_LOGIN_FILE };
#endif

FailureCounter::FailureCounter(const FailureCounterFileRecord_t &record) {
    _addr = record.addr;
    _counter = record.counter;
    _firstFailure = record.firstFailure;
    _lastFailure = record.lastFailure;
}

FailureCounter::FailureCounter(const IPAddress &addr) {
    _addr = addr;
    _counter = 0;
    _firstFailure = _lastFailure = time(nullptr);
    addFailure();
}

bool FailureCounter::operator !() const {
    return _counter == FailureCounter::INVALID_DATA;
}

FailureCounter::operator bool() const {
    return _counter != FailureCounter::INVALID_DATA;
}

const time_t FailureCounter::getTimeframe() const {
    return _lastFailure - _firstFailure;
}

const bool FailureCounter::isBlocked(const IPAddress &addr) const {
    if (_counter > SECURITY_LOGIN_ATTEMPTS && getTimeframe() <= SECURITY_LOGIN_TIMEFRAME) {
        debug_printf_P(PSTR("Failed attempt from %s #u, timeframe %u, access blocked\n"), _addr.toString().c_str(), _counter, (unsigned)getTimeframe());
        return true;
    }
    return false;
}

const bool FailureCounter::isMatch(const IPAddress &addr) const {
    return addr == _addr;
}

const uint32_t FailureCounter::getCounter() const {
    return _counter;
}

const IPAddress &FailureCounter::getIPAddress() const {
    return _addr;
}

void FailureCounter::addFailure() {
    if (_counter < FailureCounter::MAX_COUNTER) {
        _counter++;
    }
    _lastFailure = time(nullptr);
    debug_printf_P(PSTR("Failed attempt from %s #%u\n"), _addr.toString().c_str(), _counter);
#ifdef SECURITY_LOGIN_FILE
    File file = SPIFFS.open(_login_failure_file, "a");
    if (file) {
        FailureCounterFileRecord_t record;
        copyToRecord(record);
        file.write((uint8_t *)&record, sizeof(record));
        file.close();
    }
#endif
}

void FailureCounter::copyToRecord(FailureCounterFileRecord_t &record) const {
    record = { _addr, _counter, _firstFailure, _lastFailure };
}

const FailureCounter &FailureCounterContainer::addFailure(const IPAddress &addr) {
    for(auto failure = _failures.rbegin(); failure != _failures.rend(); ++failure) {
        debug_printf_P(PSTR("add failure ip %s record %s # %d\n"), addr.toString().c_str(), failure->getIPAddress().toString().c_str(), failure->getCounter());
        if (failure->isMatch(addr)) {
            failure->addFailure();
            return *failure;
        }
    }
    _failures.push_back(FailureCounter(addr));
    return _failures.back();
}

bool FailureCounterContainer::isAddressBlocked(const IPAddress &addr) {
    bool result = false;
    for(auto failure = _failures.begin(); _failures.end() != failure; ) {
        time_t timeframe = failure->getTimeframe();
        debug_printf_P(PSTR("is blocked ip %s record %s # %d, timeframe %lu\n"), addr.toString().c_str(), failure->getIPAddress().toString().c_str(), failure->getCounter(), timeframe);
        if (failure->isBlocked(addr)) {
            result = true;
            ++failure;
        } else if (timeframe > SECURITY_LOGIN_TIMEFRAME) {
            failure = _failures.erase(failure);
        } else {
            ++failure;
        }
    }
    return result;
}

String FailureCounter::getFirstFailure() const {
    char buf[32];
    timezone_strftime_P(buf, sizeof(buf), PSTR("%Y-%m-%dT%H:%M:%S %Z"), timezone_localtime(&_firstFailure));
    return buf;
}


/**
 * Read data from SPIIFS and clean up expired records
 */
void FailureCounterContainer::readFromSPIFFS() {
    _failures.clear();
#ifdef SECURITY_LOGIN_FILE
    File file = SPIFFS.open(SECURITY_LOGIN_FILE, "r");
    if (file) {
#if DEBUG
        int count = 0;
#endif
        while(file.available()) {
            FailureCounterFileRecord_t record;
            if (file.read((uint8_t *)&record, sizeof(record)) != sizeof(record)) {
                break;
            }
#if DEBUG
            count++;
#endif
            time_t timeframe = record.lastFailure - record.firstFailure;
            if (timeframe < SECURITY_LOGIN_STORE_TIMEFRAME) {
                _failures.push_back(FailureCounter(record));
            }
        }
        file.close();

        debug_printf_P(PSTR("%d records read from disk, %d stored in memory\n"), count, _failures.size());

        rewriteSPIFFSFile();
    }
#endif
}

/**
 * Remove expires records and store on SPIFFS
 **/
void FailureCounterContainer::rewriteSPIFFSFile() {

    File file = SPIFFS.open(SECURITY_LOGIN_FILE, "w");
    if (file) {
#if DEBUG
        int count = _failures.size();
#endif
        for(auto failure = _failures.begin(); _failures.end() != failure; ) {
            if (failure->getTimeframe() > SECURITY_LOGIN_TIMEFRAME) {
                failure = _failures.erase(failure);
            } else {
                FailureCounterFileRecord_t record;
                failure->copyToRecord(record);
                file.write((uint8_t *)&record, sizeof(record));
                ++failure;
            }
        }
        file.close();

        debug_printf_P(PSTR("Written %d out of %d records to SPIFFS\n"), _failures.size(), count);

    }
}


#endif
