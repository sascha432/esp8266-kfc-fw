/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <algorithm>
#include <vector>
#include <functional>
#include <LoopFunctions.h>
#include <EventTimer.h>
#include <WiFiCallbacks.h>
#include <PrintString.h>
#include <ListDir.h>
#include "kfc_fw_config.h"
#include "blink_led_timer.h"
#include "at_mode.h"
#include "serial_handler.h"
#include "reset_detector.h"
#include "plugins.h"
#include "WebUIAlerts.h"
#if PRINTF_WRAPPER_ENABLED
#include <printf_wrapper.h>
#endif
#if HAVE_GDBSTUB
#error uncomment the line below
// #include <GDBStub.h>
extern "C" void gdbstub_do_break();
#endif
#if 0
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if SPIFFS_TMP_FILES_TTL

void cleanup_tmp_dir()
{
    static MillisTimer timer(SPIFFS_TMP_CLEAUP_INTERVAL * 1000UL);
    if (timer.reached()) {
        ulong now = (millis() / 1000UL);
        String tmp_dir = sys_get_temp_dir();
        auto dir = ListDir(tmp_dir);
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
        _debug_printf_P(PSTR("Cleanup %s: Removed %d file(s)\n"), tmp_dir.c_str(), deleted);

        timer.restart();
    }
}
#endif

void check_flash_size()
{
#if defined(ESP8266)
    uint32_t realSize = ESP.getFlashChipRealSize();
#endif
    uint32_t ideSize = ESP.getFlashChipSize();
    FlashMode_t ideMode = ESP.getFlashChipMode();

#if defined(ESP32)
    MySerial.printf_P(PSTR("Flash chip rev.: %08X\n"), ESP.getChipRevision());
#endif
#if defined(ESP8266)
    MySerial.printf_P(PSTR("Flash real id:   %08X\n"), ESP.getFlashChipId());
    MySerial.printf_P(PSTR("Flash real size: %u\n"), realSize);
#endif
    MySerial.printf_P(PSTR("Flash ide  size: %u\n"), ideSize);
    MySerial.printf_P(PSTR("Flash ide speed: %u\n"), ESP.getFlashChipSpeed());
    MySerial.printf_P(PSTR("Flash ide mode:  %s\n"), (ideMode == FM_QIO ? PSTR("QIO") : ideMode == FM_QOUT ? PSTR("QOUT") : ideMode == FM_DIO ? PSTR("DIO") : ideMode == FM_DOUT ? PSTR("DOUT") : PSTR("UNKNOWN")));

#if defined(ESP8266)
    if (ideSize != realSize) {
        MySerial.printf_P(PSTR("Flash Chip configuration wrong!\n\n"));
    } else {
        MySerial.printf_P(PSTR("Flash Chip configuration ok.\n\n"));
    }
#endif
}

void setup()
{
    Serial.begin(KFC_SERIAL_RATE);
    DEBUG_HELPER_INIT();

#if DEBUG_RESET_DETECTOR
    resetDetector._init();
#endif
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);

#if HAVE_GDBSTUB
    gdbstub_do_break();
    disable_at_mode(Serial);
#endif

    if (resetDetector.getResetCounter() >= 20) {
        delay(5000);    // delay boot if too many resets are detected
        resetDetector.armTimer();
    }
    Serial.println(F("Booting KFC firmware..."));

#if ENABLE_DEEP_SLEEP
    if (resetDetector.hasWakeUpDetected()) {
        config.wakeUpFromDeepSleep();
        resetDetector.clearCounter();
    }
