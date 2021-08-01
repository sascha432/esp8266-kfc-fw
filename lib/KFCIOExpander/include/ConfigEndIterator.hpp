/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"

namespace IOExpander {

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_beginRecursive(TwoWire &wire)
    {
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_printStatusRecursive(Print &output)
    {
    }

    constexpr size_t ConfigEndIterator::_sizeRecursive() const
    {
        return 0;
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_pinModeRecursive(uint8_t pin, uint8_t mode)
    {
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_digitalWriteRecursive(uint8_t pin, uint8_t val)
    {
    }

    inline __attribute__((__always_inline__))
    int ConfigEndIterator::_digitalReadRecursive(uint8_t pin)
    {
        return 0;
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_analogReferenceRecursive(uint8_t mode)
    {
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_analogWriteRecursive(uint8_t pin, int val)
    {
    }

    inline __attribute__((__always_inline__))
    int ConfigEndIterator::_analogReadRecursive(uint8_t pin)
    {
        return 0;
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_analogWriteFreqRecursive(uint32_t freq)
    {
    }

    inline __attribute__((__always_inline__))
    void *ConfigEndIterator::_getDevicePointerRecursive(uint8_t pin)
    {
        return nullptr;
    }

    inline __attribute__((__always_inline__))
    bool ConfigEndIterator::interruptsEnabled()
    {
        return false;
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_attachInterruptRecursive(void *device, uint8_t gpioPin, uint16_t pinMask, const InterruptCallback &callback, uint8_t mode, TriggerMode triggerMode)
    {
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_detachInterruptRecursive(void *device, uint8_t gpioPin, uint16_t pinMask)
    {
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_setInterruptFlagRecursive(void *device)
    {
    }

    inline __attribute__((__always_inline__))
    void ConfigEndIterator::_dumpPinsRecursive(Print &output)
    {
    }

}
