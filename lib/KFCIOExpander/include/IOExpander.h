/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <FunctionalInterrupt.h>
#include <wire.h>
#include <PrintHtmlEntities.h>
#include <vector>
#include <Schedule.h>

#ifndef DEBUG_IOEXPANDER
#define DEBUG_IOEXPANDER 1
#endif

#if DEBUG_IOEXPANDER
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif


#ifndef HAVE_IOEXPANDER
#   if HAVE_PCF8574 || HAVE_TINYPWM || HAVE_MCP23017 || HAVE_PCA9685
#       define HAVE_IOEXPANDER 1
#   else
#       define HAVE_IOEXPANDER 0
#   endif
#endif

#if HAVE_IOEXPANDER

// example
//
/*
#define IOEXPANDER_DEVICE_CONFIG Config<DeviceConfig<PCF8574, DeviceTypePCF8574, 0x20, 90, 98>, \
    Config<DeviceConfig<PCF8574, DeviceTypePCF8574, 0x22, 98, 104>, \
        Config<DeviceConfig<PCF8574, DeviceTypePCF8574, 0x24, 104, 112> \
        > \
    > \
>
*/
//
// platformi.ini
// since PIO does not escape the CLI properly replace '<'/'>' with " LT "/" GT "
//
// -D IOEXPANDER_DEVICE_CONFIG=Config\ LT\ DeviceConfig\ LT\ MCP23017,DeviceTypeMCP23017,0x20,100\ GT\ ,Config\ LT\ DeviceConfig\ LT\ MCP23017,DeviceTypeMCP23017,0x21,120\ GT\ GT\ GT
//
// "-DIOEXPANDER_DEVICE_CONFIG=Config LT DeviceConfig LT MCP23017,DeviceTypeMCP23017,0x20,100 GT GT"
//

#if !defined(IOEXPANDER_DEVICE_CONFIG)
#   error IOEXPANDER_DEVICE_CONFIG not defined
#endif

#ifndef __CONSTEXPR17
#   if __GNUC__ >= 10
#       define __CONSTEXPR17 constexpr
#   else
#       define __CONSTEXPR17
#   endif
#endif

namespace IOExpander {

    // scan bus for devices
    void scanBus(Print &output, TwoWire &wire = Wire, uint8_t fromAddress = 0x01, uint8_t toAddress = 0x7f, uint32_t delayMillis = 1);

    // use the default digitalWrite/digitalRead/pinMode/etc functions
    // for pins between 0 and  kDigitalPinCount -1
    static constexpr uint8_t kDigitalPinCount = NUM_DIGITAL_PINS;

    class PCF8574;
    class TinyPwm;
    class MCP23017;
    class PCA9685;

    enum class DeviceTypeEnum {
        PCF8574,
        TINYPWM,
        MCP23017,
        PCA9685,
        END
    };

    enum class TriggerMode {
        NONE,               // interrupts not available
        DEVICE_DEFAULT,     // use default mode of the device
        ACTIVE_HIGH,        // rising edge
        ACTIVE_LOW,         // falling edge
        OPEN_DRAIN,         // falling edge, requires pullup resistor
    };

    using InterruptCallback = std::function<void(uint16_t pinState)>;

    void IRAM_ATTR __interruptHandler(void *arg);

    inline const __FlashStringHelper *getDeviceName(DeviceTypeEnum type) {
        switch(type) {
            #if HAVE_PCF8574
                case DeviceTypeEnum::PCF8574:
                    return F("PCF8574");
            #endif
            #if HAVE_TINYPWM
                case DeviceTypeEnum::TINYPWM:
                    return F("TinyPWM");
            #endif
            #if HAVE_MCP23017
                case DeviceTypeEnum::MCP23017:
                    return F("MCP23017");
            #endif
            #if HAVE_PCA9685
                case DeviceTypeEnum::PCA9685:
                    return F("PCA9685");
            #endif
            default:
                break;
        }
        return nullptr;
    }

    template<DeviceTypeEnum _DeviceTypeEnum>
    constexpr const __FlashStringHelper *getDeviceName() {
        return getDeviceName(_DeviceTypeEnum);
    }

    template<typename _DeviceType>
    constexpr const __FlashStringHelper *getDeviceName() {
        return getDeviceName(_DeviceType::kDeviceType);
    }

