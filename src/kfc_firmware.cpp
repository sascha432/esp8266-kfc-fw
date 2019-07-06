/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <algorithm>
#include <vector>
#include <functional>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <OSTimer.h>
#include "kfc_fw_config.h"
#include "at_mode.h"
#include "serial_handler.h"
#include "reset_detector.h"
#include "plugins.h"
#if USE_GDBSTUB
//#include <GDBStub.h>
#endif

static bool wifi_connected = false;
unsigned long wifi_up = -1UL;
static unsigned long offline_since = 0;

#if SPIFFS_TMP_FILES_TTL

void cleanup_tmp_dir() {
    static MillisTimer timer(SPIFFS_TMP_CLEAUP_INTERVAL * 1000UL);
    if (timer.reached()) {
        ulong now = (millis() / 1000UL);
        String tmp_dir = sys_get_temp_dir();
        Dir dir = SPIFFS.openDir(tmp_dir);
#if DEBUG
        int deleted = 0;
#endif
        while(dir.next()) {
            String filename = dir.fileName();
            ulong ttl = strtoul(filename.substring(tmp_dir.length()).c_str(), nullptr, HEX);
            if (ttl && now > ttl) {
                if (SPIFFS.remove(dir.fileName())) {
#if DEBUG
                    deleted++;
#endif
                }
            }
        }
        // debug_printf_P(PSTR("Cleanup %s: Removed %d file(s)\n"), tmp_dir.c_str(), deleted);

        timer.restart();
    }
}
#endif

void check_flash_size() {

#if ESP8266
    uint32_t realSize = ESP.getFlashChipRealSize();
#endif
    uint32_t ideSize = ESP.getFlashChipSize();
    FlashMode_t ideMode = ESP.getFlashChipMode();

#if ESP32
    MySerial.printf_P(PSTR("Flash chip rev.: %08X\n"), ESP.getChipRevision());
#endif
#if ESP8266
    MySerial.printf_P(PSTR("Flash real id:   %08X\n"), ESP.getFlashChipId());
    MySerial.printf_P(PSTR("Flash real size: %u\n"), realSize);
#endif
    MySerial.printf_P(PSTR("Flash ide  size: %u\n"), ideSize);
    MySerial.printf_P(PSTR("Flash ide speed: %u\n"), ESP.getFlashChipSpeed());
    MySerial.printf_P(PSTR("Flash ide mode:  %s\n"), (ideMode == FM_QIO ? str_P(F("QIO")) : ideMode == FM_QOUT ? str_P(F("QOUT")) : ideMode == FM_DIO ? str_P(F("DIO")) : ideMode == FM_DOUT ? str_P(F("DOUT")) : str_P(F("UNKNOWN"))));

#if ESP8266
    if (ideSize != realSize) {
        MySerial.printf_P(PSTR("Flash Chip configuration wrong!\n\n"));
    } else {
        MySerial.printf_P(PSTR("Flash Chip configuration ok.\n\n"));
    }
#endif
}

