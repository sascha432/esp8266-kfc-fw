/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <stl_ext/type_traits.h>
#include "Validator/BaseValidator.h"

namespace FormUI {

    namespace Validator {

        namespace RangeMessage {

            String getDefaultMessage(const String &message, bool allowZero);

        }

        template<typename _Ta>
        class RangeTemplate : public BaseValidator
        {
        public:
            using EnumType = _Ta;
            using BaseType = typename std::relaxed_underlying_type<_Ta>::type;

        public:
            RangeTemplate(const String &message = String()) :
                RangeTemplate(std::numeric_limits<BaseType>::min(), std::numeric_limits<BaseType>::max(), false, RangeMessage::getDefaultMessage(message, false))
            {}

            RangeTemplate(_Ta aMin, _Ta aMax, bool allowZero = false, const String &message = String()) :
                BaseValidator(RangeMessage::getDefaultMessage(message, allowZero)),
                _minValue(aMin),
                _maxValue(aMax),
                _digits(0),
                _allowZero(allowZero)
            {}

            RangeTemplate(_Ta aMin, _Ta aMax, uint8_t floatingPointDigits = 2, bool allowZero = false, const String &message = String()) :
                BaseValidator(RangeMessage::getDefaultMessage(message, allowZero)),
                _minValue(aMin),
                _maxValue(aMax),
                _digits(floatingPointDigits),
                _allowZero(allowZero)
            {}

            virtual bool validate() {
                if (BaseValidator::validate()) {
                    auto value = static_cast<EnumType>(_getValue(BaseType()));
                    return (_allowZero && static_cast<BaseType>(value) == 0) || (value >= _minValue && value <= _maxValue);
                }
                return false;
            }

            virtual String getMessage() override
            {
                String message = BaseValidator::getMessage();
                message.replace(FSPGM(FormValidator_min_macro), String(static_cast<BaseType>(_minValue)));
                message.replace(FSPGM(FormValidator_max_macro), String(static_cast<BaseType>(_maxValue)));
                return message;
            }

        protected:
            int _getValue(int) {
                return getField().getValue().toInt();
            }
            unsigned _getValue(unsigned) {
                return getField().getValue().toInt();
            }
            double _getValue(double) {
                return getField().getValue().toFloat();
            }

        protected:
            _Ta _minValue;
            _Ta _maxValue;
            uint8_t _digits: 7;
            uint8_t _allowZero: 1;
        };

        class Range : public RangeTemplate<int32_t> {
        public:
            Range(int32_t aMin, int32_t aMax, bool allowZero = false, const String &message = String()) : RangeTemplate(aMin, aMax, allowZero, message) {}

            Range(const String &message, int32_t aMin, int32_t aMax, bool allowZero = false) __attribute__((deprecated)) : RangeTemplate(aMin, aMax, allowZero, message) {}
        };

        class NetworkPort : public RangeTemplate<uint16_t>
        {
        public:
            static constexpr long kPortMin = 1;
            static constexpr long kPortMax = 65535;

            NetworkPort(uint16_t aMin, uint16_t aMax, bool allowZero = false) : RangeTemplate(aMin, aMax, allowZero) {}
            NetworkPort(bool allowZero = false) : RangeTemplate(kPortMin, kPortMax, allowZero) {}
        };

        class RangeDouble : public RangeTemplate<double> {
        public:
            RangeDouble(double aMin, double aMax, uint8_t digits = 2, bool allowZero = false) : RangeTemplate(aMin, aMax, digits, allowZero) {}
            RangeDouble(const String &message, double aMin, double aMax, uint8_t digits = 2, bool allowZero = false) : RangeTemplate(aMin, aMax, digits, allowZero, message) {}

            virtual bool validate();
            virtual String getMessage() override;
        };

        template<typename _Ta, _Ta _minValue = _Ta::MIN, _Ta _maxValue = _Ta::MAX, int _excludingMax = 1>
        class EnumRange : public RangeTemplate<_Ta>
        {
        public:
            using EnumType = _Ta;
            using IntType = std::relaxed_underlying_type_t<_Ta>;

            static constexpr _Ta kMinValue = _minValue;
            //static constexpr _Ta kMaxValue = (EnumType)(((IntType)_maxValue) - 1);
            static constexpr _Ta kMaxValue = static_cast<EnumType>(static_cast<IntType>(_maxValue) - _excludingMax);

            EnumRange(const String &message) :
                RangeTemplate<_Ta>(kMinValue, kMaxValue, false, message)
            {
            }

            EnumRange(_Ta aMin = kMinValue, _Ta aMax = kMaxValue, const String &message = String()) :
                RangeTemplate<_Ta>(aMin, aMax, false, message)
            {
            }
        };

    }

}


