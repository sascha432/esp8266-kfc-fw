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
#include <NeoPixelEx.h>
#include "kfc_fw_config.h"
#include "blink_led_timer.h"
#include "deep_sleep.h"
#include "at_mode.h"
#include "serial_handler.h"
#include "serial2udp.h"
#include "reset_detector.h"
#include "save_crash.h"
#include "plugins_menu.h"
#if PRINTF_WRAPPER_ENABLED
#include <printf_wrapper.h>
#endif
#include "../src/plugins/plugins.h"
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


void delayedSetup(bool delayed)
{
    if (delayed) {
        KFCFS.begin();
    }

    // check if wifi is up
    _Scheduler.add(Event::seconds(60), true, [](Event::CallbackTimerPtr timer) {
        if (System::Flags::getConfig().is_station_mode_enabled) {
            if (!WiFi.isConnected()) {
                if (timer->updateInterval(Event::seconds(60)) == false) {
                    timer->setInterval(Event::seconds(30)); // change interval to 30 seconds if it is 60
                }
                else {
                    // restart entire wifi subsystem. the interval was reset back to 60 seconds
                    config.reconfigureWiFi(F("reconfiguring WiFi adapter"));
                }
            }
        }
    });

    #if KFC_DISABLE_CRASHCOUNTER == 0
        // reset crash counter
        SaveCrash::installRemoveCrashCounter(KFC_CRASH_RECOVERY_TIME);
    #endif
}

#if KFC_SAFEMODE_GPIO_COMBO
// read key combo and debounce
bool isSystemKeyComboPressed()
{
    uint8_t count = 0;
    for(uint8_t i = 0; i < 5; i++) {
        __LDBG_printf("readall %08x mask %08x result %08x count=%u masked=%08x", digitalReadAll(), KFC_SAFEMODE_GPIO_MASK, KFC_SAFEMODE_GPIO_RESULT, count, digitalReadAll()&KFC_SAFEMODE_GPIO_MASK);
        if ((digitalReadAll() & KFC_SAFEMODE_GPIO_MASK) == KFC_SAFEMODE_GPIO_RESULT) {
            count++;
        }
        delay(10);
    }
    return (count != 0);
}
#endif

void setup()
{
    #if ENABLE_DEEP_SLEEP
        deepSleepPinState.merge();
    #endif

    // resetDetector.armTimer();
    resetDetector.begin(&KFC_SAFE_MODE_SERIAL_PORT, KFC_SERIAL_RATE); // release uart and call Serial.begin()
    #if KFC_DEBUG_USE_SERIAL1
        Serial1.begin(KFC_DEBUG_USE_SERIAL1);
        static_assert(KFC_DEBUG_USE_SERIAL1 >= 300, "must be set to the baud rate");
    #endif

    #if ENABLE_DEEP_SLEEP
        deepSleepPinState.merge();
        bool wakeup = resetDetector.hasWakeUpDetected();

        // ---------------------------------------------------------------------------------
        // custom code remote control plugin
        // ---------------------------------------------------------------------------------
        #if IOT_REMOTE_CONTROL
            // read the button states once more
            // setting the awake pin high will clear the button hardware buffer
            // this code is executed ~60ms after the reset has been invoked and even fast
            // double clicks can be detected
            pinMode(IOT_REMOTE_CONTROL_AWAKE_PIN, OUTPUT);
            digitalWrite(IOT_REMOTE_CONTROL_AWAKE_PIN, HIGH);
        #endif
        // ---------------------------------------------------------------------------------

        deepSleepPinState.merge();
        if (wakeup) {
            KFCFWConfiguration::wakeUpFromDeepSleep();
        }
        deepSleepPinState.merge();
    #endif

    serialHandler.begin();
    DEBUG_HELPER_INIT();

    // PluginComponents::RegisterEx::getInstance().dumpList(KFC_SAFE_MODE_SERIAL_PORT);

    #if DEBUG
    {
        PrintString prefix;
        __debug_prefix(prefix);
        KFCFWConfiguration::printDiag(DEBUG_OUTPUT, prefix);
    }
    #endif

    #if defined(HAVE_GDBSTUB) && HAVE_GDBSTUB
        if (resetDetector.getResetCounter()) {
            resetDetector.clearCounter();
        }
    #endif

    #if 0
        #include "../include/retracted/custom_wifi.h"
        Serial2Udp::initWiFi(F(CUSTOM_WIFI_SSID), F(CUSTOM_WIFI_PASSWORD), IPAddress(192, 168, 0, 3), 6577);
    #endif

    #if HAVE_PCF8574
        initialize_pcf8574();
    #endif
    #if HAVE_PCF8575
        initialize_pcf8575();
    #endif
    #if HAVE_TINYPWM
        initialize_tinypwm();
    #endif
    #if HAVE_PCA9685
        initialize_pca9785();
    #endif
    #if HAVE_MCP23017
        initialize_mcp23017();
    #endif

    bool safe_mode = false;
    #if KFC_DISABLE_CRASHCOUNTER == 0
        bool increaseCrashCounter = false;
    #endif
    #if !ENABLE_DEEP_SLEEP
        bool wakeup = resetDetector.hasWakeUpDetected();
    #endif

    if (!wakeup) {

        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);

        if (resetDetector.getResetCounter() >= 20/* && !resetDetector.hasWakeUpDetected()*/) {
            KFC_SAFE_MODE_SERIAL_PORT.println(F("Reboot continues in 5 seconds..."));
            // stop timer to avoid resetting counters
            resetDetector.disarmTimer();
            // delay boot if too many resets are detected
            delay(5000);
            resetDetector.armTimer();
        }

    #if DEBUG_PRE_INIT_SERIAL2TCP
        #include "../include/retracted/custom_wifi.h"
        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FAST);
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
            BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
        }
        else {
            BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
        }
    #endif

        KFC_SAFE_MODE_SERIAL_PORT.println(F("Booting KFC firmware..."));
        KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("SAFE MODE %d, reset counter %d, wake up %d\n"), resetDetector.getSafeMode(), resetDetector.getResetCounter(), resetDetector.hasWakeUpDetected());

