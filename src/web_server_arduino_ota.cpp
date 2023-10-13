/**
 * Author: sascha_lammers@gmx.de
 */

#include <Arduino_compat.h>
#include <ArduinoOTA.h>
#include <LoopFunctions.h>
#include "kfc_fw_config.h"
#include "web_server.h"

#if DEBUG_WEB_SERVER_ACTION
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;

using namespace WebServer;

#if ENABLE_ARDUINO_OTA

    inline static void ArduinoOTALoop()
    {
        ArduinoOTA.handle();
    }

    void Plugin::ArduinoOTAdestroy()
    {
        LoopFunctions::remove(ArduinoOTALoop);
    }

    const __FlashStringHelper *Plugin::ArduinoOTAErrorStr(ota_error_t err)
    {
        if (err == static_cast<ota_error_t>(ArduinoOTAInfo::kNoError)) {
            return F("NONE");
        }
        switch (err) {
        case OTA_AUTH_ERROR:
            return F("AUTH ERROR");
        case OTA_BEGIN_ERROR:
            return F("BEGIN ERROR");
        case OTA_CONNECT_ERROR:
            return F("CONNECT ERROR");
        case OTA_RECEIVE_ERROR:
            return F("RECEIVE ERROR");
        case OTA_END_ERROR:
            return F("END ERROR");
        default:
            break;
        }
        return F("UNKNOWN");
    }

    void Plugin::ArduinoOTAbegin()
    {
        if (_AOTAInfo._runnning) {
            return;
        }
        ArduinoOTA.setHostname(System::Device::getName());
        ArduinoOTA.setPassword(System::Device::getPassword());
        // ArduinoOTA.setPort(8266); // default
        ArduinoOTA.onStart([this]() {
            Logger_notice(F("Firmware upload started"));
            BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
            __LDBG_printf("ArduinoOTA start");

            #ifndef ATOMIC_FS_UPDATE
                if (ArduinoOTA.getCommand() == U_FS) {
                    KFCFS.end(); // end file system to avoid log files or other data being written during the update, which leads to FS corruption
                }
            #endif

            _AOTAInfo.start();
        });
        ArduinoOTA.onEnd([this]() {
            __LDBG_printf("ArduinoOTA end");
            if (_AOTAInfo) {
                _AOTAInfo.stop();
                Logger_security(F("Firmware upgrade successful, rebooting device"));
                BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::FLICKER);
                _Scheduler.add(2000, false, [](Event::CallbackTimerPtr timer) {
                    config.restartDevice();
                });
            }
            else {
                Logger_error(F("Firmware upgrade failed: %s"), ArduinoOTAErrorStr(_AOTAInfo._error));
            }
        });
        ArduinoOTA.onProgress([this](int progress, int size) {
            __LDBG_printf("ArduinoOTA progress %d / %d", progress, size);
            if (_AOTAInfo) {
                _AOTAInfo.update(progress, size);
                if (_updateFirmwareCallback) {
                    _updateFirmwareCallback(progress, size);
                }
            }
        });
        ArduinoOTA.onError([this](ota_error_t err) {
            if (_AOTAInfo) {
                _AOTAInfo.stop(err);
                if (!_AOTAInfo) {
                    BUILTIN_LED_SET(BlinkLEDTimer::BlinkType::SOS);
                    Logger_error(F("Firmware upgrade failed: %s"), ArduinoOTAErrorStr(_AOTAInfo._error));
                }
            }
        });
        ArduinoOTA.setRebootOnSuccess(false);
        #if ESP32
            ArduinoOTA.begin();
            ArduinoOTA.setMdnsEnabled(System::Flags::getConfig().is_mdns_enabled);
        #else
            ArduinoOTA.begin(System::Flags::getConfig().is_mdns_enabled);
        #endif
        LOOP_FUNCTION_ADD(ArduinoOTALoop);
        _AOTAInfo._runnning = true;

        __LDBG_printf("Arduino OTA running on %s", ArduinoOTA.getHostname().c_str());
    }

    void Plugin::ArduinoOTAend()
    {
        if (!_AOTAInfo._runnning) {
            return;
        }
        __LDBG_printf("Arduino OTA stopped");
        LoopFunctions::remove(ArduinoOTALoop);
        _AOTAInfo = ArduinoOTAInfo();
        // release memory
        ArduinoOTA.~ArduinoOTAClass();
        ArduinoOTA = ArduinoOTAClass();
    }

    void Plugin::ArduinoOTADumpInfo(Print &output)
    {
        output.printf_P(PSTR("running=%u in_progress=%u reboot_pending=%u progress=%u/%u error=%d (%s)\n"),
            _AOTAInfo._runnning, _AOTAInfo._inProgress, _AOTAInfo._rebootPending,
            _AOTAInfo._progress, _AOTAInfo._size,
            _AOTAInfo._error, ArduinoOTAErrorStr(_AOTAInfo._error)
            // ArduinoOTA._state
        );
    }

#else

    void Plugin::ArduinoOTAdestroy()
    {
    }

#endif
