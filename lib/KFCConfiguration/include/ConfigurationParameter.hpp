/**
* Author: sascha_lammers@gmx.de
*/

#pragma once

#include "ConfigurationParameter.h"

inline uint16_t ConfigurationParameter::read(Configuration &conf, uint16_t offset)
{
    if (!_readData(conf, offset)) {
        return 0;
    }
    return _param.length();
}

inline Configuration::ParameterList::iterator Configuration::_findParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset)
{
    offset = getDataOffset(_params.size());
    for (auto it = _params.begin(); it != _params.end(); ++it) {
        if (*it == handle && ((type == ParameterType::_ANY) || (it->_param.type() == type))) {
        // if (*it == handle && (it->_param.type() == type)) {
            //__LDBG_printf("%s FOUND", it->toString().c_str());
            // it->_getParam()._usage._counter++;
            return it;
        }
        offset += it->_param.next_offset();
    }
    __LDBG_printf("handle=%s[%04x] type=%s = NOT FOUND", ConfigurationHelper::getHandleName(handle), handle, (const char *)ConfigurationParameter::getTypeString(type));
    return _params.end();
}


inline ConfigurationParameter &Configuration::_getOrCreateParam(ConfigurationParameter::TypeEnum_t type, HandleType handle, uint16_t &offset)
{
    #if DEBUG_CONFIGURATION
        delay(1);
    #endif
    auto iterator = _findParam(ParameterType::_ANY, handle, offset);
    if (iterator == _params.end()) {
        _params.emplace_back(handle, type);
        __LDBG_printf("new param %s", _params.back().toString().c_str());
        __DBG__checkIfHandleExists("create", handle);
        return _params.back();
    }
    else if (type != iterator->_param.type()) {
        __DBG_panic("%s: new_type=%s type=%s different", ConfigurationHelper::getHandleName(iterator->getHandle()), iterator->toString().c_str(), (const char *)ConfigurationParameter::getTypeString(type));
    }
    __DBG__checkIfHandleExists("find", iterator->getHandle());
    return *iterator;
}

inline const char *ConfigurationParameter::getString(Configuration &conf, uint16_t offset)
{
    if (_param.size() == 0) {
        return emptyString.c_str();
    }
    if (!_readData(conf, offset)) {
        return emptyString.c_str();
    }
    //__LDBG_printf("%s %s", toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    return _param.string();
}

inline const uint8_t *ConfigurationParameter::getBinary(Configuration &conf, size_type &length, uint16_t offset)
{
    if (_param.size() == 0) {
        length = 0;
        return nullptr;
    }
    if (!_readData(conf, offset)) {
        length = 0;
        return nullptr;
    }
    //__LDBG_printf("%s %s", toString().c_str(), Configuration::__debugDumper(*this, _info.data, _info.size).c_str());
    length = _param.length();
    return _param.data();
}

#if ESP8266

inline bool ConfigurationParameter::_readDataTo(Configuration &conf, uint16_t offset, uint8_t *ptr) const
{
    // nothing to read
    if (_param.length() == 0) {
        return true;
    }
    return conf.flashRead(ConfigurationHelper::getFlashAddress(offset), ptr, _param.length());
}

#endif