#if KFC_SAFEMODE_GPIO_COMBO
    // boot menu
    //
    // >1000ms start in safe mode (SOS signal)
    // >12000ms reset factory settings and start in safe mode (flickering LED)
    // >20000ms abort boot menu

    if (isSystemKeyComboPressed()) {
        auto start = millis();
        uint8_t mode = 0;
        BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::MEDIUM);
        while(isSystemKeyComboPressed()) {
            auto duration = millis() - start;
            if (duration > 20000) {
                BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                mode = 0;
                break;
            }
            if (mode == 1 && duration > 12000) {
                BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                mode++;
            }
            else if (mode == 0 && duration > 1000) {
                BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
                mode++;
            }
        }
        if (mode == 2) {
            KFC_SAFE_MODE_SERIAL_PORT.println(F("Restoring factory defaults..."));
            config.restoreFactorySettings();
            config.write();
        }
        if (mode >= 1) {
            safe_mode = true;
            config.setSafeMode(safe_mode);
        }
    }
    else {
#endif

#if KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT
        if (resetDetector.hasResetDetected()) {
            if (resetDetector.getResetCounter() >= KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT) {
                KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("%ux reset detected. Restoring factory defaults in a 5 seconds...\n"), KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT);
#if __LED_BUILTIN != IGNORE_BUILTIN_LED_PIN_ID
                for(uint8_t i = 0; i < (RESET_DETECTOR_TIMEOUT + 500) / (100 + 250); i++) {
                    BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SOLID);
                    delay(100);
                    BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                    delay(250);
                }
#else
                delay(5000);
#endif
                config.restoreFactorySettings();
                config.write();
                safe_mode = true;
                resetDetector.setSafeMode(false);
                resetDetector.clearCounter();
            }
        }
#endif

#if KFC_SAFEMODE_GPIO_COMBO
    }
#endif

        if (resetDetector.getSafeMode()) {

            KFC_SAFE_MODE_SERIAL_PORT.println(F("Starting in safe mode..."));
            delay(KFC_SAFEMODE_BOOT_DELAY);
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

                BUILDIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
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
                                #if KFC_DISABLE_CRASHCOUNTER == 0
                                    SaveCrash::removeCrashCounter();
                                #endif
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
                                #if KFC_DISABLE_CRASHCOUNTER == 0
                                    SaveCrash::removeCrashCounter();
                                #endif
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
                    #if KFC_DISABLE_CRASHCOUNTER == 0
                        increaseCrashCounter = true;
                    #endif
                }
                delay(100);