    template<DeviceTypeEnum _DeviceTypeEnum, uint8_t _DefaultAddress, uint8_t _NumPins, bool _HasIsConnected, uint8_t _GPIOInterruptPinMode = 0, TriggerMode _IntTriggerMode = TriggerMode::NONE>
    struct DeviceTypeTemplate {
        using DeviceType = DeviceTypeTemplate<_DeviceTypeEnum, _DefaultAddress, _NumPins, _HasIsConnected>;

        static constexpr DeviceTypeEnum kDeviceType = _DeviceTypeEnum;
        static constexpr uint8_t kNumPins = _NumPins;
        // default I2C address
        static constexpr uint8_t kDefaultAddress = _DefaultAddress;
        // has isConnected() method
        static constexpr bool kHasIsConnected = _HasIsConnected;
        // pinMode for GPIO pin used for interrupts
        static constexpr uint8_t kIntPinMode = _GPIOInterruptPinMode;
        // interrupt trigger mode
        static constexpr TriggerMode kIntTriggerMode = _IntTriggerMode;

    };

    using DeviceTypePCF8574 = DeviceTypeTemplate<DeviceTypeEnum::PCF8574, 0x20 + 1, 8, true, INPUT_PULLUP, TriggerMode::OPEN_DRAIN>;
    using DeviceTypeTinyPwm = DeviceTypeTemplate<DeviceTypeEnum::TINYPWM, 0x60, 2, true>;
    using DeviceTypeMCP23017 = DeviceTypeTemplate<DeviceTypeEnum::MCP23017, 0x20, 16, true, INPUT, TriggerMode::ACTIVE_HIGH>;
    using DeviceTypePCA9685 = DeviceTypeTemplate<DeviceTypeEnum::PCA9685, 0x40, 16, false>;

    struct DeviceTypeEnd {
        using ConfigType = DeviceTypeEnd;
        using NextConfigType = nullptr_t;
        using DeviceConfigType = void;
        using DeviceType = void;
        using DeviceClassType = void;
        static constexpr DeviceTypeEnum kDeviceType = DeviceTypeEnum::END;
        static constexpr uint8_t kNumPins = 0;
        static constexpr uint8_t kDefaultAddress = 0;
        static constexpr bool kHasIsConnected = false;
        static constexpr bool kHasNext = false;


        #if !HAVE_PCF8574
            static_assert(kDeviceType != DeviceTypeEnum::PCF8574, "PCF8574 not enabled");
        #endif
        #if !HAVE_TINYPWM
            static_assert(kDeviceType != DeviceTypeEnum::TINYPWM, "TINYPWM not enabled");
        #endif
        #if !HAVE_MCP23017
            static_assert(kDeviceType != DeviceTypeEnum::MCP23017, "MCP23017 not enabled");
        #endif
        #if !HAVE_PCA9685
            static_assert(kDeviceType != DeviceTypeEnum::PCA9685, "PCA9685 not enabled");
        #endif
    };

    template<typename _ConfigType>
    struct ConfigIterator;

    struct ConfigEndIterator {
        void _beginRecursive(TwoWire &wire);
        void _printStatusRecursive(Print &output);
        constexpr size_t _sizeRecursive() const;
        void _pinModeRecursive(uint8_t pin, uint8_t mode);
        void _digitalWriteRecursive(uint8_t pin, uint8_t val);
        int _digitalReadRecursive(uint8_t pin);
        void _analogReferenceRecursive(uint8_t mode);
        void _analogWriteRecursive(uint8_t pin, int val);
        int _analogReadRecursive(uint8_t pin);
        void _analogWriteFreqRecursive(uint32_t freq);
        void *_getDevicePointerRecursive(uint8_t pin);
        bool interruptsEnabled();
        void _attachInterruptRecursive(void *device, uint8_t gpioPin, uint16_t pinMask, const InterruptCallback &callback, uint8_t mode, TriggerMode triggerMode);
        void _detachInterruptRecursive(void *device, uint8_t gpioPin, uint16_t pinMask);
        void _setInterruptFlagRecursive(void *device);
        void _dumpPinsRecursive(Print &output);
    };

    template<typename _DeviceConfigType, typename _NextConfigType = DeviceTypeEnd>
    struct Config {
        using ConfigType = Config<_DeviceConfigType, _NextConfigType>;
        using DeviceConfigType = _DeviceConfigType;
        using DeviceType = typename DeviceConfigType::DeviceType;
        using DeviceClassType = typename DeviceConfigType::DeviceClassType;

        static constexpr bool kHasNext = !std::is_same<_NextConfigType, DeviceTypeEnd>::value;

