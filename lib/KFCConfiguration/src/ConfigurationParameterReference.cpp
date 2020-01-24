/**
* Author: sascha_lammers@gmx.de
*/

#include "ConfigurationParameterReference.h"

#if DEBUG_CONFIGURATION
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

ConfigurationParameterReference::ConfigurationParameterReference(Configuration & _configuration, uint16_t position) : _configuration(_configuration) {
    _position = position;
}

ConfigurationParameterReference::~ConfigurationParameterReference() {
    auto &parameter = get();
    _debug_printf_P(PSTR("ConfigurationParameterReference::~ConfigurationParameterReference(): %04x (%s)\n"), parameter.getParam().handle, getHandleName(parameter.getParam().handle));
    parameter.freeData();
}

ConfigurationParameter & ConfigurationParameterReference::get() const {
    return _configuration.getParameterByPosition(_position);
}


ConfigurationParameterConstString ConfigurationParameterFactory::get(Configuration &configuration, ConfigurationParameterT<char *> *parameter) {
    return ConfigurationParameterConstString(configuration, configuration.getPosition(parameter));
}

ConfigurationParameterConstBinary ConfigurationParameterFactory::get(Configuration &configuration, ConfigurationParameterT<void *> *parameter) {
    return ConfigurationParameterConstBinary(configuration, configuration.getPosition(parameter));
}

ConfigurationParameterString ConfigurationParameterFactory::get(Configuration &configuration, const ConfigurationParameterT<char *> &parameter) {
    return ConfigurationParameterString(configuration, configuration.getPosition(&parameter));
}

ConfigurationParameterBinary ConfigurationParameterFactory::get(Configuration &configuration, const ConfigurationParameterT<void *> &parameter) {
    return ConfigurationParameterBinary(configuration, configuration.getPosition(&parameter));
}

const char * ConfigurationParameterConstString::get() const {
    if (getPositon() == 0xffff) {
        return "";
    }
    return reinterpret_cast<const char *>(ConfigurationParameterReference::get().getDataPtr());
}

const void * ConfigurationParameterConstBinary::get() const {
    if (getPositon() == 0xffff) {
        return nullptr;
    }
    return reinterpret_cast<const char *>(ConfigurationParameterReference::get().getDataPtr());
}

uint16_t ConfigurationParameterConstBinary::getSize() const {
    return ConfigurationParameterReference::get().getSize();
}

const char * ConfigurationParameterString::get() const {
    return reinterpret_cast<const char *>(ConfigurationParameterReference::get().getDataPtr());
}

void ConfigurationParameterString::set(const String & string) {
    auto &parameter = ConfigurationParameterReference::get();
    parameter.setDirty(true);
    strncpy((char *)parameter.getDataPtr(), string.c_str(), parameter.getSize());
}

void ConfigurationParameterString::set(const char * string) {
    auto &parameter = ConfigurationParameterReference::get();
    parameter.setDirty(true);
    strncpy((char *)parameter.getDataPtr(), string, parameter.getSize());
}

void ConfigurationParameterString::set(const __FlashStringHelper * string) {
    auto &parameter = ConfigurationParameterReference::get();
    parameter.setDirty(true);
    strncpy_P((char *)parameter.getDataPtr(), RFPSTR(string), parameter.getSize());
}

uint16_t ConfigurationParameterString::getSize() const {
    return ConfigurationParameterReference::get().getSize();
}

const void * ConfigurationParameterBinary::get() const {
    return reinterpret_cast<const void *>(ConfigurationParameterReference::get().getDataPtr());
}

void ConfigurationParameterBinary::set(const void * data, uint16_t length) {
    auto &parameter = ConfigurationParameterReference::get();
    parameter.setDirty(true);
    memcpy(parameter.getDataPtr(), data, std::min(length, parameter.getSize()));
}

void ConfigurationParameterBinary::set_P(const void * data, uint16_t length) {
    auto &parameter = ConfigurationParameterReference::get();
    parameter.setDirty(true);
    memcpy_P(parameter.getDataPtr(), data, std::min(length, parameter.getSize()));
}

uint16_t ConfigurationParameterBinary::getSize() const {
    return ConfigurationParameterReference::get().getSize();
}
