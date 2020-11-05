/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include  <Arduino_compat.h>
#include  <SerialTwoWire.h>
#include  <EventScheduler.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace Dimmer {

    using WireStream = DimmerTwoWireEx;
    using VersionType = dimmer_version_t;
    using MetricsType = dimmer_metrics_t;
    using ConfigInfoType = dimmer_config_info_t;
    using ConfigType = register_mem_cfg_t;

    class FadeCommand {
    public:

        static bool fadeTo(WireStream &wire, uint8_t channel, int16_t fromLevel, int16_t toLevel, float fadeTime, uint8_t address = DIMMER_I2C_ADDRESS) {
            if (wire.lock()) {
                dimmer_command_fade_t fade(channel, fromLevel, toLevel, fadeTime);
                wire.beginTransmission(address);
                wire.write(fade.address());
                wire.write<dimmer_command_fade_t>(fade);
                wire.endTransmission();
                wire.unlock();
                return true;
            }
            return false;
        }
    };

    struct Config {
        VersionType version;
        ConfigInfoType info;
        ConfigType config;
    };

    class GetConfig {
    public:

        static constexpr uint8_t kHasVersion = 0x01;
        static constexpr uint8_t kHasConfig = 0x04;
        static constexpr uint8_t kIsValid = kHasVersion|kHasConfig;
        static constexpr uint8_t kStopped = 0x080;

        using Callback = std::function<void(GetConfig &config, bool)>;


        GetConfig(WireStream &wire, uint8_t address = DIMMER_I2C_ADDRESS) : _wire(wire), _config({}), _address(address), _valid(0) {}
        ~GetConfig() {
            end();
        }

        void begin() {
            _valid = 0;
            _timer.remove();
        }

        inline void end() {
            begin();
            _valid = kStopped;
        }

        // retries = number of retries
        // retryDelay = delay between retries in milliseconds
        // callback = retry in background and invoke callback when done
        // initialDelay = initial delay when running in background
        // Note:
        // if not running in background, retries * retryDelay should not exceed 500ms
        // if executed inside an ISR / can_yield() is false, retries is set to 0
        bool readConfig(uint8_t retries, uint16_t retryDelay, Callback callback = nullptr, uint16_t initialDelay = 100) {

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

    private:
        void _rescheduleNextRetry(Event::CallbackTimerPtr timer) {
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

        void _readConfigTimer(Event::CallbackTimerPtr timer) {

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

    public:

        bool getVersion() {
            _valid &= ~(kHasVersion);
            dimmer_version_info_t version_info;
            if (_wire.lock()) {
                _wire.beginTransmission(_address);
                _wire.write<decltype(Dimmer::kRequestVersion)>(Dimmer::kRequestVersion);
                if (_wire.endTransmission() == 0 && _wire.requestFrom(_address, sizeof(version_info)) == sizeof(version_info) && _wire.read<dimmer_version_info_t>(version_info) == sizeof(version_info)) {
                    _config.version = version_info.version;
                    _config.info = version_info.info;
                    _valid |= kHasVersion;
                }
                _wire.unlock();
            }
            __LDBG_printf("status=%u version=%u.%u.%u levels=%u channels=%u addr=0x%02x length=%u",
                (_valid & kHasVersion),
                _config.version.major, _config.version.minor, _config.version.revision,
                _config.info.max_levels, _config.info.channel_count, _config.info.cfg_start_address, _config.info.length);
            return (_valid & kHasVersion) && _config.version;
        }

        bool getConfig() {
            _valid &= ~kHasConfig;
            ConfigType cfg;
            if (sizeof(cfg) != _config.info.length) {
                return false;
            }
            if (_wire.lock()) {
                _wire.beginTransmission(_address);
                _wire.write(DIMMER_REGISTER_READ_LENGTH);
                _wire.write(_config.info.length);
                _wire.write(_config.info.cfg_start_address);
                if (_wire.endTransmission() == 0 && _wire.requestFrom(_address, sizeof(cfg)) == sizeof(cfg) && _wire.read<ConfigType>(cfg) == sizeof(cfg)) {
                    _config.config = cfg;
                    _valid |= kHasConfig;
                }
                _wire.unlock();
            }
            __LDBG_printf("status=%u", (_valid & kHasConfig));
            return (_valid & kHasConfig);
        }

        static bool isValid(VersionType version) {
            return version;
        }

        Config &config() {
            return _config;
        }

        // retries will be ignored if called from inside an ISR
        // delay between retries in milliseconds
        // _config has to be populated before using this methid
        bool storeConfig(uint8_t retries, uint16_t delay) {
            __LDBG_printf("store version=%u.%u.%u", _config.version.major, _config.version.minor, _config.version.revision);
            if (!_config.version) {
                return false;
            }
            //TODO
            return false;
        }

    private:
        WireStream &_wire;
        Event::Timer _timer;
        Config _config;
        uint8_t _address;
        uint8_t _valid;
        uint8_t _retries;
        uint16_t _retryDelay;
        Callback _callback;
    };


    class GetMetrics {
    public:
        GetMetrics() : _metrics({}) {}


        MetricsType &getMetrics() {
            return _metrics;
        }

    private:
        MetricsType _metrics;
    };


};

#include <debug_helper_disable.h>