        using NextConfigType = _NextConfigType;
        using NextDeviceConfigType = typename std::conditional<kHasNext, typename NextConfigType::DeviceConfigType, DeviceConfigType>::type;
        using NextDeviceType = typename std::conditional<kHasNext, typename NextDeviceConfigType::DeviceType, DeviceType>::type;
        using NextDeviceClassType = typename std::conditional<kHasNext, typename NextDeviceConfigType::DeviceClassType, nullptr_t>::type;

        using NextConfigIterator = typename std::conditional<kHasNext, ConfigIterator<NextConfigType>, ConfigEndIterator>::type;
    };


    template<typename _ConfigType>
    struct ConfigIterator {
        using ConfigType = _ConfigType;
        using DeviceClassType = typename ConfigType::DeviceClassType;
        using DeviceConfigType = typename ConfigType::DeviceConfigType;
        using DeviceType = typename ConfigType::DeviceType;
        using NextConfigIterator = typename ConfigType::NextConfigIterator;

        DeviceClassType _device;
        NextConfigIterator _next;

        void begin(TwoWire &wire) {
            _beginRecursive(wire);
        }

        void begin() {
            _beginRecursive(Wire);
        }

        void printStatus(Print &output) {
            _printStatusRecursive(output);
        }

        constexpr size_t size() const {
            return _sizeRecursive();
        }

        void pinMode(uint8_t pin, uint8_t mode) {
            _pinModeRecursive(pin, mode);
        }

        void IRAM_ATTR digitalWrite(uint8_t pin, uint8_t val) {
            _pinModeRecursive(pin, val);
        }

        int IRAM_ATTR digitalRead(uint8_t pin) {
            return _digitalReadRecursive(pin);
        }

        int analogRead(uint8_t pin) {
            return _analogReadRecursive(pin);
        }

        void analogReference(uint8_t mode)  {
            _analogReferenceRecursive(mode);
        }

        void analogWrite(uint8_t pin, int val) {
            _analogWriteRecursive(pin, val);
        }

        void analogWriteFreq(uint32_t freq) {
            _analogWriteFreqRecursive(freq);
        }

        // return device pointer for given pin
        void *getDevicePointer(uint8_t pin) {
            return _getDevicePointerRecursive(pin);
        }

        // return true if any device has interrupts enabled
        bool interruptsEnabled() {
            return _interruptsEnabledRecursive();
        }

        // interrupts can only be enabled per device
        // the hardware interrupt must be triggered from a single GPIO pin
        // the callback will be called as scheduled_function outside the interrupt handler
        // and the handler does not have to be in IRAM
        //
        // gpioPin is the GPIO pin for the interrupt
        // triggerMode is the interrupt mode for the GPIO pin
        void attachInterrupt(uint8_t gpioPin, void *device, uint16_t pinMask, const InterruptCallback &callback, uint8_t mode, TriggerMode triggerMode = TriggerMode::DEVICE_DEFAULT) {
            __LDBG_printf("attachInterrupt gpio=%u device=%p mode=%u trigger_mode=%u", gpioPin, device, mode, triggerMode);
            _attachInterruptRecursive(device, gpioPin, pinMask, callback, mode, triggerMode);
        }

        // remove interrupt handler
        void detachInterrupt(uint8_t gpioPin, void *device, uint16_t pinMask) {
            __LDBG_printf("detachInterrupt gpio=%u device=%p", gpioPin, device);
            _detachInterruptRecursive(device, gpioPin, pinMask);
        }

        void dumpPins(Print &output)
        {
            _dumpPinsRecursive(output);
        }

        inline  __attribute__((__always_inline__))
        void _beginRecursive(TwoWire &wire) {
            _device.begin(DeviceConfigType::kI2CAddress, &wire);
            _next._beginRecursive(wire);
        }

        inline  __attribute__((__always_inline__))
        void _dumpPinsRecursive(Print &output)
        {
            output.printf_P(PSTR("+%s@0x%02x: "), _device.getDeviceName(), _device.getAddress());
            for(uint8_t i = DeviceConfigType::kBeginPin; i < DeviceConfigType::kEndPin; i++) {
                Serial.printf_P(PSTR("%u(%i)=%u "), i, i - DeviceConfigType::kBeginPin,  _device.analogRead(i));
            }
            Serial.print('\n');
            _next._dumpPinsRecursive(output);
        }

