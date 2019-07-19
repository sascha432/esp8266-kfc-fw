/**
 * Author: sascha_lammers@gmx.de
 */

#include "kfc_fw_config.h"
#include <EEPROM.h>
#include <KFCTimezone.h>
#include <time.h>
#include <OSTimer.h>
#include <EventScheduler.h>
// #include <LoopFunctions.h>
// #include <WiFiCallbacks.h>
#include <crc16.h>
#include "progmem_data.h"
#include "fs_mapping.h"
#include "misc.h"
#include "reset_detector.h"
#include "plugins.h"

#if DEBUG_CONFIG
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class BlinkLEDTimer : public OSTimer {  // PIN is inverted
public:
    BlinkLEDTimer() : OSTimer() {
        _pin = -1;
    }

    virtual void run() override {
        digitalWrite(_pin, _pattern.test(_counter++ % _pattern.size()) ? LOW : HIGH);
    }

    void set(uint32_t delay, int8_t pin, dynamic_bitset &pattern) {
        _pattern = pattern;
        if (_pin != pin) {
            _pin = pin;
            pinMode(_pin, OUTPUT);
            analogWrite(pin, 0);
            digitalWrite(pin, HIGH);
        }
        _counter = 0;
        startTimer(delay, true);
    }

    void detach() {
        if (_pin != -1) {
            analogWrite(_pin, 0);
            digitalWrite(_pin, HIGH);
        }
        OSTimer::detach();
    }

private:
    int8_t _pin;
    uint16_t _counter;
    dynamic_bitset _pattern;
};

static BlinkLEDTimer *led_timer = _debug_new BlinkLEDTimer();

void blink_led(int8_t pin, int delay, dynamic_bitset &pattern) {

    MySerial.printf_P(PSTR("blink_led pin %d, pattern %s, delay %d\n"), pin, pattern.toString().c_str(), delay);

    led_timer->detach();
    led_timer->set(delay, pin, pattern);
}

void config_set_blink(uint16_t delay, int8_t pin) { // PIN is inverted

    auto flags = config._H_GET(Config().flags);

    if (pin == -1) {
        auto ledPin = config._H_GET(Config().led_pin);
        _debug_printf_P(PSTR("Using configured LED type %d, PIN %d, blink %d\n"), flags.ledMode, ledPin, delay);
        pin = (flags.ledMode != MODE_NO_LED && ledPin != -1) ? ledPin : -1;
    } else {
        _debug_printf_P(PSTR("PIN %d, blink %d\n"), pin, delay);
    }

    led_timer->detach();

    // reset pin
    analogWrite(pin, 0);
    digitalWrite(pin, LOW);

    if (pin >= 0) {
        if (delay == BLINK_OFF) {
            digitalWrite(pin, HIGH);
        } else if (delay == BLINK_SOLID) {
            digitalWrite(pin, LOW);
        } else {
            dynamic_bitset pattern;
            if (delay == BLINK_SOS) {
                pattern.setMaxSize(24);
                pattern.setValue(0xcc1c71d5);
                delay = 200;
            } else {
                pattern.setMaxSize(2);
                pattern = 0b10;
                delay = _max(50, _min(delay, 5000));
            }
            led_timer->set(delay, pin, pattern);
        }
    }
}

uint8_t WiFi_mode_connected(uint8_t mode, uint32_t *station_ip, uint32_t *ap_ip) {
    struct ip_info ip_info;
    uint8_t connected_mode = 0;

    auto flags = config._H_GET(Config().flags);
    if (flags.wifiMode & mode) { // is any modes active?
        if ((mode & WIFI_STA) && flags.wifiMode & WIFI_STA && wifi_station_get_connect_status() == STATION_GOT_IP) { // station connected?
            if (wifi_get_ip_info(STATION_IF, &ip_info) && ip_info.ip.addr) { // verify that is has a valid IP address
                connected_mode |= WIFI_STA;
                if (station_ip) {
                    *station_ip = ip_info.ip.addr;
                }
            }
        }
        if ((mode & WIFI_AP) && flags.wifiMode & WIFI_AP) { // AP mode active?
            if (wifi_get_ip_info(SOFTAP_IF, &ip_info) && ip_info.ip.addr) { // verify that is has a valid IP address
                connected_mode |= WIFI_AP;
                if (ap_ip) {
                    *ap_ip = ip_info.ip.addr;
                }
            }
        }
    }
    return connected_mode;
}

void config_init() {
    _debug_printf_P(PSTR("config_init(): safe mode %d, wake up %d\n"), resetDetector.getSafeMode(), resetDetector.hasWakeUpDetected());

    config.setup();

    // during wake up, the WiFI is already configured at this point
    if (!resetDetector.hasWakeUpDetected()) {
        config.printInfo(MySerial);

        if (!config.connectWiFi()) {
            MySerial.printf_P(PSTR("ERROR - %s\n"), config.getLastError());
#if DEBUG
            config.dump(MySerial);
#endif
        }
    }
}

void config_reconfigure() {
    config.reconfigureWiFi();
}

PROGMEM_PLUGIN_CONFIG_DEF(
/* pluginName               */ cfg,
/* setupPriority            */ PLUGIN_PRIO_CONFIG,
/* allowSafeMode            */ false,
/* autoSetupWakeUp          */ true,
/* rtcMemoryId              */ CONFIG_RTC_MEM_ID,
/* setupPlugin              */ config_init,
/* statusTemplate           */ nullptr,
/* configureForm            */ nullptr,
/* reconfigurePlugin        */ config_reconfigure,
/* reconfigure Dependencies */ nullptr,
/* prepareDeepSleep         */ nullptr,
/* atModeCommandHandler     */ nullptr
);
