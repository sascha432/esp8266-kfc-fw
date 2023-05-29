/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <SerialTwoWire.h>
#include <EventScheduler.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace Dimmer {

    static constexpr uint8_t kDefaultSlaveAddress = DIMMER_I2C_ADDRESS;

    class ConfigReaderWriter;
    using ConfigReaderWriterPtr = std::unique_ptr<ConfigReaderWriter>;

    class TwoWire : public TwoWireEx {
    public:
        using TwoWireEx::TwoWireEx;

        struct Lock {
            Lock(TwoWire &wire) :
                _wire(wire),
                _locked(_wire.lock())
            {
            }
            ~Lock() {
                if (_locked) {
                    _wire.unlock();
                    _locked = false;
                }
            }
            operator bool() const {
                return _locked;
            }
            TwoWire &_wire;
            bool _locked;
        };

        void fadeTo(uint8_t channel, int16_t fromLevel, int16_t toLevel, float fadeTime, uint8_t address = kDefaultSlaveAddress) {
            if (!lock()) {
                return;
            }
            beginTransmission(address);
            write(dimmer_command_fade_t(channel, fromLevel, toLevel, fadeTime));
            endTransmission();
            unlock();
        }

        void writeEEPROM(bool noLocking = false, uint8_t address = kDefaultSlaveAddress) {
            if (noLocking || lock()) {
                beginTransmission(address);
                write(DIMMER_REGISTER_COMMAND);
                write(DIMMER_COMMAND_WRITE_EEPROM);
                endTransmission();
                if (!noLocking) {
                    unlock();
                }
            }
        }

        void writeConfig(bool noLocking = false, uint8_t address = kDefaultSlaveAddress) {
            #ifdef DIMMER_COMMAND_WRITE_CONFIG
                if (noLocking || lock()) {
                    beginTransmission(address);
                    write(DIMMER_REGISTER_COMMAND);
                    write(DIMMER_COMMAND_WRITE_EEPROM);
                    write(DIMMER_COMMAND_WRITE_CONFIG);
                    endTransmission();
                    if (!noLocking) {
                        unlock();
                    }
                }
            #else
                writeEEPROM(noLocking);
            #endif
        }

        void restoreFactory(uint8_t address = kDefaultSlaveAddress) {
            if (!lock()) {
                return;
            }
            beginTransmission(address);
            write(DIMMER_REGISTER_COMMAND);
            write(DIMMER_COMMAND_RESTORE_FS);
            endTransmission();
            unlock();
        }

        void printInfo(uint8_t address = kDefaultSlaveAddress) {
            if (!lock()) {
                return;
            }
            beginTransmission(address);
            write(DIMMER_REGISTER_COMMAND);
            write(DIMMER_COMMAND_PRINT_INFO);
            endTransmission();
            unlock();
        }

        void printConfig(uint8_t address = kDefaultSlaveAddress) {
            #ifdef DIMMER_COMMAND_PRINT_CONFIG
                if (!lock()) {
                    return;
                }
                beginTransmission(address);
                write(DIMMER_REGISTER_COMMAND);
                write(DIMMER_COMMAND_PRINT_CONFIG);
                endTransmission();
                unlock();
            #endif
        }

        void setZeroCrossing(uint16_t value, uint8_t address = kDefaultSlaveAddress)
        {
            if (!lock()) {
                return;
            }
            beginTransmission(address);
            write(DIMMER_REGISTER_COMMAND);
            write(DIMMER_COMMAND_SET_ZC_DELAY);
            write(static_cast<uint8_t>(value));
            write(static_cast<uint8_t>(value << 8));
            endTransmission();
            unlock();
        }

        void changeZeroCrossing(int16_t value, uint8_t address = kDefaultSlaveAddress)
        {
            if (!lock()) {
                return;
            }
            beginTransmission(address);
            write(DIMMER_REGISTER_COMMAND);
            if (value >= 0) {
                write(DIMMER_COMMAND_INCR_ZC_DELAY);
                write(static_cast<uint8_t>(value));
            }
            else {
                write(DIMMER_COMMAND_DECR_ZC_DELAY);
                write(static_cast<uint8_t>(-value));
            }
            endTransmission();
            unlock();
        }

        void forceTemperatureCheck(uint8_t address = kDefaultSlaveAddress) {
            if (!lock()) {
                return;
            }
            beginTransmission(address);
            write(DIMMER_REGISTER_COMMAND);
            write(DIMMER_COMMAND_FORCE_TEMP_CHECK);
            endTransmission();
            unlock();
        }

        CubicInterpolation readCubicInterpolation(uint8_t channel, uint8_t address = kDefaultSlaveAddress) {
            CubicInterpolation ci;
            if (!lock()) {
                return ci;
            }
            beginTransmission(address);
            write(DIMMER_REGISTER_COMMAND);
            write(DIMMER_COMMAND_READ_CUBIC_INT);
            write(channel);
            if (((endTransmission(false) == 0) && (requestFrom(DIMMER_I2C_ADDRESS, CubicInterpolation::size()) == CubicInterpolation::size()) && (read(ci._data) == CubicInterpolation::size()))) {
                ci.setChannel(channel);
            }
            unlock();
            return ci;
        }

        bool writeCubicInterpolation(const CubicInterpolation &ci, uint8_t address = kDefaultSlaveAddress) {
            if (!lock()) {
                return false;
            }
            beginTransmission(address);
            write(DIMMER_REGISTER_COMMAND);
            write(DIMMER_COMMAND_WRITE_CUBIC_INT);
            write(ci._channel);
            write(ci._data);
            bool result = (endTransmission() == 0);
            unlock();
            return result;
        }

        MetricsType getMetrics(uint8_t address = kDefaultSlaveAddress) {
            MetricsType metrics;
            if (lock()) {
                beginTransmission(address);
                write(DIMMER_REGISTER_COMMAND);
                write(0); //TODO not implemented
                // #warning //TODO not implemented
                if (!(
                    (endTransmission() == 0) &&
                    (requestFrom(DIMMER_I2C_ADDRESS, MetricsType::size()) == MetricsType::size()) &&
                    (read(metrics) == MetricsType::size())
                )) {
                    metrics.invalidate();
                }
                unlock();
            }
            return metrics;
        }

        ConfigReaderWriter getConfigReader(uint8_t address = kDefaultSlaveAddress);
        ConfigReaderWriter getConfigWriter(uint8_t address = kDefaultSlaveAddress);

    };

    class ConfigReaderWriter {
    public:

        static constexpr uint8_t kHasVersion = 0x01;
        static constexpr uint8_t kHasConfig = 0x02;
        static constexpr uint8_t kIsValid = kHasVersion|kHasConfig;

        using Callback = std::function<void(ConfigReaderWriter &config, bool)>;

        ConfigReaderWriter(ConfigReaderWriter &&rw) :
            _wire(rw._wire),
            _timer(), // timers cannot be moved or copied
            _config(rw._config),
            _address(rw._address),
            _valid(rw._valid),
            _retries(rw._retries),
            _retryDelay(rw._retryDelay),
            _callback(std::exchange(rw._callback, nullptr))
        {
            _Timer(rw._timer).remove();
        }


        ConfigReaderWriter(TwoWire &wire, uint8_t address = kDefaultSlaveAddress);
        ~ConfigReaderWriter();

        void begin();
        void end();

        static bool isValid(VersionType version);
        Config &config();
        const Config &config() const;

        // retries = number of retries
        // retryDelay = delay between retries in milliseconds
        // callback = retry in background and invoke callback when done
        // initialDelay = initial delay when running in background
        // Note:
        // if not running in background, retries * retryDelay should not exceed 500ms
        // if executed inside an ISR / can_yield() is false, retries is set to 0
        bool readConfig(uint8_t retries, uint16_t retryDelay, Callback callback = nullptr, uint16_t initialDelay = 0);
        bool storeConfig(uint8_t retries, uint16_t delay);

        // false = read version only
        // true = read config and version
        bool getConfig(bool config);

    private:
        void _rescheduleNextRetry(Event::CallbackTimerPtr timer);
        void _readConfigTimer(Event::CallbackTimerPtr timer);

    private:
        TwoWire &_wire;
        Event::Timer _timer;
        Config _config;
        uint8_t _address;
        uint8_t _valid;
        uint8_t _retries;
        uint16_t _retryDelay;
        Callback _callback;
    };

    inline ConfigReaderWriter::~ConfigReaderWriter()
    {
        end();
    }

    inline void ConfigReaderWriter::begin()
    {
        _valid = 0;
        _Timer(_timer).remove();
    }

    inline void ConfigReaderWriter::end()
    {
        begin();
    }

    inline bool ConfigReaderWriter::isValid(VersionType version)
    {
        return version;
    }

    inline Config &ConfigReaderWriter::config()
    {
        return _config;
    }

    inline const Config &ConfigReaderWriter::config() const
    {
        return _config;
    }

    inline ConfigReaderWriter TwoWire::getConfigReader(uint8_t address)
    {
        return ConfigReaderWriter(*this, address);
    }

    inline ConfigReaderWriter TwoWire::getConfigWriter(uint8_t address)
    {
        return ConfigReaderWriter(*this, address);
    }

};

#include <debug_helper_disable.h>