        inline  __attribute__((__always_inline__))
        void _printStatusRecursive(Print &output) {
            output.printf_P(PSTR(HTML_S(div) "%s @ I2C address 0x%02x\n"), _device.getDeviceName(), _device.getAddress());
            if __CONSTEXPR17 (DeviceType::kHasIsConnected) {
                if (!_device.isConnected()) {
                    output.print(F(HTML_S(br) "ERROR - Device not found!\n"));
                }
            }
            output.print(F(HTML_E(div)));
            _next._printStatusRecursive(output);
        }

        constexpr size_t _sizeRecursive() const {
            return _next._sizeRecursive() + 1;
        }

        inline  __attribute__((__always_inline__))
        void _pinModeRecursive(uint8_t pin, uint8_t mode) {
            if (DeviceConfigType::pinMatch(pin)) {
                _device.pinMode(pin, mode);
                return;
            }
            _next._pinModeRecursive(pin, mode);
        }

        inline  __attribute__((__always_inline__))
        void _digitalWriteRecursive(uint8_t pin, uint8_t val) {
            if (DeviceConfigType::pinMatch(pin)) {
                _device.digitalWrite(pin, val);
                return;
            }
            _next._digitalWriteRecursive(pin, val);
        }

        inline  __attribute__((__always_inline__))
        int _digitalReadRecursive(uint8_t pin) {
            if (DeviceConfigType::pinMatch(pin)) {
                return _device.digitalRead(pin);
            }
            return _next._digitalReadRecursive(pin);
        }

        inline  __attribute__((__always_inline__))
        int _analogReadRecursive(uint8_t pin) {
            if (DeviceConfigType::pinMatch(pin)) {
                return _device.analogRead(pin);
            }
            return _next._analogReadRecursive(pin);
        }

        inline  __attribute__((__always_inline__))
        void _analogReferenceRecursive(uint8_t mode) {
            _device.analogReference(mode);
            return _next._analogReferenceRecursive(mode);
        }

        inline  __attribute__((__always_inline__))
        void _analogWriteRecursive(uint8_t pin, int val) {
            if (DeviceConfigType::pinMatch(pin)) {
                _device.analogWrite(pin, val);
                return;
            }
            _next._analogWriteRecursive(pin, val);
        }

        inline  __attribute__((__always_inline__))
        void _analogWriteFreqRecursive(uint32_t freq) {
            _device.analogWriteFreq(freq);
            _next._analogWriteFreqRecursive(freq);
        }

        inline  __attribute__((__always_inline__))
        void *_getDevicePointerRecursive(uint8_t pin) {
            if (DeviceConfigType::pinMatch(pin)) {
                return reinterpret_cast<void *>(&_device);
            }
            _next._getDevicePointerRecursive(pin);
        }

        inline  __attribute__((__always_inline__))
        bool _interruptsEnabledRecursive() {
            if (_device.interruptsEnabled()) {
                return true;
            }
            _next._interruptsEnabledRecursive();
        }

        inline  __attribute__((__always_inline__))
        void _attachInterruptRecursive(void *device, uint8_t gpioPin, uint16_t pinMask, const InterruptCallback &callback, uint8_t mode, TriggerMode triggerMode) {
            if (device == reinterpret_cast<void *>(&_device)) {
                bool enabled = _device.interruptsEnabled();
                _device.enableInterrupts(pinMask, callback, mode, triggerMode);
                // attach the interrupt handler if interrupts are not enabled for this GPIO pin
                if (enabled == false && _device.interruptsEnabled()) {
                    // set pinMode if device has a preset
                    if __CONSTEXPR17 (DeviceType::kIntPinMode) {
                        ::pinMode(gpioPin, DeviceType::kIntPinMode);
                    }
                    static_assert(DeviceType::kIntTriggerMode != TriggerMode::NONE, "interrupts not available");
                    __LDBG_printf("attachInterruptArg device=%s gpio=%u mode=%u", _device.getDeviceName(), gpioPin, triggerMode, _triggerMode2IntMode(triggerMode));
                    ::attachInterruptArg(gpioPin, __interruptHandler, device, _triggerMode2IntMode(triggerMode));
                }
                return;
            }
            _next._attachInterruptRecursive(device, gpioPin, pinMask, callback, mode, triggerMode);
        }

        inline  __attribute__((__always_inline__))
        void _detachInterruptRecursive(void *device, uint8_t gpioPin, uint16_t pinMask) {
            if (device == reinterpret_cast<void *>(&_device)) {
                if (_device.interruptsEnabled() == false) {
                    // remove interrupt handler
                    __LDBG_printf("detachInterrupt device=%s gpio=%u", _device.getDeviceName(), gpioPin);
                    ::detachInterrupt(gpioPin);
                }
                _device.disableInterrupts();
                return;
            }
            _next._detachInterruptRecursive(device);
        }

