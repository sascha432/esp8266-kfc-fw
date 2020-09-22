/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationParameter.h"

template<class _Ta>
class ConfigurationParameterT : public ConfigurationParameter
{
public:
    using ConfigurationParameter::ConfigurationParameter;

    operator bool() const {
        return _param.hasData();
    }

    _Ta &get() {
        return *reinterpret_cast<_Ta *>(_param.data());
    }
    void set(const _Ta &value) {
        __LDBG_assert_panic(_param.isWriteable(), "not writable");
        *reinterpret_cast<_Ta *>(_param._writeable->begin()) = value;
    }
};

template<>
class ConfigurationParameterT <char *> : public ConfigurationParameter
{
public:
    using ConfigurationParameter::ConfigurationParameter;

    operator bool() const {
        return _param.hasData();
    }

    void set(const char *value) {
        __LDBG_assert_panic(_param.isWriteable(), "not writable");
        auto maxLength = _param._writeable->size() - 1;
        strncpy(_param.string(), value, maxLength)[maxLength] = 0;
    }
    void set(const __FlashStringHelper *value) {
        __LDBG_assert_panic(_param.isWriteable(), "not writable");
        auto maxLength = _param._writeable->size() - 1;
        strncpy_P(_param.string(), reinterpret_cast<PGM_P>(value), maxLength)[maxLength] = 0;
    }
    void set(const String &value) {
        set(value.c_str());
    }
};
