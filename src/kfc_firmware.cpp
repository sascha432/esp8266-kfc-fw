/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <algorithm>
#include <vector>
#include <functional>
#include <Event.h>
#include <StreamWrapper.h>
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

// connect to wifi and serial2tcp before booting to see all debug output
#ifndef DEBUG_PRE_INIT_SERIAL2TCP
#define DEBUG_PRE_INIT_SERIAL2TCP       0
#endif

#if DEBUG_PRE_INIT_SERIAL2TCP
#include "../src/plugins/serial2tcp/serial2tcp.h"
#include "../src/plugins/serial2tcp/Serial2TcpBase.h"
#include "../src/plugins/serial2tcp/Serial2TcpClient.h"
#include "PluginComponent.h"
#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;

#if HAVE_KFC_BOOT_CHECK_FLASHSIZE
void check_flash_size()
{
#if defined(ESP8266)
    uint32_t realSize = ESP.getFlashChipRealSize();
#endif
    uint32_t ideSize = ESP.getFlashChipSize();
    FlashMode_t ideMode = ESP.getFlashChipMode();

#if defined(ESP32)
    Serial.printf_P(PSTR("Flash chip rev.: %08X\n"), ESP.getChipRevision());
#endif
#if defined(ESP8266)
    Serial.printf_P(PSTR("Flash real id:   %08X\n"), ESP.getFlashChipId());
    Serial.printf_P(PSTR("Flash real size: %u\n"), realSize);
#endif
    Serial.printf_P(PSTR("Flash ide  size: %u\n"), ideSize);
    Serial.printf_P(PSTR("Flash ide speed: %u\n"), ESP.getFlashChipSpeed());
    Serial.printf_P(PSTR("Flash ide mode:  %s\n"), (ideMode == FM_QIO ? PSTR("QIO") : ideMode == FM_QOUT ? PSTR("QOUT") : ideMode == FM_DIO ? PSTR("DIO") : ideMode == FM_DOUT ? PSTR("DOUT") : PSTR("UNKNOWN")));

#if defined(ESP8266)
    if (ideSize != realSize) {
        Serial.printf_P(PSTR("Flash Chip configuration wrong!\n\n"));
    } else {
        Serial.printf_P(PSTR("Flash Chip configuration ok.\n\n"));
    }
#endif
}
#endif

void setup()
{

    KFC_SAFE_MODE_SERIAL_PORT.begin(KFC_SERIAL_RATE);
#if KFC_DEBUG_USE_SERIAL1
    Serial1.begin(KFC_DEBUG_USE_SERIAL1);
    static_assert(KFC_DEBUG_USE_SERIAL1 >= 300, "must be set to the baud rate");
#endif
    serialHandler.begin();
    DEBUG_HELPER_INIT();

#if DEBUG_RESET_DETECTOR
    resetDetector._init();
#endif
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);

    if (resetDetector.getResetCounter() >= 20) {
        KFC_SAFE_MODE_SERIAL_PORT.println(F("Reboot continues in 5 seconds..."));
        // stop timer to avoid resetting counters
        resetDetector.disarmTimer();
        // delay boot if too many resets are detected
        delay(5000);
        resetDetector.armTimer();
    }

#if DEBUG_PRE_INIT_SERIAL2TCP
    #include "../include/retracted/custom_wifi.h"
    BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::FAST);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.enableSTA(true);
    Serial.printf_P(PSTR("WiFi.begin=%u\n"), WiFi.begin(CUSTOM_WIFI_SSID, CUSTOM_WIFI_PASSWORD));
    Serial.printf_P(PSTR("WiFi.reconnect=%u\n"), WiFi.reconnect());
    Serial.printf_P(PSTR("WiFi.waitForConnectResult=%u\n"), WiFi.waitForConnectResult());
    Serial.printf_P(PSTR("WiFi.connected=%u\n"), WiFi.isConnected());
    if (WiFi.isConnected()) {
        using Serial2TCP = KFCConfigurationClasses::Plugins::Serial2TCP;
        Serial2TCP::Serial2Tcp_t cfg = CUSTOM_SERIAL2TCP_CFG;
        auto instance = Serial2TcpBase::createInstance(cfg, CUSTOM_SERIAL2TCP_SERVER);
        instance->begin();
        delay(1000);
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
    }
    else {
        BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOS);
    }
