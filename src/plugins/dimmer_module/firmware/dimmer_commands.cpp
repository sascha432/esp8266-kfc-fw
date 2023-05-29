/**
 * Author: sascha_lammers@gmx.de
 */

#include "../firmware_protocol.h"
#include <Arduino_compat.h>

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
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
    if (_retries) {
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
    if (!getConfig(true)) {
        _rescheduleNextRetry(timer);
        return;
    }
    __LDBG_printf("end callback status=%u", (_valid & kIsValid) == kIsValid);
    _callback(*this, (_valid & kIsValid) == kIsValid);
}

bool ConfigReaderWriter::getConfig(bool config)
{
    _valid = 0;
    _config = {};

    Dimmer::TwoWire::Lock lock(_wire);
    if (lock) {
        __LDBG_printf("requesting version address=0x%02x r=%02x%02x%02x", _address, Dimmer::kRequestVersion[0], Dimmer::kRequestVersion[1], Dimmer::kRequestVersion[2]);

        _wire.beginTransmission(_address);
        _wire.write<decltype(Dimmer::kRequestVersion)>(Dimmer::kRequestVersion);
        if (
            _wire.endTransmission(false) == 0 &&
            _wire.requestFrom(_address, Config::kVersionSize, 0U) == Config::kVersionSize &&
            _wire.readBytes(reinterpret_cast<uint8_t *>(&_config.version), Config::kVersionSize) == Config::kVersionSize
        ) {
            _valid |= kHasVersion;
            _config.info.length = std::min<uint8_t>(_config.info.length, sizeof(_config.config));
            __LDBG_printf("version=%u.%u.%u levels=%u channels=%u addr=0x%02x length=%u",
                _config.version.major, _config.version.minor, _config.version.revision,
                _config.info.max_levels, _config.info.channel_count, _config.info.cfg_start_address, _config.info.length
            );
        }
        else {
            __LDBG_printf("failed to read version=%u.%u.%u levels=%u channels=%u addr=0x%02x length=%u",
                _config.version.major, _config.version.minor, _config.version.revision,
                _config.info.max_levels, _config.info.channel_count, _config.info.cfg_start_address, _config.info.length
            );
            _config.version = {};
            _config.info = {};
            return false;
        }

        _wire.beginTransmission(_address);
        _wire.write(DIMMER_REGISTER_READ_LENGTH);
        _wire.write(_config.info.length);
        _wire.write(_config.info.cfg_start_address);
        if (
            _wire.endTransmission(false) == 0 &&
            _wire.requestFrom(_address, _config.info.length) == _config.info.length &&
            _wire.readBytes(reinterpret_cast<uint8_t *>(&_config.config), _config.info.length) == _config.info.length
        ) {
            _valid |= kHasConfig;
            return true;
        }
        else {
            _config.config = {};
            __LDBG_printf("failed to read config");
            return false;
        }
    }
    return false;
}

bool ConfigReaderWriter::storeConfig(uint8_t retries, uint16_t retryDelay)
{
    __LDBG_printf("store version=%u.%u.%u address=%02x length=%u", _config.version.major, _config.version.minor, _config.version.revision, _config.info.cfg_start_address, _config.info.length);
    if (!_config.version) {
        __LDBG_printf("invalid version");
        return false;
    }
    if (!_config.info.cfg_start_address) {
        __LDBG_printf("invalid address");
        return false;
    }

    Dimmer::TwoWire::Lock lock(_wire);
    if (!lock) {
        __LDBG_printf("cannot acquire lock");
        return false;
    }
    while(_retries) {
        _config.info.length = std::min<uint8_t>(_config.info.length, sizeof(_config.config));

        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        if (_wire.write(_config.info.cfg_start_address) == 1 &&
            _wire.write(reinterpret_cast<uint8_t *>(&_config.config), _config.info.length) == _config.info.length &&
            _wire.endTransmission() == 0
        ) {
            __LDBG_printf("config written address=%02x length=%u", _config.info.cfg_start_address, _config.info.length);
            _wire.writeConfig(true);
            _config.version = dimmer_version_t({});
            return true;
        }
        if (!can_yield()) {
            __LDBG_printf("cannot yield");
            break;
        }
        delay(_retryDelay);
        _retries--;
        __LDBG_printf("retries %u", _retries);
    }

    __LDBG_printf("error writing config");
    return false;
}

bool ConfigReaderWriter::readConfig(uint8_t retries, uint16_t retryDelay, Callback callback, uint16_t initialDelay)
{
    __LDBG_printf("retries=%u retry_dly=%u callback=%p initial_dly=%u", retries, retryDelay, &callback, initialDelay);

    _retries = retries;
    _retryDelay = retryDelay;
    _callback = callback;
    _valid = 0;

    if (_callback) {
        _Timer(_timer).add(Event::milliseconds(initialDelay), false, [this](Event::CallbackTimerPtr timer) {
            __LDBG_printf("getConfig timer _valid=%u", _valid);
            _readConfigTimer(timer);
        });
        return true;
    }
    else {
        while(_retries) {
            if (getConfig(true)) {
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