        inline  __attribute__((__always_inline__))
        void _setInterruptFlagRecursive(void *device) {
            if (device == reinterpret_cast<void *>(&_device)) {
                // setInterrutFlag() returns false if any interrupts are pending
                // the function will not be installed again until the flag has been cleared
                if (_device.setInterruptFlag() == false) {
                    // run function outside ISR
                    schedule_function([this]() {
                        _device.interruptHandler();
                    });
                }
                return;
            }
            _next._setInterruptFlagRecursive(device);
        }

    protected:
        constexpr int _triggerMode2IntMode(TriggerMode mode) const {
            return (mode == TriggerMode::DEVICE_DEFAULT) ? _triggerMode2IntMode(DeviceType::kIntTriggerMode) : (mode == TriggerMode::ACTIVE_HIGH) ? RISING : FALLING;
        }

    };

    template<typename _DeviceClassType, typename _DeviceType, uint8_t _Address, uint8_t _BeginPin, uint8_t _EndPin = 0>
    struct DeviceConfig {
        using DeviceClassType = _DeviceClassType;
        using DeviceType = _DeviceType;

        static constexpr DeviceTypeEnum kDeviceType = DeviceType::kDeviceType;
        static constexpr uint8_t kI2CAddress = _Address;
        static constexpr uint8_t kBeginPin = _BeginPin;
        static constexpr uint8_t kEndPin = _EndPin == 0 ? (kBeginPin + DeviceType::kNumPins) : _EndPin;
        static constexpr uint8_t kNumPins = kEndPin - kBeginPin;

        static_assert(_BeginPin >= kDigitalPinCount, "_BeginPin must be greater or eqal kDigitalPinCount");

        static constexpr bool pinMatch(uint8_t pin) {
            return pin >= kBeginPin && pin <= kEndPin;
        }
    };

    template<typename _DeviceType, typename _DeviceClassType>
    class Base {
    public:
        using DeviceType = _DeviceType;
        using DeviceClassType = _DeviceClassType;

        static constexpr uint8_t kDefaultAddress = DeviceType::kDefaultAddress;
        static constexpr uint8_t kNumPins = DeviceType::kNumPins;
        static constexpr DeviceTypeEnum kDeviceType = DeviceType::kDeviceType;

    public:
        Base(uint8_t address = 0, TwoWire *wire = &Wire) : _address(address), _wire(wire) {}

        void begin(uint8_t address, TwoWire *wire);
        void begin(uint8_t address);
        void begin();

        bool isConnected() const;
        uint8_t getAddress() const;
        TwoWire &getWire();

        constexpr const __FlashStringHelper *getDeviceName() {
            return IOExpander::getDeviceName<DeviceType>();
        }

    protected:
        uint8_t _address;
        TwoWire *_wire;
    };

    namespace IOWrapper {

        template<typename _Parent, typename _Ta, typename _Tb>
        class IOWrapper {
        public:
            IOWrapper(_Parent &parent, _Ta value) :
                _value(value),
                _parent(&parent)
            {
            }

            IOWrapper(_Parent *parent, _Ta value) :
                _value(value),
                _parent(parent)
            {
            }

            _Tb &operator|=(int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value | value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator|=(long unsigned int  value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value | value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator&=(int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value & value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator&=(long unsigned int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value & value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator=(int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value);
                return *reinterpret_cast<_Tb *>(this);
            }

            _Tb &operator=(long unsigned int value) {
                *reinterpret_cast<_Tb *>(this) = static_cast<_Ta>(_value);
                return *reinterpret_cast<_Tb *>(this);
            }

            operator int() {
                return static_cast<int>(_value);
            }

            operator uint8_t() {
                return static_cast<uint8_t>(_value);
            }


        protected:
            friend PCF8574;

            _Ta _value;
            _Parent *_parent;
        };

    }

}

#include "PCF8574.h"
#include "TinyPwm.h"
#include "MCP23017.h"
#include "PCA9685.h"

#include "IOExpander.hpp"
#include "ConfigEndIterator.hpp"
#include "PCF8574.hpp"
#include "TinyPwm.hpp"
#include "MCP23017.hpp"
#include "PCA9685.hpp"

#endif