#if defined(ESP8266)
                ESP.wdtFeed();
#endif
            }
        }
#endif

        // __DBG_printf("FS begin");
        // start FS, we need it for getCrashCounter()
        // KFCFS.setConfig(LittleFSConfig(true));
        KFCFS.begin();

#if KFC_AUTO_SAFE_MODE_CRASH_COUNT != 0 && KFC_DISABLE_CRASHCOUNTER == 0
        if (resetDetector.hasCrashDetected() || increaseCrashCounter) {
            uint8_t counter = SaveCrash::getCrashCounter();
            if (counter >= KFC_AUTO_SAFE_MODE_CRASH_COUNT) {  // boot in safe mode if there were 3 (KFC_AUTO_SAFE_MODE_CRASH_COUNT) crashes within the 5 minutes (KFC_CRASH_RECOVERY_TIME)
                resetDetector.setSafeModeAndClearCounter(false);
                safe_mode = true;
                RTCMemoryManager::clear();
            }
        }
#endif

        config.setSafeMode(safe_mode);

    }

    // __DBG_printf("read config");
    config.read();
    // __DBG_printf("starting safe_mode=%u", safe_mode);
    if (safe_mode) {

        serialHandler.replaceFirst(&KFC_SAFE_MODE_SERIAL_PORT);

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        auto &componentRegister = PluginComponents::RegisterEx::getInstance();
        componentRegister.setup(PluginComponent::SetupModeType::SAFE_MODE);

        // check if wifi is up
        _Scheduler.add(Event::seconds(10), true, [](Event::CallbackTimerPtr timer) {
            timer->updateInterval(Event::seconds(60));
            if (!WiFi.isConnected()) {
                config.reconfigureWiFi(F("WiFi not connected, reconfiguring WiFi adapter"));
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

    }
    else {

        // setup internal or external RTC
        KFCFWConfiguration::setupRTC();

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        auto &componentRegister = PluginComponents::RegisterEx::getInstance();

        componentRegister.setup(
#if ENABLE_DEEP_SLEEP
            wakeup ?
                PluginComponent::SetupModeType::AUTO_WAKE_UP :
#endif
                PluginComponent::SetupModeType::DEFAULT
        );

        if (wakeup) {
            _Scheduler.add(250, false, [](Event::CallbackTimerPtr) {
                delayedSetup(true);
            });
        }
        else {
            delayedSetup(false);
        }

#if DEBUG_DEEP_SLEEP
        __DBG_printf("wakeup=%u mode=%u", wakeup, deepSleepParams.getWakeupMode());
        if (deepSleepParams.getWakeupMode() == DeepSleep::WakeupMode::AUTO) {
            Logger_notice(F("Wakeup from deep sleep start-time=" TIME_T_FMT " sleep-time=%.3f rtc-offset=%.6f"), time(nullptr), deepSleepParams.getTotalTime(), DeepSleep::_realTimeOffset / 1000000.0);

        }
#endif
    }

#if DEBUG_ASSETS
    __DBG_printf("DEBUG_ASSETS=1: " DEBUG_ASSETS_URL1 " " DEBUG_ASSETS_URL2);
#endif

}

#undef HIGH

void loop()
{
static bool toggle;
toggle = !toggle;

    auto &loopFunctions = LoopFunctions::getVector();
    bool cleanUp = false;
    for(uint8_t i = 0; i < loopFunctions.size(); i++) { // do not use iterators since the vector can be modifed inside the callback
        if (loopFunctions[i].deleteCallback) {
            cleanUp = true;
        }
        else {
            __Scheduler.run(Event::PriorityType::NORMAL); // check priority above NORMAL after every loop function
            loopFunctions[i].callback();
        }
    }
    if (cleanUp) {
        loopFunctions.erase(std::remove_if(loopFunctions.begin(), loopFunctions.end(), [](const LoopFunctions::Entry &entry) { return entry.deleteCallback == true; }), loopFunctions.end());
        loopFunctions.shrink_to_fit();
    }
    __Scheduler.run(); // check all events

#if ESP32
    run_scheduled_functions();
#endif
}


