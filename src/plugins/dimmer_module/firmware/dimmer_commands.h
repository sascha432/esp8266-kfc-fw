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

        void fadeTo(uint8_t channel, int16_t fromLevel, int16_t toLevel, float fadeTime, uint8_t address = kDefaultSlaveAddress) {
            if (lock()) {
                beginTransmission(address);
                write(dimmer_command_fade_t(channel, fromLevel, toLevel, fadeTime));
                endTransmission();
                unlock();
            }
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

        void forceTemperatureCheck(uint8_t address = kDefaultSlaveAddress) {
            if (lock()) {
                beginTransmission(address);
                write(DIMMER_REGISTER_COMMAND);
                write(DIMMER_COMMAND_FORCE_TEMP_CHECK);
                endTransmission();
                unlock();
            }
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
                    (requestFrom(DIMMER_I2C_ADDRESS, sizeof(metrics)) == sizeof(metrics)) &&
                    (read(metrics) == sizeof(metrics))
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
        static constexpr uint8_t kStopped = 0x80;

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
        }


        ConfigReaderWriter(TwoWire &wire, uint8_t address = kDefaultSlaveAddress);
        ~ConfigReaderWriter();

        void begin();
        void end();

        static bool isValid(VersionType version);
        Config &config();

        // retries = number of retries
        // retryDelay = delay between retries in milliseconds
        // callback = retry in background and invoke callback when done
        // initialDelay = initial delay when running in background
        // Note:
        // if not running in background, retries * retryDelay should not exceed 500ms
        // if executed inside an ISR / can_yield() is false, retries is set to 0
        bool readConfig(uint8_t retries, uint16_t retryDelay, Callback callback = nullptr, uint16_t initialDelay = 100);
        bool storeConfig(uint8_t retries, uint16_t delay);

        bool getVersion();
        bool getConfig();

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
