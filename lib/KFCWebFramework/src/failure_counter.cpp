/**
 * Author: sascha_lammers@gmx.de
 */

#include <KFCTimezone.h>
#include "failure_counter.h"

#if SECURITY_LOGIN_ATTEMPTS

#ifdef SECURITY_LOGIN_FILE
PROGMEM_STRING_DEF(login_failure_file, SECURITY_LOGIN_FILE);
#endif

extern FailureCounterContainer &loginFailures;

FailureCounter::FailureCounter(const FailureCounterFileRecord_t &record)
{
    _addr = record.addr;
    _counter = record.counter;
    _firstFailure = record.firstFailure;
    _lastFailure = record.lastFailure;
}

FailureCounter::FailureCounter(const IPAddress &addr)
{
    _addr = addr;
    _counter = 0;
    _firstFailure = _lastFailure = time(nullptr);
    addFailure();
}

bool FailureCounter::operator !() const
{
    return _counter == FailureCounter::INVALID_DATA;
}

FailureCounter::operator bool() const
{
    return _counter != FailureCounter::INVALID_DATA;
}

time_t FailureCounter::getTimeframe() const
{
    return _lastFailure - _firstFailure;
}

bool FailureCounter::isBlocked(const IPAddress &addr) const
{
    if (_counter > SECURITY_LOGIN_ATTEMPTS && getTimeframe() <= SECURITY_LOGIN_TIMEFRAME) {
        debug_printf_P(PSTR("Failed attempt from %s #%u, timeframe %u, access blocked\n"), _addr.toString().c_str(), (unsigned)_counter, (unsigned)getTimeframe());
        return true;
    }
    return false;
}

bool FailureCounter::isMatch(const IPAddress &addr) const
{
    return addr == _addr;
}

uint32_t FailureCounter::getCounter() const {
    return _counter;
}

const IPAddress &FailureCounter::getIPAddress() const
{
    return _addr;
}

void FailureCounter::addFailure()
{
    if (_counter < FailureCounter::MAX_COUNTER) {
        _counter++;
    }
    _lastFailure = time(nullptr);
    debug_printf_P(PSTR("Failed attempt from %s #%u\n"), _addr.toString().c_str(), _counter);
#ifdef SECURITY_LOGIN_FILE
    if (IS_TIME_VALID(_lastFailure) && _lastFailure > loginFailures._lastRewrite + SECURITY_LOGIN_REWRITE_INTERVAL) {
        loginFailures._lastRewrite = SECURITY_LOGIN_REWRITE_INTERVAL;
        loginFailures.rewriteSPIFFSFile();
    }
    else {
        File file = SPIFFS.open(FSPGM(login_failure_file), fs::FileOpenMode::append);
        if (file) {
            write(file);
            file.close();
        }
    }
#endif
}

void FailureCounter::write(File &file) const
{
    FailureCounterFileRecord_t record = { _addr, _counter, _firstFailure, _lastFailure };
    file.write(reinterpret_cast<uint8_t *>(&record), sizeof(record));
}

const FailureCounter &FailureCounterContainer::addFailure(const IPAddress &addr)
{
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

bool FailureCounterContainer::isAddressBlocked(const IPAddress &addr)
{
    _removeOldRecords();
    for(auto &failure: _failures) {
#if DEBUG
        time_t timeframe = failure.getTimeframe();
        debug_printf_P(PSTR("is blocked ip %s record %s # %d, timeframe %lu\n"), addr.toString().c_str(), failure.getIPAddress().toString().c_str(), failure.getCounter(), timeframe);
#endif
        if (failure.isBlocked(addr)) {
            return true;
        }
    }
    return false;
}

String FailureCounter::getFirstFailure() const
{
    char buf[32];
    timezone_strftime_P(buf, sizeof(buf), PSTR("%Y-%m-%dT%H:%M:%S %Z"), timezone_localtime(&_firstFailure));
    return buf;
}


/**
 * Read data from SPIIFS and clean up expired records
 */
void FailureCounterContainer::readFromSPIFFS()
{
    _failures.clear();
#ifdef SECURITY_LOGIN_FILE
    File file = SPIFFS.open(FSPGM(login_failure_file), fs::FileOpenMode::read);
    if (file) {
#if DEBUG
        int count = 0;
#endif
        bool isTimeValid = IS_TIME_VALID(time(nullptr));
        while(file.available()) {
            FailureCounterFileRecord_t record;
            if (file.read((uint8_t *)&record, sizeof(record)) != sizeof(record)) {
                break;
            }
#if DEBUG
            count++;
#endif
            time_t timeframe = record.lastFailure - record.firstFailure;
            if (isTimeValid == false || timeframe < SECURITY_LOGIN_STORE_TIMEFRAME) { // keep all records if the time has not been set
                _failures.push_back(FailureCounter(record));
            }
        }
        file.close();

        debug_printf_P(PSTR("%d records read from disk, %d stored in memory\n"), count, _failures.size());
    }
    else {

    }
#endif
}

/**
 * Remove expires records and store on SPIFFS
 **/
void FailureCounterContainer::rewriteSPIFFSFile()
{
    File file = SPIFFS.open(FSPGM(login_failure_file), fs::FileOpenMode::write);
    if (file) {
#if DEBUG
        int count = _failures.size();
#endif
        _removeOldRecords();
        for(auto &failure: _failures) {
            failure.write(file);
        }
        file.close();

        debug_printf_P(PSTR("Written %d out of %d records to SPIFFS\n"), _failures.size(), count);
    }
}

void FailureCounterContainer::_removeOldRecords()
{
    _failures.erase(std::remove_if(_failures.begin(), _failures.end(), [](const FailureCounter &failure) {
        return failure.getTimeframe() > SECURITY_LOGIN_STORE_TIMEFRAME;
    }), _failures.end());
}

#endif
