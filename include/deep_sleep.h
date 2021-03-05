/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>
#include <chrono>
#include <chrono>


namespace DeepSleep
{
    using milliseconds = std::chrono::duration<uint32_t, std::milli>;
    using seconds = std::chrono::duration<uint32_t, std::ratio<1>>;
    using minutes = std::chrono::duration<uint32_t, std::ratio<60>>;

    static constexpr auto kExample = std::chrono::duration_cast<DeepSleep::milliseconds>(DeepSleep::minutes(5)).count();

    struct __attribute__packed__ WiFiQuickConnect
    {
        int16_t channel: 15;                    //  0
        int16_t use_static_ip: 1;               // +2 byte
        struct Uint8Array {
            uint8_t _data[WL_MAC_ADDR_LENGTH];      // +6 byte
            operator uint8_t *() {
                return _data;
            }
            operator const uint8_t *() const {
                return _data;
            }
            Uint8Array &operator=(const uint8_t *data) {
                std::copy_n(data, WL_MAC_ADDR_LENGTH, _data);
                return *this;
            }
            Uint8Array() = default;
        } bssid;

        uint32_t local_ip;
        uint32_t dns1;
        uint32_t dns2;
        uint32_t subnet;
        uint32_t gateway;

        WiFiQuickConnect() :
            channel(0),
            use_static_ip(0),
            bssid({}),
            local_ip(0),
            dns1(0),
            dns2(0),
            subnet(0),
            gateway(0)
        {}

    };

    struct __attribute__packed__ DeepSleepParam {

        uint32_t _totalSleepTime;                   // total amount in milliseconds
        uint32_t _remainingSleepTime;               // time left in millis
        uint32_t _currentSleepTime;                 // the current cycle just woken up from
        uint32_t _runtime;                          // total runtime in microseconds during wakeups
        uint16_t _counter;                          // total count of deep sleep cycles for the total amount
        RFMode _mode;


        DeepSleepParam() = default;
        DeepSleepParam(minutes deepSleepTime, RFMode mode = RF_NO_CAL) : DeepSleepParam(std::chrono::duration_cast<milliseconds>(deepSleepTime), mode) {}
        DeepSleepParam(seconds deepSleepTime, RFMode mode = RF_NO_CAL) : DeepSleepParam(std::chrono::duration_cast<milliseconds>(deepSleepTime), mode) {}
        DeepSleepParam(milliseconds deepSleepTime, RFMode mode = RF_NO_CAL) :
            _totalSleepTime(deepSleepTime.count()),
            _remainingSleepTime(deepSleepTime.count()),
            _currentSleepTime(0),
            _runtime(0),
            _counter(0),
            _mode(mode)
        {
        }

        bool isUnlimited() const {
            return _totalSleepTime == ~0U;
        }

        uint32_t getDeepSleepMaxMillis() const {
            return ESP.deepSleepMax() / (1000 + 150/*some extra margin*/);
        }

        // returns the time to sleep during next cycle and updates the member "_remainingSleepTime"
        uint32_t updateRemainingTime() {
            auto maxSleep = getDeepSleepMaxMillis();
            if (_remainingSleepTime / 2 >= maxSleep) {
                _remainingSleepTime -= maxSleep;
                return maxSleep;
            }
            else if (_remainingSleepTime >= maxSleep) {
                _remainingSleepTime /= 2;
                return _remainingSleepTime;
            }
            else {
                maxSleep = _remainingSleepTime;
                _remainingSleepTime = 0;
                return maxSleep;
            }
        }

        uint64_t getDeepSleepTimeMicros() const {
            return _currentSleepTime * 1000ULL;
        }

    };

};
