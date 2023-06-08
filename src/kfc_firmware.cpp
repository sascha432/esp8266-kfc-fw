/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <Event.h>
#include <ListDir.h>
#include <PrintString.h>
#include <StreamWrapper.h>
#include <algorithm>
#include <functional>
#include <vector>
#include "at_mode.h"
#include "blink_led_timer.h"
#include "deep_sleep.h"
#include "kfc_fw_config.h"
#include "plugins_menu.h"
#include "reset_detector.h"
#include "save_crash.h"
#include "serial2udp.h"
#include "serial_handler.h"
#include "web_server.h"
#include "../src/plugins/plugins.h"
#if __LED_BUILTIN_WS2812_NUM_LEDS
#    include <NeoPixelEx.h>
#endif
#if HAVE_IOEXPANDER
#    include <IOExpander.h>
#endif
#if PRINTF_WRAPPER_ENABLED
#    include <printf_wrapper.h>
#endif

#if DEBUG_ALL
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// connect to wifi and serial2tcp before booting to see all debug output
#ifndef DEBUG_PRE_INIT_SERIAL2TCP
#    define DEBUG_PRE_INIT_SERIAL2TCP 0
#endif

#if DEBUG_PRE_INIT_SERIAL2TCP
#    include "../src/plugins/serial2tcp/Serial2TcpBase.h"
#    include "../src/plugins/serial2tcp/Serial2TcpClient.h"
#    include "../src/plugins/serial2tcp/serial2tcp.h"
#    include "PluginComponent.h"
#endif

#if ESP8266

extern "C" void preinit(void)
{
    reset_detector_setup_global_ctors();
}

#endif

using KFCConfigurationClasses::System;
using KFCConfigurationClasses::Network;

void delayedSetup(bool delayed)
{
    if (delayed) {
        KFCFS_begin();
    }

    // check if wifi is up
    constexpr uint32_t kCheckWiFiConnectionIntervalSeconds = 60;
    constexpr uint32_t kWiFiErrorIntervalIncrease = 15;
    uint32_t resetTimer = kWiFiErrorIntervalIncrease;

    _Scheduler.add(Event::seconds(kCheckWiFiConnectionIntervalSeconds), true, [&resetTimer](Event::CallbackTimerPtr timer) {
        if (System::Flags::getConfig().is_station_mode_enabled) {
            if (!WiFi.isConnected()) {
                // register error
                config.registerWiFiError();
                if (timer->updateInterval(Event::seconds(resetTimer))) {
                    // increase reset timer in case the connection will take a lot longer than 10 seconds
                    resetTimer += kWiFiErrorIntervalIncrease;
                    // restart entire wifi subsystem
                    // starts AP if enabled
                    config.reconfigureWiFi(F("reconfiguring WiFi adapter"));
                }
                else {
                    // reconnect station mode only if reconfigure was executed before
                    Logger_notice(F("reconnecting WiFi station mode"));
                    config.connectWiFi(config.kKeepWiFiNetwork, true);
                }
            }
            else {
                resetTimer = kWiFiErrorIntervalIncrease;
                // reset timer if wifi is connected
                timer->updateInterval(Event::seconds(kCheckWiFiConnectionIntervalSeconds));
            }
        }
    });

    #if KFC_DISABLE_CRASH_COUNTER == 0
        // reset crash counter
        SaveCrash::installRemoveCrashCounter(KFC_CRASH_RECOVERY_TIME);
    #endif
}

#if KFC_SAFEMODE_GPIO_COMBO
    inline static uint32_t getPinState()
    {
        #if ENABLE_DEEP_SLEEP
            DeepSleep::deepSleepPinState.merge();
            return DeepSleep::deepSleepPinState.getStates();
        #else
            return GPIO32::read();
        #endif
    }

    // repeat must be >=1
    static bool isSystemKeyComboPressed(uint8_t repeat = 2)
    {
        if ((getPinState() & KFC_SAFEMODE_GPIO_MASK) == KFC_SAFEMODE_GPIO_RESULT) {
            #if ENABLE_DEEP_SLEEP
                DeepSleep::deepSleepPinState.init();
            #endif
            return true;
        }
        while(--repeat) {
            delay(5);
            if ((getPinState() & KFC_SAFEMODE_GPIO_MASK) == KFC_SAFEMODE_GPIO_RESULT) {
                #if ENABLE_DEEP_SLEEP
                    DeepSleep::deepSleepPinState.init();
                #endif
                return true;
            }
        }
        return false;
    }