void setup_wifi_callbacks() {

    static auto _connectedHandler = WiFi.onStationModeConnected([](const WiFiEventStationModeConnected &event) {
        if (!wifi_connected) {
            String str = formatTime((millis() - offline_since) / 1000, true);
            Logger_notice(F("WiFi connected to %s (offline for %s)"), event.ssid.c_str(), str.c_str());
            debug_printf_P(PSTR("Station: WiFi connected to %s, offline for %s\n"), event.ssid.c_str(), str.c_str());
            debug_printf_P(PSTR("Free heap %s\n"), formatBytes(ESP.getFreeHeap()).c_str());
            wifi_connected = true;
        }
    });

    static auto _gotIpHJandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP &event) {
        Logger_notice(F("%s: IP/Net %s/%s GW %s DNS: %s, %s"), wifi_station_dhcpc_status() == DHCP_STOPPED ? "Static configuration" : "DHCP", to_c_str(WiFi.localIP()), to_c_str(WiFi.subnetMask()), to_c_str(WiFi.gatewayIP()), to_c_str(WiFi.dnsIP()), to_c_str(WiFi.dnsIP(1)));
        debug_printf_P(PSTR("Station: Got IP %s\n"), to_c_str(WiFi.localIP()));
        config_set_blink(BLINK_SOLID);
        wifi_up = millis();
        _systemStats.station_reconnects++;
        // time_to_sleep = millis() + 5000; // allow deep sleep 5 seconds after WiFi has been connected

        wifi_set_sleep_type(MODEM_SLEEP_T);

        WiFiCallbacks::callEvent(WiFiCallbacks::EventEnum_t::CONNECTED, (void *)&event);
    });

    static auto _disconnectedHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
        if (wifi_connected) {
            config_set_blink(BLINK_FAST);
            unsigned long duration = wifi_up == -1UL ? 0 : wifi_up - millis();
            String str = formatTime(duration / 1000, true);
            Logger_notice(F("WiFi disconnected, reason %s, (online for %s)"), WiFi_disconnect_reason(event.reason).c_str(), str.c_str());
            debug_printf_P(PSTR("Station: WiFi disconnected from %s, reason %s, online for %s\n"), event.ssid.c_str(), WiFi_disconnect_reason(event.reason).c_str(), str.c_str());
            offline_since = millis();
            wifi_connected = false;
            wifi_up = -1UL;
            WiFiCallbacks::callEvent(WiFiCallbacks::EventEnum_t::DISCONNECTED, (void *)&event);
        }
    });

    static auto _stationModeDHCPTimeout = WiFi.onStationModeDHCPTimeout([]() {
        Logger_notice(F("DHCP timeout"));
        debug_println(F("Station: DHCP timeout"));
        config_set_blink(BLINK_FLICKER);
    });

    static auto _softAPModeStationConnected = WiFi.onSoftAPModeStationConnected([](const WiFiEventSoftAPModeStationConnected &event) {
        Logger_notice(F("Station connected [" MACSTR "]"), MAC2STR(event.mac));
        debug_printf_P(PSTR("AP: Station connected [" MACSTR "]\n"), MAC2STR(event.mac));
        _systemStats.ap_clients_connected++;
    });

    static auto _softAPModeStationDisconnected = WiFi.onSoftAPModeStationDisconnected([](const WiFiEventSoftAPModeStationDisconnected &event) {
        Logger_notice(F("Station disconnected [" MACSTR "]"), MAC2STR(event.mac));
        debug_printf(PSTR("AP: Station disconnected [" MACSTR "]\n"), MAC2STR(event.mac));
    });
}

#if DEBUG
class WatchDogTimer : public OSTimer {
public:
    WatchDogTimer() : OSTimer() {
    }

    virtual void run() {
        static const char *spinner = "-/|\\-/|\\";
        _count++;
        if (_count % 2 == 0) {
            Serial.print(spinner[(_count / 2) % 8]);
            Serial.print('\b');
        }
    }

    void setCount(int count) {
        _count = count;
    }

private:
    int _count = 0;
};

WatchDogTimer myWdt;
#endif

bool safe_mode = false;

