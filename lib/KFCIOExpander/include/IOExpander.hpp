/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "IOExpander.h"

#if HAVE_IOEXPANDER

#if DEBUG_IOEXPANDER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

#ifndef IOEXPANDER_INLINE
#define IOEXPANDER_INLINE inline
#endif

namespace IOExpander {

    template<typename _DeviceType, typename _DeviceClassType>
    inline void Base<_DeviceType, _DeviceClassType>::begin(uint8_t address, TwoWire *wire)
    {
        _wire = wire;
        begin(address);
    }

    template<typename _DeviceType, typename _DeviceClassType>
    inline void Base<_DeviceType, _DeviceClassType>::begin(uint8_t address)
    {
        _address = address;
    }

    template<typename _DeviceType, typename _DeviceClassType>
    inline void Base<_DeviceType, _DeviceClassType>::begin()
    {
    }

    template<typename _DeviceType, typename _DeviceClassType>
    inline bool Base<_DeviceType, _DeviceClassType>::isConnected() const
    {
        if (!_address || !_wire) {
            return false;
        }
        _wire->beginTransmission(_address);
        return (_wire->endTransmission() == 0);
    }

    template<typename _DeviceType, typename _DeviceClassType>
    inline uint8_t Base<_DeviceType, _DeviceClassType>::getAddress() const
    {
        return _address;
    }

    template<typename _DeviceType, typename _DeviceClassType>
    inline TwoWire &Base<_DeviceType, _DeviceClassType>::getWire()
    {
        return *_wire;
    }

    #define LT <
    #define GT >

    extern ConfigIterator<IOEXPANDER_DEVICE_CONFIG> config;

    #undef LT
    #undef GT

}

#endif
