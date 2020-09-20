/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Field/BaseField.h"
#include "Field/ValuePointer.h"

#include "Utility/Debug.h"

#define FormBitValue_UNSET_ALL 0    // static const _Ta FormBitValue<_Ta, N>::UNSET_ALL = 0;

namespace FormUI {

    namespace Field {

        template <typename _Ta, size_t N>
        class BitSet : public Field::ValuePointer<_Ta> {
        public:
            using VarType = typename Field::ValuePointer<_Ta>::VarType;

        public:
            BitSet(const char *name, VarType *value, std::array<_Ta, N> bitmask, Field::Type type = Field::Type::SELECT) :
                Field::ValuePointer<_Ta>(name, value, type),
                _bitmask(bitmask)
            {
                Field::BaseField::initValue(String(_convertToValue(*value)));
            }

            virtual void copyValue() {
                this->_setValue(_convertToBitset(Field::BaseField::getValue().toInt()/* zero based index*/, this->_getValue()/* original bit set */));
            }

            virtual bool setValue(const String &value) override {
                return Field::BaseField::setValue(String(value.toInt()));      // copy 0 base index
            }

        private:
            // return all bits in the bit mask array combined
            _Ta _getCombinedBitmask() {
                _Ta value = 0;
                for(auto bitmask = _bitmask.begin(); bitmask != _bitmask.end(); ++bitmask) {
                    if (*bitmask != FormBitValue_UNSET_ALL) {
                        value |= *bitmask;
                    }
                }
                return value;
            }

            // convert bit set to 0 based index
            _Ta _convertToValue(_Ta bitset) {
                _Ta value = _getBitValue(FormBitValue_UNSET_ALL);
                _Ta comb = _getCombinedBitmask();
                for(auto bitmask = _bitmask.begin(); bitmask != _bitmask.end(); ++bitmask) {
                    if ((*bitmask != FormBitValue_UNSET_ALL) && ((bitset & comb) == *bitmask)) {
                        value = std::distance(_bitmask.begin(), bitmask);
                    }
                }
                return value;
            }

            // convert 0 based index to bit set
            _Ta _convertToBitset(_Ta value, _Ta bitset) {
                bitset &= ~_getCombinedBitmask(); // set bits to 0 that are set in the combined bit mask
                if (value >= 0 && value < _bitmask.size() && _bitmask[value] != FormBitValue_UNSET_ALL) { // set bits to 1 that belong to the selected value
                    bitset |= _bitmask[value];
                }
                return bitset;
            }

            // get 0 based index for a bit mask
            _Ta _getBitValue(_Ta value) {
                auto it = std::find(_bitmask.begin(), _bitmask.end(), value);
                if (it != _bitmask.end()) {
                    return std::distance(_bitmask.begin(), it);
                }
                return FormBitValue_UNSET_ALL;
            }

            std::array<_Ta, N> _bitmask;
        };

    }
}

#include <debug_helper_disable.h>