void setup() {

    Serial.begin(115200);
    Serial.println(F("Booting KFC firmware..."));

#if SPIFFS_SUPPORT
    SPIFFS.begin();
#endif
#if SYSTEM_STATS
    system_stats_read();
#endif

    KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("safe mode %d reset counter %d\n"), resetDetector.getSafeMode(), resetDetector.getResetCounter());

    if (resetDetector.getSafeMode()) {

        safe_mode = true;

        KFC_SAFE_MODE_SERIAL_PORT.println(F("Starting in safe mode..."));
        delay(2000);
        resetDetector.setSafeMode(false);

    } else {

        if (resetDetector.getResetCounter() > 2) {
            KFC_SAFE_MODE_SERIAL_PORT.println(F("Triple resets detected. Restoring factory defaults..."));
            config_write(true);
        }

        if (resetDetector.getResetCounter() > 1) {

            KFC_SAFE_MODE_SERIAL_PORT.println(F("Multiple resets detected. Reboot continues in 20 seconds..."));
            KFC_SAFE_MODE_SERIAL_PORT.println(F("Press reset again to start in safe mode."));

            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("\nCrash detected: %d\nReset counter: %d\n\n"), resetDetector.hasCrashDetected(), resetDetector.getResetCounter());

            config_set_blink(BLINK_SOS);
            resetDetector.setSafeMode(1);

            uint16_t count = 20;
            while(count--) {
                KFC_SAFE_MODE_SERIAL_PORT.print('.');
                delay(1000);
            }
            // reset has not been pressed, disable safe mode
            resetDetector.setSafeMode(false);
        }

    }

    config_read(true);
    if (safe_mode) {

        MySerialWrapper.replace(&KFC_SAFE_MODE_SERIAL_PORT, true);
        DebugSerial = MySerialWrapper;

        #if SERIAL_HANDLER
            serialHandler.begin();
        #endif

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        setup_plugins(true);
        setup_wifi_callbacks();

        config_setup();
        config_info();
        if (!config_apply_wifi_settings()) {
            MySerial.printf_P(PSTR("ERROR - %s\n"), _Config.getLastError());
        }

    } else {

#if SERIAL_HANDLER
        serialHandler.begin();
#endif

#if AT_MODE_SUPPORTED
        at_mode_setup();
#endif

        if (resetDetector.hasCrashDetected()) {
            Logger_error(F("System crash detected: %s"), resetDetector.getResetInfo().c_str());
            debug_printf_P(PSTR("System crash detected: %s\n"), resetDetector.getResetInfo().c_str());
            #if SYSTEM_STATS
                _systemStats.crash_counter++;
            #endif
        }

#if SYSTEM_STATS
        if (resetDetector.hasResetDetected() || resetDetector.hasRebootDetected()) {
            _systemStats.reset_counter++;
        }
        _systemStats.reboot_counter++;
        system_stats_update();
#endif

#if DEBUG
        check_flash_size();
        debug_printf_P(PSTR("Free Sketch Space %u\n"), ESP.getFreeSketchSpace());
        debug_printf_P(PSTR("CPU frequency %d\n"), system_get_cpu_freq());
#endif

#if SPIFFS_CLEANUP_TMP_DURING_BOOT
        debug_println(F("Cleaning up /tmp directory"));
        Dir dir = SPIFFS.openDir(sys_get_temp_dir());
        while(dir.next()) {
            bool status = SPIFFS.remove(dir.fileName());
            debug_printf_P(PSTR("Remove(%s) = %d\n"), dir.fileName().c_str(), status);
        }
#endif
#if SPIFFS_TMP_FILES_TTL
        LoopFunctions::add(cleanup_tmp_dir);
#endif

        setup_plugins(false);
        setup_wifi_callbacks();
        config_setup();

        config_info();
        if (!config_apply_wifi_settings()) {
            MySerial.printf_P(PSTR("ERROR - %s\n"), _Config.getLastError());
            config_dump(MySerial);
        }
    }

#if DEBUG
    // myWdt.startTimer(100, true); // dummy spinner for the serial port
#endif
}

void loop() {
    auto &loopFunctions = LoopFunctions::getVector();
    for(uint8_t i = 0; i < loopFunctions.size(); i++) { // do not use iterators since the vector can be modifed inside the callback
        if (loopFunctions[i].deleteCallback) {
            loopFunctions.erase(loopFunctions.begin() + i);
            i--;
        } else if (loopFunctions[i].callback) {
            loopFunctions[i].callback();
        } else {
            reinterpret_cast<void(*)()>(loopFunctions[i].callbackPtr)();
        }
    }
}
