/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "at_mode.h"

class ATModeCommands
{
public:
    using ATModeCommandCallback = void (*)(AtModeArgs &args);

    struct Item {
        PGM_P command;
        ATModeCommandCallback handler;

        constexpr Item(ATModeCommandCallback handler, PGM_P command, PGM_P help = nullptr, PGM_P args = nullptr, PGM_P queryHelp = nullptr) :
            command(command),
            handler(handler)
        {
            (void)help;
            (void)args;
            (void)queryHelp;
        }
    };

public:
    static bool handle(AtModeArgs &args);

public:
    // ignored commands (REM, I2CT, I2CA, I2CR)
    static void IgnoreCommand(AtModeArgs &args);

    #ifndef DISABLE_TWO_WIRE
        // I2C bus
        static void I2CSCommand(AtModeArgs &args);
        static void I2CTMCommand(AtModeArgs &args);
        static void I2CRQCommand(AtModeArgs &args);
    #endif
    #if HAVE_I2CSCANNER
        static void I2CSCANCommand(AtModeArgs &args);
    #endif

    // reset / power
    static void DeepSleepCommand(AtModeArgs &args);
    static void ResetCommand(AtModeArgs &args);

    // configuration
    static void LoadCommand(AtModeArgs &args);
    static void StoreCommand(AtModeArgs &args);
    static void ImportCommand(AtModeArgs &args);
    static void FactoryCommand(AtModeArgs &args);
    static void FactoryStoreResetCommand(AtModeArgs &args);
    #if defined(HAVE_NVS_FLASH)
        static void NVSCommand(AtModeArgs &args);
    #endif

    // file system
    static void TouchCommand(AtModeArgs &args);
    static void MkdirCommand(AtModeArgs &args);
    static void RemoveCommand(AtModeArgs &args);
    static void RenameCommand(AtModeArgs &args);
    static void ListCommand(AtModeArgs &args);
    static void ListRecursiveCommand(AtModeArgs &args);
    static void CatCommand(AtModeArgs &args);

    // network
    static void WiFiCommand(AtModeArgs &args);

    #if ENABLE_ARDUINO_OTA
        // Arduino OTA
        static void AOTACommand(AtModeArgs &args);
    #endif

    // LED
    static void NEOPXCommand(AtModeArgs &args);
    static void LEDCommand(AtModeArgs &args);

    // IO
    static void PWMCommand(AtModeArgs &args);

    // plugins
    static void PLGCommand(AtModeArgs &args);

    // system
    static void DelayCommand(AtModeArgs &args);
    static void CPUCommand(AtModeArgs &args);
    #if RTC_SUPPORT
        static void RTCCommand(AtModeArgs &args);
    #endif

    #if DEBUG
        // only available in debug mode
        static void DumpCommand(AtModeArgs &args);
        static void DumpTimersCommand(AtModeArgs &args);
        static void DumpFsCommand(AtModeArgs &args);
        // implementation requires DEBUG_CONFIGURATION_GETHANDLE, the macro is not available in this header
        static void DumpHandlesCommand(AtModeArgs &args);
        static void MetricsCommand(AtModeArgs &args);
        static void RtcMemoryCommand(AtModeArgs &args);
        static void AtModeCommand(AtModeArgs &args);
        static void PanicCommand(AtModeArgs &args);
    #endif
};
