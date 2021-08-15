/**
 * Author: sascha_lammers@gmx.de
 */

#include  <Arduino_compat.h>
#include "../firmware_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Dimmer;

ConfigReaderWriter::ConfigReaderWriter(TwoWire &wire, uint8_t address) :
    _wire(wire),
    _config({}),
    _address(address),
    _valid(0)
{}

void ConfigReaderWriter::_rescheduleNextRetry(Event::CallbackTimerPtr timer)
{
    if (!(_valid & kStopped) && _retries) {
        __LDBG_printf("rearm retries=%u delay=%u", _retries, _retryDelay);
        _retries--;
        timer->rearm(Event::milliseconds(_retryDelay), false);
    }
    else {
        __LDBG_printf("end callback status=false");
        _callback(*this, false);
    }
}

void ConfigReaderWriter::_readConfigTimer(Event::CallbackTimerPtr timer)
{
    if (!(_valid & kHasVersion)) {
        if (!getVersion()) {
            _rescheduleNextRetry(timer);
            return;
        }
    }
    if (!(_valid & kHasConfig)) {
        if (!getConfig()) {
            _rescheduleNextRetry(timer);
            return;
        }
    }

    __LDBG_printf("end callback status=%u", (_valid & kIsValid) == kIsValid);
    _callback(*this, (_valid & kIsValid) == kIsValid);
}

bool ConfigReaderWriter::getVersion()
{
    _valid &= ~(kHasVersion);
    if (_wire.lock()) {
        _config.version = {};
        // __LDBG_printf("requesting version address=0x%02x request=%02x%02x%02x", _address, Dimmer::kRequestVersion[0], Dimmer::kRequestVersion[1], Dimmer::kRequestVersion[2]);
        _wire.beginTransmission(_address);
        _wire.write<decltype(Dimmer::kRequestVersion)>(Dimmer::kRequestVersion);
        if (_wire.endTransmission() == 0 && _wire.requestFrom(_address, Config::kVersionSize) == Config::kVersionSize && _wire.readBytes(reinterpret_cast<uint8_t *>(&_config.version), Config::kVersionSize) == Config::kVersionSize) {
            _valid |= kHasVersion;
        }
        else {
            _config.version = {};
        }
        _wire.unlock();
    }
    __LDBG_printf("status=%u version=%u.%u.%u levels=%u channels=%u addr=0x%02x length=%u",
        (_valid & kHasVersion),
        _config.version.major, _config.version.minor, _config.version.revision,
        _config.info.max_levels, _config.info.channel_count, _config.info.cfg_start_address, _config.info.length);
    return (_valid & kHasVersion) && _config.version;
}

bool ConfigReaderWriter::getConfig()
{
    _valid &= ~kHasConfig;

    __DBG_assert_printf(_config.info.length == sizeof(_config.config), "_config.info.length=%u sizeof(_config.config)=%u", _config.info.length, _config.config);
    _config.config = {};
    _config.info.length = std::min<uint8_t>(_config.info.length, sizeof(_config.config));

    if (_wire.lock()) {
        _wire.beginTransmission(_address);
        _wire.write(DIMMER_REGISTER_READ_LENGTH);
        _wire.write(_config.info.length);
        _wire.write(_config.info.cfg_start_address);
        if (_wire.endTransmission() == 0 && _wire.requestFrom(_address, _config.info.length) == _config.info.length && _wire.readBytes(reinterpret_cast<uint8_t *>(&_config.config), _config.info.length) == _config.info.length) {
            _valid |= kHasConfig;
        }
        else {
            _config.config = {};
        }
        _wire.unlock();
    }
    __LDBG_printf("status=%u", (_valid & kHasConfig));
    return (_valid & kHasConfig);
}

// retries will be ignored if called from inside an ISR
// delay between retries in milliseconds
// _config has to be populated before using this methid
bool ConfigReaderWriter::storeConfig(uint8_t retries, uint16_t retryDelay)
{
    __LDBG_printf("store version=%u.%u.%u", _config.version.major, _config.version.minor, _config.version.revision);
    if (!_config.version) {
        return false;
    }
    if (!_wire.lock()) {
        return false;
    }
    while(_retries) {
        _config.info.length = std::min<uint8_t>(_config.info.length, sizeof(_config.config));

        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        if (_wire.write(_config.info.cfg_start_address) == 1 &&
            _wire.write(reinterpret_cast<uint8_t *>(&_config.config), _config.info.length) == _config.info.length &&
            _wire.endTransmission() == 0)
        {
            _wire.unlock();
            _wire.writeEEPROM();
            return true;
        }
        if (!can_yield()) {
            break;
        }
        delay(_retryDelay);
        _retries--;
    }

    _wire.unlock();
    return false;
}

bool ConfigReaderWriter::readConfig(uint8_t retries, uint16_t retryDelay, Callback callback, uint16_t initialDelay) {

    __LDBG_printf("retries=%u retry_dly=%u callback=%p initial_dly=%u", retries, retryDelay, &callback, initialDelay);

    _retries = retries;
    _retryDelay = retryDelay;
    _callback = callback;
    _valid = 0;

    if (_callback) {

        _timer.add(Event::milliseconds(initialDelay), false, [this](Event::CallbackTimerPtr timer) {
            __LDBG_printf("getConfig timer _valid=%u", _valid);
            if (!(_valid & kStopped)) {
                _readConfigTimer(timer);
            }
        });
        return true;

    }
    else {
        while(_retries) {
            while(true) {
                if (!(_valid & kHasVersion)) {
                    if (!getVersion()) {
                        break;
                    }
                }
                if (!(_valid & kHasConfig)) {
                    if (!getConfig()) {
                        break;
                    }
                }
                break;
            }
            if (!can_yield()) {
                break;
            }
            delay(_retryDelay);
            _retries--;
        }
        return _valid == kIsValid;
    }
}
