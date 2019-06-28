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
    FormBitValue(const String &name, T *value, std::array<T, N> bitmask, FormField::FieldType_t type = FormField::INPUT_SELECT) : FormValue<T>(name, value, type) {
        _bitmask = bitmask;
        FormField::initValue(String(_convertToValue(*value)));

        T comb = 0;
        T tmp;
        tmp = this->_getValue();
        for(auto b = _bitmask.begin(); b != _bitmask.end(); ++b) {
            T val = _convertToValue(*b);
            if_debug_printf_P(PSTR("value %03d mask %s value %d to bitset %s\n"), std::distance(_bitmask.begin(), b), _bitmaskToString(*b).c_str(), (int)val, _bitmaskToString(_convertToBitset(val, *b)).c_str());
            comb |= *b;
        }
        if_debug_printf_P(PSTR("value xxx mask %s\n"), _bitmaskToString(comb).c_str());
        if_debug_printf_P(PSTR("value & ~mask  %s (%d)\n"), _bitmaskToString(tmp&comb).c_str(), (int)tmp);
    }

    virtual void copyValue() {
        this->_setValue(_convertToBitset(this->getValue()/*form value*/, this->_getValue()/*init value*/));
    }

    virtual bool setValue(const String &value) override {
        return FormField::setValue(String(_convertToValue(value.toInt())));
    }

private:
    T _convertToValue(T bitset) {
        T value = _getBitValue(FormBitValue_UNSET_ALL);
        //if_debug_forms_printf_P(PSTR(DEBUG_FORMS_PREFIX " all bits unset has the value %d\n"), value);
        for(auto bitmask = _bitmask.begin(); bitmask != _bitmask.end(); ++bitmask) {
            if ((*bitmask != FormBitValue_UNSET_ALL) && ((bitset & *bitmask) == *bitmask)) {
                value = std::distance(_bitmask.begin(), bitmask);
                //if_debug_forms_printf_P(PSTR(DEBUG_FORMS_PREFIX "mask %08lx matches %08lx, value %lu\n"), (unsigned long)(*bitmask), (unsigned long)bitset, (unsigned long)value);
            }
        }
        //if_debug_forms_printf_P(PSTR(DEBUG_FORMS_PREFIX "result: value %lu\n"), (unsigned long)value);
        return value;
    }

    T _convertToBitset(T value, T bitset) {
        T _combinedBitmask = 0;
        for(auto bitmask : _bitmask) {
            if (bitmask != FormBitValue_UNSET_ALL) {
                _combinedBitmask |= bitmask;
            }
        }
        bitset &= ~_combinedBitmask; // set only bits to 0 that are set in the bitmasks
        if (value >= 0 && value < _bitmask.size() && _bitmask[value] != FormBitValue_UNSET_ALL) { // set bits to 1 that belong to the selected value
            bitset |= _bitmask[value];
        }
        //if_debug_forms_printf_P(PSTR(DEBUG_FORMS_PREFIX "value %lu converts to %08lx, bitmask %08lx, bitset %08lx AND NOT %08lx (combined mask)\n"), (unsigned long)value, (unsigned long)bitset, (unsigned long)bitset, (unsigned long)(bitset & ~_combinedBitmask), (unsigned long)_combinedBitmask);
        return bitset;
    }

    T _getBitValue(T value) {
        auto it = std::find(_bitmask.begin(), _bitmask.end(), value);
        if (it != _bitmask.end()) {
            return std::distance(_bitmask.begin(), it);
        }
        return FormBitValue_UNSET_ALL;
    }

    String _bitmaskToString(T value) {
        String str;
        for(uint8_t i = 15; i >= 0; i--) {
            unsigned long mask = 1<<i;
            str += (value & mask) ? '1' : '0';
        }
        return str;
    }

    std::array<T, N> _bitmask;
};

