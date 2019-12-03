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
#include <PrintString.h>
#include "kfc_fw_config.h"
#include "blink_led_timer.h"
#include "at_mode.h"
#include "serial_handler.h"
#include "reset_detector.h"
#include "progmem_data.h"
#include "plugins.h"
#if PRINTF_WRAPPER_ENABLED
#include <printf_wrapper.h>
#endif
// #include <GDBStub.h>
// extern "C" void gdbstub_do_break();


#if SPIFFS_TMP_FILES_TTL

void cleanup_tmp_dir() {
    static MillisTimer timer(SPIFFS_TMP_CLEAUP_INTERVAL * 1000UL);
    if (timer.reached()) {
        ulong now = (millis() / 1000UL);
        String tmp_dir = sys_get_temp_dir();
        Dir dir = SPIFFS_openDir(tmp_dir);
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

static void deep_sleep_forever() {
    for(;;) {
#if defined(ESP8266)
        ESP.deepSleep(ESP.deepSleepMax() / 2, WAKE_RF_DISABLED); // NOTE using ESP.deepSleepMax() reports "too long"
#else
        ESP.deepSleep(UINT32_MAX);
#endif


        delay(1);
    }
}

// #include "ESPAsyncWebServer.h"

// String callback_func(const String &str) {
//     if (str == "REPLACE_ME") {
//         return "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
//     }
//     return String();
// }

// class TestAsyncAbstractResponse : public AsyncAbstractResponse {
// public:
//     TestAsyncAbstractResponse(AwsTemplateProcessor callback) : AsyncAbstractResponse(callback) {
//         _template = "01234567890test_%REPLACE_ME%_test01234567890";
//         _code = 200;
//     }
//     virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) {
//         maxLen = std::min(_template.length(), maxLen);
//         memcpy(buf, _template.c_str(), maxLen);
//         _template.remove(0, maxLen);
//         return maxLen;
//     }
// private:
//     String _template;
// };

// void test_abstract_response() {
//     TestAsyncAbstractResponse *resp = new TestAsyncAbstractResponse(callback_func);
//     char *out = (char *)malloc(1024);
//     char *ptr = out;
//     uint8_t buf[64];
//     size_t len;

//     Serial.println();
//     while((len = resp->_fillBufferAndProcessTemplates(buf, sizeof(buf)))) {
//         Serial.printf("[%u]|%*.*s|\n", len, len, len, buf);
//         memcpy(ptr, buf, len);
//         ptr += len;
//     }
//     *ptr = 0;
//     Serial.println("---");
//     Serial.printf("[%u]|%s|\n", strlen(out), out);
//     free(out);
// }


void remove_crash_counter_file() {
#if SPIFFS_SUPPORT
    SPIFFS.begin();
    SPIFFS.remove(FSPGM(crash_counter_file));
#endif
}

void setup() {

    Serial.begin(KFC_SERIAL_RATE);
    DEBUG_HELPER_INIT();

#if DEBUG_RESET_DETECTOR
    resetDetector._init();
#endif

    // gdbstub_do_break();
    // disable_at_mode(Serial);

    if (resetDetector.getResetCounter() >= 20) {
        delay(5000);    // delay boot if too many resets are detected
        resetDetector.armTimer();
    }
    Serial.println(F("Booting KFC firmware..."));

    if (resetDetector.hasWakeUpDetected()) {
        config.wakeUpFromDeepSleep();
        resetDetector.clearCounter();
    }
    Serial.printf_P(PSTR("SAFE MODE %d, reset counter %d, wake up %d\n"), resetDetector.getSafeMode(), resetDetector.getResetCounter(), resetDetector.hasWakeUpDetected());

    if (resetDetector.hasResetDetected()) {

        // if (resetDetector.getResetCounter() >= 10) {
        //     KFC_SAFE_MODE_SERIAL_PORT.println(F("Entering deep sleep until next reset in 5 seconds..."));
        //     for(uint8_t i = 0; i < (RESET_DETECTOR_TIMEOUT + 200) / (10 + 25); i++) {
        //         BlinkLEDTimer::setBlink(LED_BUILTIN, BlinkLEDTimer::SOLID);
        //         delay(10);
        //         BlinkLEDTimer::setBlink(LED_BUILTIN, BlinkLEDTimer::OFF);
        //         delay(25);
        //     }
        //     deep_sleep_forever();
        // }

        if (resetDetector.getResetCounter() >= 4) {
            KFC_SAFE_MODE_SERIAL_PORT.println(F("4x reset detected. Restoring factory defaults in a 5 seconds..."));
            for(uint8_t i = 0; i < (RESET_DETECTOR_TIMEOUT + 500) / (100 + 250); i++) {
                BlinkLEDTimer::setBlink(LED_BUILTIN, BlinkLEDTimer::SOLID);
                delay(100);
                BlinkLEDTimer::setBlink(LED_BUILTIN, BlinkLEDTimer::OFF);
                delay(250);
            }
            config.restoreFactorySettings();
            config.write();
            resetDetector.setSafeMode(false);
        }

    }

    bool safe_mode = false;
    if (resetDetector.getSafeMode()) {

        safe_mode = true;

        KFC_SAFE_MODE_SERIAL_PORT.println(F("Starting in safe mode..."));
        delay(2000);
        resetDetector.clearCounter();
        resetDetector.setSafeMode(false);

    } else {

        if (resetDetector.getResetCounter() > 1) {

            KFC_SAFE_MODE_SERIAL_PORT.println(F("Multiple resets detected. Reboot continues in 20 seconds..."));
            KFC_SAFE_MODE_SERIAL_PORT.println(F("Press reset again to start in safe mode."));
            KFC_SAFE_MODE_SERIAL_PORT.println(F("\nTo restore factory defaults, press reset once a second until the LED starts to flash. After 5 seconds the normal boot process continues. To put the device to deep sleep until next reset, continue to press reset till the LED starts to flicker"));

            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("\nCrash detected: %d\nReset counter: %d\n\n"), resetDetector.hasCrashDetected(), resetDetector.getResetCounter());
#if DEBUG
            KFC_SAFE_MODE_SERIAL_PORT.println(F("\nAvailable keys:\n"));
            KFC_SAFE_MODE_SERIAL_PORT.println(F(
                "    h: Halt device and go to deep sleep\n"
                "    l: Enter wait loop\n"
                "    s: reboot in safe mode\n"
                "    r: reboot in normal mode\n"
                "    f: restore factory settings\n"
                "    c: clear RTC memory\n"
                //"    o: reboot in normal mode, start heap timer and block WiFi\n"
            ));
#endif

            BlinkLEDTimer::setBlink(LED_BUILTIN, BlinkLEDTimer::SOS);
            resetDetector.clearCounter();
            resetDetector.setSafeMode(1);

            if (
#if DEBUG
            __while(20e3, []() {
                if (Serial.available()) {
                    switch(Serial.read()) {
                        case 'c':
                            RTCMemoryManager::clear();
                            remove_crash_counter_file();
                            KFC_SAFE_MODE_SERIAL_PORT.println(F("RTC memory cleared"));
                            break;
                        case 'h':
                            deep_sleep_forever();
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
                            remove_crash_counter_file();
                            return false;
                        case 's':
                            resetDetector.setSafeMode(1);
                            resetDetector.clearCounter();
                            remove_crash_counter_file();
                            return false;
                    }
                }
                return true;
            }, 1000, []() {
                KFC_SAFE_MODE_SERIAL_PORT.print('.');
                return true;
            })
#else
            __while(20e3, nullptr, 1000, []() {
                KFC_SAFE_MODE_SERIAL_PORT.print('.');
                return true;
            })
#endif
            ) {
                // timeout occured, disable safe mode
                resetDetector.setSafeMode(false);
            }
            KFC_SAFE_MODE_SERIAL_PORT.println();
        }

    }

#if SPIFFS_SUPPORT
    SPIFFS.begin();
    if (resetDetector.hasCrashDetected()) {
        File file = SPIFFS.open(FSPGM(crash_counter_file), "r");
        char counter = 0;
        if (file) {
            counter = file.read() + 1;
            file.close();
        }
        file = SPIFFS.open(FSPGM(crash_counter_file), "w");
        file.write(counter);
        file.close();
        if (counter >= 3) {  // boot in safe mode if there were 3 crashes within the first 5min.
            resetDetector.setSafeMode(1);
        }
    }
#endif

    config.read();
    if (safe_mode) {

        MySerialWrapper.replace(&KFC_SAFE_MODE_SERIAL_PORT, true);
        DebugSerial = MySerialWrapper;

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        prepare_plugins();
        setup_plugins(PluginComponent::PLUGIN_SETUP_SAFE_MODE);

        // check if wifi is up once per minute
        Scheduler.addTimer(60e3, true, [](EventScheduler::TimerPtr timer) {
            if (!WiFi.isConnected()) {
                config.reconfigureWiFi();
            }
        });

    } else {


        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        if (resetDetector.hasCrashDetected()) {
            PrintString message;
            message.printf_P(PSTR("System crash detected: %s\n"), resetDetector.getResetInfo().c_str());
            Logger_error(message);
            debug_println(message);
        }

#if DEBUG
        if (!resetDetector.hasWakeUpDetected()) {
            check_flash_size();
            debug_printf_P(PSTR("Free Sketch Space %u\n"), ESP.getFreeSketchSpace());
#if defined(ESP8266)
            debug_printf_P(PSTR("CPU frequency %d\n"), system_get_cpu_freq());
#endif
        }
#endif

#if SPIFFS_CLEANUP_TMP_DURING_BOOT
        if (!resetDetector.hasWakeUpDetected()) {
            debug_println(F("Cleaning up /tmp directory"));
            Dir dir = SPIFFS_openDir(sys_get_temp_dir());
            while(dir.next()) {
                bool status = SPIFFS.remove(dir.fileName());
                debug_printf_P(PSTR("Remove(%s) = %d\n"), dir.fileName().c_str(), status);
            }
        }
#endif
#if SPIFFS_TMP_FILES_TTL
        LoopFunctions::add(cleanup_tmp_dir);
#endif

        prepare_plugins();
        setup_plugins(resetDetector.hasWakeUpDetected() ? PluginComponent::PLUGIN_SETUP_AUTO_WAKE_UP : PluginComponent::PLUGIN_SETUP_DEFAULT);

#if SPIFFS_SUPPORT
        Scheduler.addTimer(300e3, false, [](EventScheduler::TimerPtr timer) { // remove file after 5min.
            SPIFFS.remove(FSPGM(crash_counter_file));
        });
#endif

    }
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
            loopFunctions[i].callbackPtr();
        }
    }
}