#endif

void setup()
{
    #if ESP32
        resetDetector.armTimer();
    #endif

    #if ENABLE_DEEP_SLEEP
        DeepSleep::deepSleepPinState.merge();
    #endif

    #if ESP32
        KFC_SAFE_MODE_SERIAL_PORT.end();
    #endif

    resetDetector.begin(&KFC_SAFE_MODE_SERIAL_PORT, KFC_SERIAL_RATE); // release uart and call Serial.begin()
    #if KFC_DEBUG_USE_SERIAL1
        Serial1.begin(KFC_DEBUG_USE_SERIAL1);
        static_assert(KFC_DEBUG_USE_SERIAL1 >= 300, "must be set to the baud rate");
    #endif

    #if BUILTIN_LED_NEOPIXEL
        WS2812LEDTimer::init();
    #endif

    #if ENABLE_DEEP_SLEEP

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

        #if WIFI_QUICK_CONNECT_MANUAL
            if (wakeup && !DeepSleep::enableWiFiOnBoot) {
                KFCFWConfiguration::wakeUpFromDeepSleep();
                DeepSleep::deepSleepPinState.merge();
            }
        #endif

    #endif

    // display loaded plugins
    // PluginComponents::RegisterEx::getInstance().dumpList(KFC_SAFE_MODE_SERIAL_PORT);

    serialHandler.begin();
    DEBUG_HELPER_INIT();

    #if ENABLE_DEEP_SLEEP && DEBUG_DEEP_SLEEP
        __DBG_printf("enable WiFi on boot=%u", DeepSleep::enableWiFiOnBoot);
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

    #if defined(HAVE_IOEXPANDER)
        __LDBG_printf("IOExpander::config.begin() size=%u count=%u", sizeof(IOExpander::config), IOExpander::config.size());
        IOExpander::config.begin(KFCFWConfiguration::initTwoWire());
    #endif

    #if ENABLE_DEEP_SLEEP
        DeepSleep::deepSleepPinState.merge();
    #endif

    bool safe_mode = false;
    #if KFC_DISABLE_CRASH_COUNTER == 0
        bool increaseCrashCounter = false;
    #endif
    #if !ENABLE_DEEP_SLEEP
        bool wakeup = resetDetector.hasWakeUpDetected();
    #endif

    if (!wakeup) {

        if (resetDetector.getResetCounter() >= 10) {
            uint32_t delayTime = (resetDetector.getResetCounter() > 30) ? 120 : (resetDetector.getResetCounter() > 20) ? 30 : 5;
            KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("Reboot continues in %u seconds...\n"), delayTime);
            // stop timer to avoid resetting counters
            resetDetector.disarmTimer();
            // delay boot if too many resets are detected
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SLOW);
            #if KFC_SAFEMODE_GPIO_COMBO
                auto start = millis();
                delayTime *= 1000;
                while(millis() - start < delayTime) {
                    if (isSystemKeyComboPressed(10)) {
                        break;
                    }
                }
            #else
                delay(delayTime * 1000U);
            #endif
            resetDetector.armTimer();
        }

        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);

        #if DEBUG_PRE_INIT_SERIAL2TCP
            #include "../include/retracted/custom_wifi.h"
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FAST);
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
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
            }
            else {
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
            }
        #endif

        KFC_SAFE_MODE_SERIAL_PORT.println(F("Booting KFC firmware..."));
        // KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("SAFE MODE %d, reset counter %d, wake up %d\n"), resetDetector.getSafeMode(), resetDetector.getResetCounter(), resetDetector.hasWakeUpDetected());

        #if KFC_SAFEMODE_GPIO_COMBO
            // boot menu
            //
            // >1000ms start in safe mode (SOS signal)
            // >12000ms reset factory settings and start in safe mode (flickering LED)
            // >20000ms abort boot menu

            if (isSystemKeyComboPressed()) {

                auto start = millis();
                uint8_t mode = 0;
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::MEDIUM);
                while(isSystemKeyComboPressed(10)) {

                    uint32_t duration = millis() - start;

                    #if DEBUG
                        delay(400);
                        KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("System Key Combo Pressed %ums\n"), duration);
                    #endif

                    if (duration > 20000) {
                        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::OFF);
                        mode = 0;
                        break;
                    }
                    if (mode == 1 && duration > 12000) {
                        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                        mode++;
                    }
                    else if (mode == 0 && duration > 1000) {
                        BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
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

        #elif KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT

            if (resetDetector.hasResetDetected()) {
                if (resetDetector.getResetCounter() >= KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT) {
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                    KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("%ux reset detected. Restoring factory defaults in 5 seconds...\n"), KFC_RESTORE_FACTORY_SETTINGS_RESET_COUNT);
                    delay(5000);
                    config.restoreFactorySettings();
                    config.write();
                    safe_mode = true;
                    resetDetector.setSafeMode(false);
                    resetDetector.clearCounter();
                }
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
                    KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("\nCrash detected: %u\nReset counter: %u\n"), resetDetector.hasCrashDetected(), resetDetector.getResetCounter());
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

                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
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
                                    #if KFC_DISABLE_CRASH_COUNTER == 0
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
                                    #if KFC_DISABLE_CRASH_COUNTER == 0
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
                        // timeout occurred, count as crash
                        // safe_mode should be false
                        #if KFC_DISABLE_CRASH_COUNTER == 0
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

        KFCFS_begin();

        #if 0
        // dump file system and wait 5 seconds
        {
            KFC_SAFE_MODE_SERIAL_PORT.println(F("File system contents:"));
            auto dir = ListDir(F("/"), false, true);
            while(dir.next()) {
                if (dir.isFile()) {
                    KFC_SAFE_MODE_SERIAL_PORT.printf_P(PSTR("%8.8s "), formatBytes(dir.fileSize()).c_str());
                }
                else {
                    KFC_SAFE_MODE_SERIAL_PORT.print(F("[...]    "));
                }
                KFC_SAFE_MODE_SERIAL_PORT.println(dir.fileName());
            }
            for(uint8_t i = 0; i < 50; i++) {
                delay(100);
                KFC_SAFE_MODE_SERIAL_PORT.print('.');
            }
            KFC_SAFE_MODE_SERIAL_PORT.println();
        }
        #endif

        #if KFC_AUTO_SAFE_MODE_CRASH_COUNT != 0 && KFC_DISABLE_CRASH_COUNTER == 0
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

    config.read();
    SaveCrash::Data::setMD5(KFCConfigurationClasses::System::Firmware::getFirmwareMD5());

    #if ENABLE_DEEP_SLEEP
        DeepSleep::deepSleepPinState.merge();
    #endif

    auto &componentRegister = PluginComponents::RegisterEx::getInstance();
    componentRegister.sort();

    // --------------------------------------------------------------------
    // safe mode
    // --------------------------------------------------------------------
    if (safe_mode) {

        serialHandler.replaceFirst(&KFC_SAFE_MODE_SERIAL_PORT);

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        componentRegister.setup(PluginComponent::SetupModeType::SAFE_MODE);

        // check if wifi is up
        _Scheduler.add(Event::seconds(10), true, [](Event::CallbackTimerPtr timer) {
            timer->updateInterval(Event::seconds(60));
            if (!WiFi.isConnected()) {
                config.reconfigureWiFi(F("WiFi not connected, reconfiguring WiFi adapter"));
            }
        });

        // auto restart if safe mode has been enabled
        auto rebootDelay = System::Device::getConfig().getSafeModeRebootTimeout();
        if (rebootDelay) {
            __LDBG_printf("rebooting in %u minutes", rebootDelay);
            // restart device if running in safe mode for rebootDelay minutes
            _Scheduler.add(Event::minutes(rebootDelay), false, [](Event::CallbackTimerPtr timer) {
                Logger_notice(F("Rebooting device after safe mode timeout"));
                config.restartDevice();
            });
        }

        // handle softAP behavior and shutdown in safe mode
        auto flags = System::Flags::getConfig();
        if ((flags.is_softap_enabled || flags.is_softap_standby_mode_enabled) && !strcmp_P(Network::WiFi::getSoftApPassword(), SPGM(defaultPassword))) {
            Logger_warning(F("SoftAP is using default password and will be disabled in " _STRINGIFY(SOFTAP_SHUTDOWN_TIME) " minutes..."));
            _Scheduler.add(Event::minutes(SOFTAP_SHUTDOWN_TIME), false, [](Event::CallbackTimerPtr timer) {
                if (WiFi.getMode() & WIFI_AP) {
                    WiFi.enableAP(false);
                }
            });
        }

        #if ENABLE_ARDUINO_OTA
            // start ArduinoOTA in safe mode
            WebServer::Plugin::getInstance().ArduinoOTAbegin();
        #endif

    }
    // --------------------------------------------------------------------
    // default mode
    // --------------------------------------------------------------------
    else {

        // setup internal or external RTC
        KFCFWConfiguration::setupRTC();

        #if AT_MODE_SUPPORTED
            at_mode_setup();
        #endif

        #if IOT_CLOCK || IOT_LED_MATRIX || IOT_LED_STRIP
            // disable the LEDs tif it was caused by a power surge when turning on
            if (resetDetector.hasCrashDetected() || resetDetector.hasBrownoutDetected()) {
                ClockPlugin::getInstance().lock();
           }
        #endif

        #if ENABLE_DEEP_SLEEP
            componentRegister.setup(wakeup ? PluginComponent::SetupModeType::AUTO_WAKE_UP : PluginComponent::SetupModeType::DEFAULT);
        #else
            componentRegister.setup(PluginComponent::SetupModeType::DEFAULT);
        #endif

        if (wakeup) {
            // delay file system initialization and other functionality to improve deep sleep wake up performance
            // TODO accessing the FS might cause a crash
            _Scheduler.add(500, false, [](Event::CallbackTimerPtr) {
                delayedSetup(true);
            });
        }
        else {
            delayedSetup(false);
        }

        #if DEBUG_DEEP_SLEEP
            __DBG_printf("wakeup=%u mode=%u", wakeup, DeepSleep::deepSleepParams.getWakeupMode());
            if (DeepSleep::deepSleepParams.getWakeupMode() == DeepSleep::WakeupMode::AUTO) {
                Logger_notice(F("Wakeup from deep sleep start-time=" TIME_T_FMT " sleep-time=%.3f rtc-offset=%.6f"), time(nullptr), DeepSleep::deepSleepParams.getTotalTime(), realTimeOffset / 1000000.0);
            }
        #endif
    }

    #if DEBUG_ASSETS
        __DBG_printf("DEBUG_ASSETS=1: " DEBUG_ASSETS_URL1 " " DEBUG_ASSETS_URL2);
    #endif

}

