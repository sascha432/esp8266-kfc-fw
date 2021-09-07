/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

//
// provides a reset counter and interface to access the reason of the restart
// the counter can be used to restore settings, boot into safe mode or offer any kind of reset function that
// is based on turning the device off and on quickly

#ifndef __RESET_DETECTOR_INCLUDED
#define __RESET_DETECTOR_INCLUDED
#define __RESET_DETECTOR_INSIDE_INCLUDE

// enable debug outout
#ifndef DEBUG_RESET_DETECTOR
#    define DEBUG_RESET_DETECTOR 0
#endif

// include methods in source code = 0, or in header as always inline = 1
// use 1 for production, speeds everything up and reduces code size
// use 0 for development to avoid recompiling half the program for every change
#ifndef RESET_DETECTOR_INCLUDE_HPP_INLINE
#    if DEBUG_RESET_DETECTOR
#        define RESET_DETECTOR_INCLUDE_HPP_INLINE 1
#    else
#        define RESET_DETECTOR_INCLUDE_HPP_INLINE 1
#    endif
#endif

#include <Arduino_compat.h>
#include <HardwareSerial.h>

#if ESP8266
#    include <osapi.h>
#    include <user_interface.h>
#elif ESP32 || _WIN32 || _WIN64
#else
#    error Platform not supported
#endif

#include <push_pack.h>

#define RESET_DETECTOR_TIMEOUT 5000

#ifndef USE_ESP_GET_RESET_REASON
#    if ESP8266
#        define USE_ESP_GET_RESET_REASON 1
#    else
#        define USE_ESP_GET_RESET_REASON 0
#    endif
#endif

#include <PluginComponent.h>
#include <RTCMemoryManager.h>
#include <debug_helper.h>
#include <plugins.h>

class ResetDetector {
public:
    using Reason_t = uint8_t;
    using Counter_t = uint16_t;

    class Data {
    public:
        static constexpr Counter_t kInvalidCounter = (1U << (sizeof(Counter_t) << 3)) - 1;
        static constexpr Counter_t kMaxCounter = kInvalidCounter - 1;
        static constexpr Reason_t kInvalidReason = 0xff;
        static constexpr size_t kReasonMax = 8;

    public:
        Data(Counter_t reset_counter = kInvalidCounter, bool safe_mode = false);

        operator bool() const;
        operator int() const;
        operator Counter_t() const;
        Data &operator=(Counter_t counter);
        Data &operator++();

        void setValid(bool valid);
        void setSafeMode(bool safe_mode);
        bool isSafeMode() const;
        Counter_t getResetCounter();
        void pushReason(Reason_t reason);
        bool hasValidReason() const;
        Reason_t getReason() const;
        const Reason_t *begin() const;
        const Reason_t *end() const;

    protected:
        Reason_t *_begin();
        Reason_t *_end();
        Reason_t *_current();

    private:
        Counter_t _reset_counter;
        bool _safe_mode;
        Reason_t _reason[kReasonMax];
    };

    static constexpr auto kResetDetectorDataSize = sizeof(Data);

private:
    Data data;

public:

    ResetDetector();
    ~ResetDetector();

public:
    void begin();
    void end();
    void begin(HardwareSerial *serial, int baud);

    // returns the number of resets. starts with 1 after the first reset and increases with
    // each reset that occurs before the device is running longer than RESET_DETECTOR_TIMEOUT
    // once the timeout has occured, it returns 0
    Counter_t getResetCounter() const;
    // returns the same value as getResetCounter() but does not reset to 0 after the timeout
    Counter_t getInitialResetCounter() const;
    // returns the value that was stored in the safe mode field
    bool getSafeMode() const;
    // store value in safe mode field
    void setSafeMode(bool safeMode);
    // set counter to 0
    void clearCounter();
#if DEBUG
    void __setResetCounter(Counter_t counter);
#endif
    void setSafeModeAndClearCounter(bool safeMode);

    bool hasCrashDetected() const;
    bool hasResetDetected() const;
    bool hasRebootDetected() const;
    bool hasWakeUpDetected() const;
    const String getResetInfo() const;

    const __FlashStringHelper *getResetReason() const;
    static const __FlashStringHelper *getResetReason(Reason_t reason);

    ETSTimerEx *getTimer();
    void armTimer();
    void disarmTimer();
    static void _timerCallback(ResetDetector *arg);

private:
    void _readData();
    void _writeData();

private:
    ETSTimerEx _timer;
    Data _storedData;
    Data _data;
    #if ESP8266 && DEBUG_RESET_DETECTOR
        uart_t *_uart;
    #endif
};

class ResetDetectorPlugin : public PluginComponent {
public:
    ResetDetectorPlugin();

    virtual void getStatus(Print &output) override;
    virtual void createMenu() override; // in web_server.cpp


#if AT_MODE_SUPPORTED
    virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
    virtual bool atModeHandler(AtModeArgs &args) override;
#endif
};

class HardwareSerial;

extern ResetDetector &resetDetector;

#if RESET_DETECTOR_INCLUDE_HPP_INLINE
#    include "reset_detector.hpp"
#endif

#include <pop_pack.h>

#undef __RESET_DETECTOR_INSIDE_INCLUDE
#endif