#endif

#if HAVE_GDBSTUB
    gdbstub_do_break();
    disable_at_mode(Serial);
#endif

    KFC_SAFE_MODE_SERIAL_PORT.println(F("Booting KFC firmware..."));

#if ENABLE_DEEP_SLEEP
    if (resetDetector.hasWakeUpDetected()) {
        config.wakeUpFromDeepSleep();
        resetDetector.clearCounter();
    }
#endif
    KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("SAFE MODE %d, reset counter %d, wake up %d\n"), resetDetector.getSafeMode(), resetDetector.getResetCounter(), resetDetector.hasWakeUpDetected());

#if KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT
    if (resetDetector.hasResetDetected()) {
        if (resetDetector.getResetCounter() >= KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT) {
            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("%ux reset detected. Restoring factory defaults in a 5 seconds...\n"), KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT);
            for(uint8_t i = 0; i < (RESET_DETECTOR_TIMEOUT + 500) / (100 + 250); i++) {
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOLID);
                delay(100);
                BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::OFF);
                delay(250);
            }
            config.restoreFactorySettings();
            config.write();
            resetDetector.setSafeMode(false);
            resetDetector.clearCounter();
        }
    }
#endif

    bool safe_mode = false;
    bool increaseCrashCounter = false;
    if (resetDetector.getSafeMode()) {

        KFC_SAFE_MODE_SERIAL_PORT.println(F("Starting in safe mode..."));
        delay(2000);
        // normal boot after safe mode
        resetDetector.setSafeModeAndClearCounter(false);
        // activate safe mode
        safe_mode = true;

    }
#if KFC_SHOW_BOOT_MENU_RESET_COUNT
    else {

        if (resetDetector.getResetCounter() > KFC_SHOW_BOOT_MENU_RESET_COUNT) {

            KFC_SAFE_MODE_SERIAL_PORT.println();
            for(uint8_t i = 0; i < 76; i++) {
                KFC_SAFE_MODE_SERIAL_PORT.print('=');
            }
            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("\nCrashs detected: %u\nReset counter: %u\n"), resetDetector.hasCrashDetected(), resetDetector.getResetCounter());
            KFC_SAFE_MODE_SERIAL_PORT.println(F("\nAvailable keys:\n"));
            KFC_SAFE_MODE_SERIAL_PORT.println(F(
                "    t: disable boot menu timeout\n"
                "    s: reboot in safe mode\n"
                "    r: continue to boot normally\n"
                "    f: restore factory settings\n"
                "    c: clear RTC memory\n"
                "    p: enable AP mode and set passwords to default\n"
            ));
            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("Multiple resets detected. Reboot continues in %u seconds...\n"), KFC_BOOT_MENU_TIMEOUT);
            KFC_SAFE_MODE_SERIAL_PORT.println(F("Press reset again to start in safe mode."));
            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("Press reset %ux times to restore factory defaults. A blinking LED indicates success and the normal boot process continues after %u seconds.\n\n"), KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT, RESET_DETECTOR_TIMEOUT / 1000U);

            BlinkLEDTimer::setBlink(__LED_BUILTIN, BlinkLEDTimer::SOS);
            resetDetector.setSafeMode(1);

            static_assert(KFC_BOOT_MENU_TIMEOUT >= 3, "timeout should be at least 3 seconds");
            auto endTimeout = millis() + (KFC_BOOT_MENU_TIMEOUT * 1000UL);

            while(millis() < endTimeout) {
                if (KFC_SAFE_MODE_SERIAL_PORT.available()) {
                    auto ch = KFC_SAFE_MODE_SERIAL_PORT.read();
                    switch(ch) {
                        case 'p':
                            config.read();
                            config.recoveryMode();
                            config.write();
                            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("AP mode with DHCPD enabled (SSID %s)\nUsername '%s', passwords set to '%s'\nWeb server running on port 80\n\nPress r to reboot...\n"),
                                Network::WiFi::getSoftApSSID(),
                                System::Device::getUsername(),
                                SPGM(defaultPassword)
                            );
                            endTimeout = 0;
                            break;
                        case 'c':
                            RTCMemoryManager::clear();
                            SaveCrash::removeCrashCounter();
                            KFC_SAFE_MODE_SERIAL_PORT.println(F("RTC memory cleared"));
                            break;
                        case 't':
                            endTimeout = std::numeric_limits<decltype(endTimeout)>::max();
                            KFC_SAFE_MODE_SERIAL_PORT.println(F("Boot menu timeout disabled"));
                            break;
                        case 'f':
                            config.restoreFactorySettings();
                            config.write();
                            KFC_SAFE_MODE_SERIAL_PORT.println(F("Factory settings restored"));
                            // fallthrough
                        case 'r':
                            SaveCrash::removeCrashCounter();
                            // fallthrough
                        case 's':
                            resetDetector.setSafeModeAndClearCounter(false);
                            safe_mode = (ch == 's');
                            endTimeout = 0;
                            break;
                    }
                }
            }
            if (endTimeout) {
                // timeout occured, count as crash
                // safe_mode should be false
                increaseCrashCounter = true;
            }
            delay(100);
