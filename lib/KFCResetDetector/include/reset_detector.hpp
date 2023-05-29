/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef __RESET_DETECTOR_INSIDE_INCLUDE
#    define __RESET_DETECTOR_INLINE__
#    define __RESET_DETECTOR_INLINE_ALWAYS__
#    define __RESET_DETECTOR_INLINE_INLINE_DEFINED__
#else
#    define __RESET_DETECTOR_INLINE__        inline
#    define __RESET_DETECTOR_INLINE_ALWAYS__ inline __attribute__((__always_inline__))
#    define __RESET_DETECTOR_INLINE_INLINE_DEFINED__
#    include "reset_detector.h"
#endif

#if DEBUG_RESET_DETECTOR
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

// ------------------------------------------------------------------------
//  ResetDetector
// ------------------------------------------------------------------------

__RESET_DETECTOR_INLINE__
ResetDetector::ResetDetector() :
    _timer({})
    #if ESP8266 && DEBUG_RESET_DETECTOR
        , _uart(nullptr)
    #endif
{
}

__RESET_DETECTOR_INLINE__
ResetDetector::~ResetDetector()
{
    end();
}

__RESET_DETECTOR_INLINE_ALWAYS__
const __FlashStringHelper *ResetDetector::getResetReason() const
{
    return ResetDetector::getResetReason(_data.getReason());
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Counter_t ResetDetector::getResetCounter() const
{
    return _data;
}

__RESET_DETECTOR_INLINE__
ResetDetector::Counter_t ResetDetector::getInitialResetCounter() const
{
    return _storedData;
}

__RESET_DETECTOR_INLINE_ALWAYS__
bool ResetDetector::getSafeMode() const
{
    return _data.isSafeMode();
}

__RESET_DETECTOR_INLINE__
void ResetDetector::setSafeMode(bool safeMode)
{
    _data.setSafeMode(safeMode);
    _data.setValid(true);
    _writeData();
}

__RESET_DETECTOR_INLINE__
bool ResetDetector::hasCrashDetected() const
{
    #if ESP32
        switch(_data.getReason()) {
            case ESP_RST_UNKNOWN:
            case ESP_RST_PANIC:
            case ESP_RST_INT_WDT:
            case ESP_RST_TASK_WDT:
            case ESP_RST_WDT:
                return true;
            default:
                return false;
        }
    #elif ESP8266

        // REASON_WDT_RST          = 1,    /* hardware watch dog reset */
        // REASON_EXCEPTION_RST    = 2,    /* exception reset, GPIO status won’t change */
        // REASON_SOFT_WDT_RST     = 3,    /* software watch dog reset, GPIO status won’t change */

        return (
            _data.getReason() != REASON_DEFAULT_RST &&
            _data.getReason() != REASON_EXT_SYS_RST &&
            _data.getReason() != REASON_SOFT_RESTART &&
            _data.getReason() != REASON_DEEP_SLEEP_AWAKE
        );
    #endif
}

__RESET_DETECTOR_INLINE__
bool ResetDetector::hasBrownoutDetected() const
{
    #if ESP32
       return (_data.getReason() == ESP_RST_BROWNOUT);
    #else
        return false;
    #endif
}

__RESET_DETECTOR_INLINE__
bool ResetDetector::hasResetDetected() const
{
    switch(_data.getReason()) {
    #if ESP32
        case ESP_RST_UNKNOWN:
        case ESP_RST_EXT:
        case ESP_RST_BROWNOUT:
        case ESP_RST_SDIO:
            return true;
    #elif ESP8266
        case REASON_DEFAULT_RST:
        case REASON_EXT_SYS_RST:
            return true;
    #endif
        default:
            break;
    }
    return false;
}

__RESET_DETECTOR_INLINE__
bool ResetDetector::hasRebootDetected() const
{
    switch(_data.getReason()) {
    #if ESP32
        case ESP_RST_SW:
            return true;
    #elif ESP8266
        case REASON_SOFT_RESTART:
            return true;
    #endif
        default:
            break;
    }
    return false;
}

__RESET_DETECTOR_INLINE__
bool ResetDetector::hasWakeUpDetected() const
{
    switch(_data.getReason()) {
    #if ESP32
        case ESP_RST_DEEPSLEEP:
            return true;
    #elif ESP8266
        case REASON_DEEP_SLEEP_AWAKE:
            return true;
    #endif
        default:
            break;
    }
    return false;
}

__RESET_DETECTOR_INLINE_ALWAYS__
void ResetDetector::disarmTimer()
{
    _timer.done();
}

__RESET_DETECTOR_INLINE__
void ResetDetector::_timerCallback(ResetDetector *rd)
{
    rd->clearCounter();
    rd->disarmTimer();
}

__RESET_DETECTOR_INLINE__
ETSTimerEx *ResetDetector::getTimer()
{
    return &_timer;
}

__RESET_DETECTOR_INLINE__
void ResetDetector::armTimer()
{
    _timer.create(reinterpret_cast<ETSTimerEx::ETSTimerExCallback>(_timerCallback), this);
    _timer.arm(RESET_DETECTOR_TIMEOUT, false, true);
}

__RESET_DETECTOR_INLINE__
const String ResetDetector::getResetInfo() const
{
    #if ESP32
        return getResetReason();
    #else
        return ESP.getResetInfo();
    #endif
}

__RESET_DETECTOR_INLINE__
void ResetDetector::clearCounter()
{
    __LDBG_printf("set counter=0 (%u) safe_mode=%u", _data.getResetCounter(), _data.isSafeMode());
    _data = 0;
    _writeData();
}

__RESET_DETECTOR_INLINE__
void ResetDetector::setSafeModeAndClearCounter(bool safeMode)
{
    __LDBG_printf("set counter=0 (%u) set safe_mode=%u", _data.getResetCounter(), data.isSafeMode());
    _data = Data(0, safeMode);
    _writeData();
}

__RESET_DETECTOR_INLINE__
void ResetDetector::_writeData()
{
    if (!_data) {
        RTCMemoryManager::remove(PluginComponent::RTCMemoryId::RESET_DETECTOR);
    }
    else {
        RTCMemoryManager::write(PluginComponent::RTCMemoryId::RESET_DETECTOR, &_data, sizeof(_data));
    }
}

// ------------------------------------------------------------------------
//  ResetDetector::Data
// ------------------------------------------------------------------------

__RESET_DETECTOR_INLINE__
ResetDetector::Data::Data(Counter_t reset_counter, bool safe_mode) :
    _reset_counter(reset_counter),
    _safe_mode(safe_mode),
    _reason{kInvalidReason, kInvalidReason, kInvalidReason, kInvalidReason, kInvalidReason, kInvalidReason, kInvalidReason, kInvalidReason}
{
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Data::operator bool() const
{
    return _reset_counter != kInvalidCounter;
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Data::operator int() const
{
    return _reset_counter;
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Data::operator Counter_t() const
{
    return _reset_counter;
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Data &ResetDetector::Data::operator=(Counter_t counter)
{
    _reset_counter = counter;
    return *this;
}

__RESET_DETECTOR_INLINE__
ResetDetector::Data &ResetDetector::Data::operator++()
{
    if (_reset_counter < kMaxCounter) {
        _reset_counter++;
    }
    return *this;
}

__RESET_DETECTOR_INLINE__
void ResetDetector::Data::setValid(bool valid)
{
    if (valid) {
        if (!*this) {
            _reset_counter = 0;
        }
    }
    else {
        _reset_counter = kInvalidCounter;
    }
}

__RESET_DETECTOR_INLINE_ALWAYS__
void ResetDetector::Data::setSafeMode(bool safe_mode)
{
    _safe_mode = safe_mode;
}

__RESET_DETECTOR_INLINE_ALWAYS__
bool ResetDetector::Data::isSafeMode() const
{
    return _safe_mode;
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Counter_t ResetDetector::Data::getResetCounter()
{
    return _reset_counter;
}

__RESET_DETECTOR_INLINE__
void ResetDetector::Data::pushReason(Reason_t reason)
{
    auto begin = _begin();
    std::copy(begin + 1, _end(), begin);
    *_current() = reason;
}

__RESET_DETECTOR_INLINE_ALWAYS__
bool ResetDetector::Data::hasValidReason() const
{
    return getReason() != kInvalidReason;
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Reason_t ResetDetector::Data::getReason() const
{
    return *const_cast<Data *>(this)->_current();
}

__RESET_DETECTOR_INLINE__
const ResetDetector::Reason_t *ResetDetector::Data::begin() const
{
    auto ptr = const_cast<Data *>(this)->_begin();
    while(ptr < end() && *ptr == kInvalidReason) {
        ptr++;
    }
    return ptr; // ptr >= _begin() && < _end()
}

__RESET_DETECTOR_INLINE_ALWAYS__
const ResetDetector::Reason_t *ResetDetector::Data::end() const
{
    return const_cast<Data *>(this)->_end();
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Reason_t *ResetDetector::Data::_begin()
{
    return _reason;
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Reason_t *ResetDetector::Data::_end()
{
    return &_reason[kReasonMax];
}

__RESET_DETECTOR_INLINE_ALWAYS__
ResetDetector::Reason_t *ResetDetector::Data::_current()
{
    return &_reason[kReasonMax - 1];
}

#ifdef __RESET_DETECTOR_INLINE_INLINE_DEFINED__
#undef  __RESET_DETECTOR_INLINE__
#undef __RESET_DETECTOR_INLINE_INLINE_DEFINED__
#ifdef __RESET_DETECTOR_NOINLINE__
#undef __RESET_DETECTOR_NOINLINE__
#endif
#endif

#if DEBUG_RESET_DETECTOR
#include <debug_helper_disable.h>
#endif