#endif
    Serial.printf_P(PSTR("SAFE MODE %d, reset counter %d, wake up %d\n"), resetDetector.getSafeMode(), resetDetector.getResetCounter(), resetDetector.hasWakeUpDetected());
    config.setSafeMode(resetDetector.getSafeMode());

    if (resetDetector.hasResetDetected()) {
        if (resetDetector.getResetCounter() >= 4) {
            KFC_SAFE_MODE_SERIAL_PORT.println(F("4x reset detected. Restoring factory defaults in a 5 seconds..."));
            for(uint8_t i = 0; i < (RESET_DETECTOR_TIMEOUT + 500) / (100 + 250); i++) {
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOLID);
                delay(100);
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
                delay(250);
            }
            config.restoreFactorySettings();
            config.write();
            resetDetector.setSafeMode(false);
        }
    }

    bool safe_mode = false;
    bool incr_crash_counter = false;
    if (resetDetector.getSafeMode()) {

        safe_mode = true;

        KFC_SAFE_MODE_SERIAL_PORT.println(F("Starting in safe mode..."));
        delay(2000);
        resetDetector.clearCounter();
        resetDetector.setSafeMode(false);
        config.setSafeMode(true);

    } else {

        if (resetDetector.getResetCounter() > 1) {

            KFC_SAFE_MODE_SERIAL_PORT.println(F("Multiple resets detected. Reboot continues in 20 seconds..."));
            KFC_SAFE_MODE_SERIAL_PORT.println(F("Press reset again to start in safe mode."));
            KFC_SAFE_MODE_SERIAL_PORT.println(F("\nTo restore factory defaults, press reset once a second until the LED starts to flash. After 5 seconds the normal boot process continues. To put the device to deep sleep until next reset, continue to press reset till the LED starts to flicker"));

            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("\nCrash detected: %d\nReset counter: %d\n\n"), resetDetector.hasCrashDetected(), resetDetector.getResetCounter());
#if DEBUG
            KFC_SAFE_MODE_SERIAL_PORT.println(F("\nAvailable keys:\n"));
            KFC_SAFE_MODE_SERIAL_PORT.println(F(
                "    l: Enter wait loop\n"
                "    s: reboot in safe mode\n"
                "    r: reboot in normal mode\n"
                "    f: restore factory settings\n"
                "    c: clear RTC memory\n"
                //"    o: reboot in normal mode, start heap timer and block WiFi\n"
            ));
#endif

            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOS);
            resetDetector.setSafeMode(1);

            if (
#if DEBUG
            __while(10000, [&safe_mode]() {
                if (Serial.available()) {
                    switch(Serial.read()) {
                        case 'c':
                            RTCMemoryManager::clear();
                            SaveCrash::removeCrashCounter();
                            KFC_SAFE_MODE_SERIAL_PORT.println(F("RTC memory cleared"));
                            break;
                        case 'l':
                            __while(-1UL, []() {
                                return (Serial.read() != 'x');
                            }, 10e3, []() {
                                KFC_SAFE_MODE_SERIAL_PORT.println(F("Press 'x' to restart the device..."));
                                return true;
                            });
                            config.restartDevice();
                        case 'f':
                            config.restoreFactorySettings();
                            // continue switch
                        case 'r':
                            resetDetector.setSafeMode(false);
                            resetDetector.clearCounter();
                            SaveCrash::removeCrashCounter();
                            safe_mode = false;
                            config.setSafeMode(false);
                            return false;
                        case 's':
                            resetDetector.setSafeMode(1);
                            resetDetector.clearCounter();
                            safe_mode = true;
                            return false;
                    }
                }
                return true;
            }, 1000, []() {
                KFC_SAFE_MODE_SERIAL_PORT.print('.');
                return true;
            })
#else
            __while(5000, nullptr, 1000, []() {
                KFC_SAFE_MODE_SERIAL_PORT.print('.');
                return true;
            })
#endif
            ) {
                // timeout occured, disable safe mode
                resetDetector.setSafeMode(false);
                incr_crash_counter = true;
            }
            KFC_SAFE_MODE_SERIAL_PORT.println();
        }

    }

#if SPIFFS_SUPPORT
    SPIFFS.begin();
    if (resetDetector.hasCrashDetected() || incr_crash_counter) {
        uint8_t counter = SaveCrash::getCrashCounter();
        if (counter >= 3) {  // boot in safe mode if there were 3 crashes within the first minute
            resetDetector.setSafeMode(1);
        }
    }
