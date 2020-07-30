/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "FormBase.h"

#define FormBitValue_UNSET_ALL 0    // static const T FormBitValue<T, N>::UNSET_ALL = 0;

template <typename T, size_t N>
class FormBitValue : public FormValue<T> {
public:
    FormBitValue(const String &name, T *value, std::array<T, N> bitmask, FormField::Type type = FormField::Type::SELECT) : FormValue<T>(name, value, type), _bitmask(bitmask) {
        FormField::initValue(String(_convertToValue(*value)));
    }

    virtual void copyValue() {
        this->_setValue(_convertToBitset(FormField::getValue().toInt()/* zero based index*/, this->_getValue()/* original bit set */));
    }

    virtual bool setValue(const String &value) override {
        return FormField::setValue(String(value.toInt()));      // copy 0 base index
    }

private:
    // return all bits in the bit mask array combined
    T _getCombinedBitmask() {
        T value = 0;
        for(auto bitmask = _bitmask.begin(); bitmask != _bitmask.end(); ++bitmask) {
            if (*bitmask != FormBitValue_UNSET_ALL) {
                value |= *bitmask;
            }
        }
        return value;
    }

    // convert bit set to 0 based index
    T _convertToValue(T bitset) {
        T value = _getBitValue(FormBitValue_UNSET_ALL);
        T comb = _getCombinedBitmask();
        for(auto bitmask = _bitmask.begin(); bitmask != _bitmask.end(); ++bitmask) {
            if ((*bitmask != FormBitValue_UNSET_ALL) && ((bitset & comb) == *bitmask)) {
                value = std::distance(_bitmask.begin(), bitmask);
            }
        }
        return value;
    }

    // convert 0 based index to bit set
    T _convertToBitset(T value, T bitset) {
        bitset &= ~_getCombinedBitmask(); // set bits to 0 that are set in the combined bit mask
        if (value >= 0 && value < _bitmask.size() && _bitmask[value] != FormBitValue_UNSET_ALL) { // set bits to 1 that belong to the selected value
            bitset |= _bitmask[value];
        }
        return bitset;
    }

    // get 0 based index for a bit mask
    T _getBitValue(T value) {
        auto it = std::find(_bitmask.begin(), _bitmask.end(), value);
        if (it != _bitmask.end()) {
            return std::distance(_bitmask.begin(), it);
        }
        return FormBitValue_UNSET_ALL;
    }

    std::array<T, N> _bitmask;
};
