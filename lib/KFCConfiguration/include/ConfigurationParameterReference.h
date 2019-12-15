/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

// ConfigurationParameterReference allows to access parameters as objects
// The memory is released when the object is destroyed
//
// use ConfigurationParameterFactory::get() to create ConfigurationParameterReference objects

#include "ConfigurationParameter.h"
#include "Configuration.h"
#include <type_traits>

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

class ConfigurationParameterReference {
public:
    ConfigurationParameterReference(Configuration &_configuration, uint16_t handle);
    ~ConfigurationParameterReference();

protected:
    ConfigurationParameter &get() const;

    inline uint16_t getPositon() const {
        return _position;
    }

private:
    uint16_t _position;   
    uint16_t _handle;
    Configuration &_configuration;
};

template <typename T>
class ConfigurationParameterVar: public ConfigurationParameterReference {
public:
    static_assert(!std::is_same<char *, T>::value, "use ConfigurationParameterString");
    static_assert(!std::is_pointer<T>::value, "use ConfigurationParameterBinary");

    using ConfigurationParameterReference::ConfigurationParameterReference;

    operator T() {
        return *reinterpret_cast<T *>(ConfigurationParameterReference::get().getDataPtr());
    }

    ConfigurationParameterVar &operator =(const T &value) {
        set(value);
        return *this;
    }

    const T *operator ->() const {
        return get();
    }

    const T &get() const {
        return *reinterpret_cast<T *>(ConfigurationParameterReference::get().getDataPtr());
    }

    T &getWriteable() {
        auto parameter = ConfigurationParameterReference::get();
        parameter.setDirty(true);
        return *reinterpret_cast<T *>(ConfigurationParameterReference::get().getDataPtr());
    }

    void set(const T &value) {
        auto parameter = ConfigurationParameterReference::get();
        parameter.setDirty(true);
        *reinterpret_cast<T *>(parameter.getDataPtr()) = value;
    }
};

class ConfigurationParameterConstString: public ConfigurationParameterReference {
public:
    using ConfigurationParameterReference::ConfigurationParameterReference;

    inline operator const char *() const {
        return get();
    }

    const char *get() const;
};

class ConfigurationParameterConstBinary: public ConfigurationParameterReference {
public:
    using ConfigurationParameterReference::ConfigurationParameterReference;

    inline operator const void *() const {
        return get();
    }

    const void *get() const;
    uint16_t getSize() const;
};


class ConfigurationParameterString: public ConfigurationParameterReference {
public:
    using ConfigurationParameterReference::ConfigurationParameterReference;

    inline operator const char *() const {
        return get();
    }

    const char *get() const;
    void set(const String & string);
    void set(const char * string);
    void set(const __FlashStringHelper *string);
    uint16_t getSize() const;
};

class ConfigurationParameterBinary: public ConfigurationParameterReference {
public:
    using ConfigurationParameterReference::ConfigurationParameterReference;

    inline operator const void *() const {
        return get();
    }

    const void *get() const;
    void set(const void *data, uint16_t length);
    void set_P(const void *data, uint16_t length);
    uint16_t getSize() const;
};

class ConfigurationParameterFactory {
public:
    static ConfigurationParameterConstString get(Configuration &configuration, ConfigurationParameterT<char *> *parameter);
    static ConfigurationParameterConstBinary get(Configuration &configuration, ConfigurationParameterT<void *> *parameter);

    static ConfigurationParameterString get(Configuration &configuration, const ConfigurationParameterT<char *> &parameter);
    static ConfigurationParameterBinary get(Configuration &configuration, const ConfigurationParameterT<void *> &parameter);

    template <typename T>
    static ConfigurationParameterVar<T> get(Configuration &configuration, const ConfigurationParameterT<T> &parameter) {
        return ConfigurationParameterVar<T>(configuration, configuration.getPosition(&parameter));
    }
};

#include <debug_helper_enable.h>