void loop()
{
    auto &loopFunctions = LoopFunctions::getVector();
    bool cleanUp = false;
    for(uint8_t i = 0; i < loopFunctions.size(); i++) { // do not use iterators since the vector can be modifed inside the callback
        if (loopFunctions[i].deleteCallback) {
            cleanUp = true;
        }
        else {
            __Scheduler.run(Event::PriorityType::NORMAL); // check priority above NORMAL after every loop function
            #if DEBUG_LOOP_FUNCTIONS
                uint32_t start = millis();
                loopFunctions[i].callback();
                uint32_t dur = millis() - start;
                if (dur > DEBUG_LOOP_FUNCTIONS_MAX_TIME) {
                    __DBG_printf("loop function time=%u source=%s line=%u", dur, loopFunctions[i]._source.c_str(), loopFunctions[i]._line);
                }
            #else
                loopFunctions[i].callback();
            #endif
            #if ESP32 && defined(CONFIG_HEAP_POISONING_COMPREHENSIVE)
                heap_caps_check_integrity_all(true);
            #endif
        }
    }
    if (cleanUp) {
        InterruptLock lock;
        loopFunctions.erase(std::remove_if(loopFunctions.begin(), loopFunctions.end(), [](const LoopFunctions::Entry &entry) {
                return entry.deleteCallback == true;
            }
        ), loopFunctions.end());
        loopFunctions.shrink_to_fit();
    }
    __Scheduler.run(); // check all events

    #if _MSC_VER || ESP32
        run_scheduled_functions();
    #endif
}