#if defined(ESP8266)
            ESP.wdtFeed();
#endif
        }
    }
#endif

    // start FS, we need it for getCrashCounter()
    KFCFS.begin();

#if KFC_AUTO_SAFE_MODE_CRASH_COUNT
    if (resetDetector.hasCrashDetected() || increaseCrashCounter) {
        uint8_t counter = SaveCrash::getCrashCounter();
        if (counter >= KFC_AUTO_SAFE_MODE_CRASH_COUNT) {  // boot in safe mode if there were 3 (KFC_AUTO_SAFE_MODE_CRASH_COUNT) crashes within the 5 minutes (KFC_CRASH_RECOVERY_TIME)
            resetDetector.setSafeModeAndClearCounter(false);
            safe_mode = true;
        }
    }
#endif

    config.setSafeMode(safe_mode);

#if WEBUI_ALERTS_ENABLED
    // read only mode in safe mode
    // and do not remove non persistent alerts if a crash has been detected
    WebAlerts::Alert::readStorage(safe_mode ?
        WebAlerts::RewriteType::READ_ONLY :
        (resetDetector.hasCrashDetected() ?
            WebAlerts::RewriteType::KEEP_NON_PERSISTENT :
            WebAlerts::RewriteType::REMOVE_NON_PERSISTENT)
    );
