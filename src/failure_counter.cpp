/**
 * Author: sascha_lammers@gmx.de
 */

#include <global.h>

#if SECURITY_LOGIN_ATTEMPTS

#include <Arduino_compat.h>
#include <PrintString.h>
#include <kfc_fw_config.h>
#include "failure_counter.h"

#if DEBUG_LOGIN_FAILURES
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

FailureCounter::FailureCounter(FailureCounterContainer &container) :
    _counter(FailureCounter::INVALID_DATA),
    _container(container)
{
}


FailureCounter::FailureCounter(FailureCounterContainer &container, const FailureCounterFileRecord_t &record) :
    _addr(record.addr),
    _counter(record.counter),
    _firstFailure(record.firstFailure),
    _lastFailure(record.lastFailure),
    _container(container)
{
}

FailureCounter::FailureCounter(FailureCounterContainer &container, const IPAddress &addr) :
    _addr(addr),
    _counter(0),
    _firstFailure(time(nullptr)),
    _lastFailure(_firstFailure),
    _container(container)
{
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

uint32_t FailureCounter::getTimeframe() const
{
    return _lastFailure - _firstFailure;
}

bool FailureCounter::isBlocked(const IPAddress &addr) const
{
    if (_counter > SECURITY_LOGIN_ATTEMPTS && getTimeframe() <= _container._checkTimeframe) {
        __LDBG_printf("Failed attempt from %s #%u, timeframe %u, access blocked", _addr.toString().c_str(), _counter, getTimeframe());
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
    __LDBG_printf("Failed attempt from %s #%u", _addr.toString().c_str(), _counter);
    if (IS_TIME_VALID(_lastFailure) && _lastFailure > _container._lastRewrite + _container._rewriteInterval) {
        _container._lastRewrite = _container._rewriteInterval;
        _container.rewriteStorageFile();
    }
    else {
        auto file = KFCFS.open(FSPGM(login_failure_file), fs::FileOpenMode::append);
        if (file) {
            write(file);
            file.close();
        }
    }
}

void FailureCounter::write(File &file) const
{
    FailureCounterFileRecord_t record = { _addr, _counter, _firstFailure, _lastFailure };
    file.write(reinterpret_cast<uint8_t *>(&record), sizeof(record));
}

const FailureCounter &FailureCounterContainer::addFailure(const IPAddress &addr)
{
    for(auto failure = _failures.rbegin(); failure != _failures.rend(); ++failure) {
        __LDBG_printf("add failure ip %s record %s # %d", addr.toString().c_str(), failure->getIPAddress().toString().c_str(), failure->getCounter());
        if (failure->isMatch(addr)) {
            failure->addFailure();
            return *failure;
        }
    }
    _failures.push_back(FailureCounter(*this, addr));
    return _failures.back();
}

bool FailureCounterContainer::isAddressBlocked(const IPAddress &addr)
{
    _removeOldRecords();
    for(auto &failure: _failures) {
        __LDBG_printf("is blocked ip %s record %s # %d, timeframe %lu", addr.toString().c_str(), failure.getIPAddress().toString().c_str(), failure.getCounter(), failure.getTimeframe());
        if (failure.isBlocked(addr)) {
            return true;
        }
    }
    return false;
}

String FailureCounter::getFirstFailure() const
{
    PrintString tmp;
    tmp.strftime_P(PSTR("%Y-%m-%dT%H:%M:%S %Z"), _firstFailure);
    return tmp;
}


FailureCounterContainer::FailureCounterContainer() :
    _lastRewrite(0)
{
    auto cfg = System::WebServer::getConfig();
    _rewriteInterval = cfg.getLoginRewriteInterval();
    _storageTimeframe = cfg.getLoginStorageTimeframe();
    _checkTimeframe = cfg.login_timeframe;
    _attempts = cfg.login_attempts;
}


/**
 * Read data from SPIIFS and clean up expired records
 */
void FailureCounterContainer::readFromFS()
{
    _failures.clear();
    auto file = KFCFS.open(FSPGM(login_failure_file), fs::FileOpenMode::read);
    if (file) {
#if DEBUG_LOGIN_FAILURES
        auto count = 0;
#endif
        bool isTimeValid = IS_TIME_VALID(time(nullptr));
        while(file.available()) {
            FailureCounterFileRecord_t record;
            if (file.read((uint8_t *)&record, sizeof(record)) != sizeof(record)) {
                break;
            }
#if DEBUG_LOGIN_FAILURES
            count++;
#endif
            uint32_t timeframe = record.lastFailure - record.firstFailure;
            if (isTimeValid == false || timeframe <  _storageTimeframe) { // keep all records if the time has not been set
                _failures.push_back(FailureCounter(*this, record));
            }
        }
        file.close();

        __LDBG_printf("%d records read from disk, %d stored in memory", count, _failures.size());
    }
}

/**
 * Remove expires records and store on FS
 **/
void FailureCounterContainer::rewriteStorageFile()
{
    auto file = KFCFS.open(FSPGM(login_failure_file), fs::FileOpenMode::write);
    if (file) {
#if DEBUG_LOGIN_FAILURES
        auto count = _failures.size();
#endif
        _removeOldRecords();
        for(auto &failure: _failures) {
            failure.write(file);
        }
        file.close();

        __LDBG_printf("Written %d out of %d records to FS", _failures.size(), count);
    }
}

void FailureCounterContainer::_removeOldRecords()
{
    // if (this == nullptr) {
    //     __DBG_printf("_removeOldRecords: self=nullptr");
    //     return;
    // }
    _failures.erase(std::remove_if(_failures.begin(), _failures.end(), [this](const FailureCounter &failure) {
        return failure.getTimeframe() > _storageTimeframe;
    }), _failures.end());
}


#endif