#endif

    Scheduler.begin();

    config.read();
    if (safe_mode) {

        config.setSafeMode(true);
        WebUIAlerts_add(F("Running in Safe Mode"), AlertMessage::TypeEnum_t::DANGER, AlertMessage::ExpiresEnum_t::REBOOT);
        MySerialWrapper.replace(&KFC_SAFE_MODE_SERIAL_PORT, true);
        DebugSerial = MySerialWrapper;

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        prepare_plugins();
        setup_plugins(PluginComponent::SetupModeType::SAFE_MODE);

        // check if wifi is up
        Scheduler.addTimer(1000, true, [](EventScheduler::TimerPtr timer) {
            timer->rearm(60000);
            if (!WiFi.isConnected()) {
                _debug_println(F("WiFi not connected, restarting"));
                config.reconfigureWiFi();
            }
        });

        auto rebootDelay = KFCConfigurationClasses::System::Device::getSafeModeRebootTime();
        if (rebootDelay) {
            debug_printf_P(PSTR("rebooting in %u minutes\n"), rebootDelay);
            // restart device if running in safe mode for rebootDelay minutes
            Scheduler.addTimer(rebootDelay * 60000UL, false, [](EventScheduler::TimerPtr timer) {
                Logger_notice(F("Rebooting device after safe mode timeout"));
                config.restartDevice();
            });
        }

    } else {

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        if (resetDetector.hasCrashDetected()) {
            Logger_error(F("System crash detected: %s\n"), resetDetector.getResetInfo().c_str());
#if !WEBUI_ALERTS_SEND_TO_LOGGER
            WebUIAlerts_add(PrintString(F("System crash detected.<br>%s"), resetDetector.getResetInfo().c_str()), AlertMessage::TypeEnum_t::DANGER);
#endif
        }

#if DEBUG
#if ENABLE_DEEP_SLEEP
        if (!resetDetector.hasWakeUpDetected())
#endif
        {
            check_flash_size();
            _debug_printf_P(PSTR("Free Sketch Space %u\n"), ESP.getFreeSketchSpace());
#if defined(ESP8266)
            _debug_printf_P(PSTR("CPU frequency %d\n"), system_get_cpu_freq());
#endif
        }
#endif

#if SPIFFS_CLEANUP_TMP_DURING_BOOT
#if ENABLE_DEEP_SLEEP
        if (!resetDetector.hasWakeUpDetected())
#endif
        {
            _debug_println(F("Cleaning up /tmp directory"));
            auto dir = ListDir(sys_get_temp_dir());
            while(dir.next()) {
                _IF_DEBUG(bool status =) SPIFFS.remove(dir.fileName());
                _debug_printf_P(PSTR("remove=%s result=%d\n"), dir.fileName().c_str(), status);
            }
        }
#endif
#if SPIFFS_TMP_FILES_TTL
        LoopFunctions::add(cleanup_tmp_dir);
#endif

        prepare_plugins();

        setup_plugins(
#if ENABLE_DEEP_SLEEP
            resetDetector.hasWakeUpDetected() ?
                PluginComponent::SetupModeType::AUTO_WAKE_UP :
#endif
                PluginComponent::SetupModeType::DEFAULT
        );

        // check if wifi is up
        Scheduler.addTimer(60000, true, [](EventScheduler::TimerPtr timer) {
            auto flags = config._H_GET(Config().flags);
            if (flags.wifiMode & WIFI_STA) {
                if (!WiFi.isConnected()) {
                    // WiFi is down, wait 30 seconds if it reconnects automatically
                    Scheduler.addTimer(30000, false, [](EventScheduler::TimerPtr timer) {
                        if (!WiFi.isConnected()) {
                            // restart entire wifi subsystem
                            config.reconfigureWiFi();
                            Logger_notice(F("WiFi subsystem restarted"));
                        }
                    });
                }
            }
        });

#if SPIFFS_SUPPORT
        // reset crash counter after 3min
        SaveCrash::installRemoveCrashCounter(180);
#endif
    }

#if LOAD_STATISTICS
    load_avg_timer = millis() + 30000;
#endif

    _debug_println(F("end"));
}

#if LOAD_STATISTICS
unsigned long load_avg_timer = 0;
uint32_t load_avg_counter = 0;
float load_avg[3] = {0, 0, 0};
#endif

void loop()
{
    auto &loopFunctions = LoopFunctions::getVector();
    for(uint8_t i = 0; i < loopFunctions.size(); i++) { // do not use iterators since the vector can be modifed inside the callback
        if (loopFunctions[i].deleteCallback) {
            loopFunctions.erase(loopFunctions.begin() + i);
            i--;
        } else if (loopFunctions[i].callback) {
            loopFunctions[i].callback();
        } else {
            loopFunctions[i].callbackPtr();
        }
    }
#if LOAD_STATISTICS
    load_avg_counter++;
    if (millis() >= load_avg_timer) {
        load_avg_timer = millis() + 1000;
        if (load_avg[0] == 0) {
            load_avg[0] = load_avg_counter / 30;
            load_avg[1] = load_avg_counter / 30;
            load_avg[2] = load_avg_counter / 30;

        } else {
            load_avg[0] = ((load_avg[0] * 60) + load_avg_counter) / (60 + 1);
            load_avg[1] = ((load_avg[1] * 300) + load_avg_counter) / (300 + 1);
            load_avg[2] = ((load_avg[2] * 900) + load_avg_counter) / (900 + 1);

        }
        load_avg_counter = 0;
    }
#endif
#if ESP32
    run_scheduled_functions();
#endif
}