#endif

    config.read();
    if (safe_mode) {

        WebAlerts::Alert::warning(F("Running in Safe Mode"), WebAlerts::ExpiresType::REBOOT);
        serialHandler.replaceFirst(&KFC_SAFE_MODE_SERIAL_PORT);

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        prepare_plugins();
        setup_plugins(PluginComponent::SetupModeType::SAFE_MODE);

        // check if wifi is up
        _Scheduler.add(Event::seconds(1), true, [](Event::CallbackTimerPtr timer) {
            timer->updateInterval(Event::seconds(60));
            if (!WiFi.isConnected()) {
                _debug_println(F("WiFi not connected, restarting"));
                config.reconfigureWiFi();
            }
        });

        auto rebootDelay = System::Device::getConfig().getSafeModeRebootTimeout();
        if (rebootDelay) {
            __LDBG_printf("rebooting in %u minutes", rebootDelay);
            // restart device if running in safe mode for rebootDelay minutes
            _Scheduler.add(Event::minutes(rebootDelay), false, [](Event::CallbackTimerPtr timer) {
                Logger_notice(F("Rebooting device after safe mode timeout"));
                config.restartDevice();
            });
        }

        auto flags = System::Flags::getConfig();
        if ((flags.is_softap_enabled || flags.is_softap_standby_mode_enabled) && !strcmp_P(Network::WiFi::getSoftApPassword(), SPGM(defaultPassword))) {
            Logger_warning(F("SoftAP is using default password and will be disabled in 15 minutes..."));
            _Scheduler.add(Event::minutes(15), false, [](Event::CallbackTimerPtr timer) {
                if (WiFi.getMode() & WIFI_AP) {
                    WiFi.enableAP(false);
                }
            });
        }

    } else {

        #if AT_MODE_SUPPORTED
            BOOTLOG_PRINTF("at_mode_setup");
            at_mode_setup();
        #endif

        if (resetDetector.hasCrashDetected()) {
            WebAlerts::Alert::error(PrintString(F("System crash detected.<br>%s"), resetDetector.getResetInfo().c_str()));
        }

#if DEBUG && HAVE_KFC_BOOT_CHECK_FLASHSIZE
#if ENABLE_DEEP_SLEEP
        if (!resetDetector.hasWakeUpDetected())
#endif
        {
            check_flash_size();
            Serial.printf_P(PSTR("Free Sketch Space %u\n"), ESP.getFreeSketchSpace());
#if defined(ESP8266)
            Serial.printf_P(PSTR("CPU frequency %d\n"), system_get_cpu_freq());
#endif
        }
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
        BOOTLOG_PRINTF("adding wifi check");
        _Scheduler.add(Event::seconds(60), true, [](Event::CallbackTimerPtr timer) {
            if (System::Flags::getConfig().is_station_mode_enabled) {
                if (!WiFi.isConnected()) {
                    if (timer->updateInterval(Event::seconds(60)) == false) {
                        timer->setInterval(Event::seconds(30)); // change interval to 30 seconds if it is 60
                    }
                    else {
                        // restart entire wifi subsystem. the interval was reset back to 60 seconds
                        config.reconfigureWiFi();
                        Logger_notice(F("WiFi subsystem restarted"));
                    }
                }
            }
        });

        // reset crash counter
        BOOTLOG_PRINTF("installing safecrash crash recovery");
        SaveCrash::installRemoveCrashCounter(KFC_CRASH_RECOVERY_TIME);
    }

#if LOAD_STATISTICS
    load_avg_timer = millis() + 30000;
#endif

    _debug_println(F("end"));
    BOOTLOG_PRINTF("setup done");
}

#if LOAD_STATISTICS
unsigned long load_avg_timer = 0;
uint32_t load_avg_counter = 0;
float load_avg[3] = {0, 0, 0};
#endif

#undef HIGH

void loop()
{
    auto &loopFunctions = LoopFunctions::getVector();
    bool cleanup = false;
    for(uint8_t i = 0; i < loopFunctions.size(); i++) { // do not use iterators since the vector can be modifed inside the callback
        if (loopFunctions[i].deleteCallback) {
            cleanup = true;
        } else {
            __Scheduler.run(Event::PriorityType::NORMAL); // check priority above NORMAL after every loop function
            if (loopFunctions[i].callback) {
                loopFunctions[i].callback();
            }
            else {
                loopFunctions[i].callbackPtr();
            }
        }
    }
    if (cleanup) {
        loopFunctions.erase(std::remove(loopFunctions.begin(), loopFunctions.end(), LoopFunctions::Entry::Type::DELETED), loopFunctions.end());
        loopFunctions.shrink_to_fit();
    }
    __Scheduler.run(); // check all events

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
