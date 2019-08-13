
/**
  Author: sascha_lammers@gmx.de
*/

#include "PluginComponent.h"
#include "plugins.h"
#include <ESPAsyncWebServer.h>
#include <Form.h>
#include <misc.h>

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

bool PluginComponent::nameEquals(const __FlashStringHelper *name) {
    return strcmp_P_P(getName(), reinterpret_cast<PGM_P>(name)) == 0;
}
bool PluginComponent::nameEquals(const char *name) {
    return strcmp_P(name, getName()) == 0;
}
bool PluginComponent::nameEquals(const String &name) {
    return strcmp_P(name.c_str(), getName()) == 0;
}

PluginComponent::PluginPriorityEnum_t PluginComponent::getSetupPriority() const {
    return DEFAULT_PRIORITY;
}

uint8_t PluginComponent::getRtcMemoryId() const {
    return 0;
}

bool PluginComponent::allowSafeMode() const {
    return false;
}

bool PluginComponent::autoSetupAfterDeepSleep() const {
    return false;
}


void PluginComponent::setup(PluginSetupMode_t mode) {
}

void PluginComponent::reconfigure(PGM_P source) {
}

bool PluginComponent::hasReconfigureDependecy(PluginComponent *plugin) const {
    return false;
}

void PluginComponent::invokeReconfigure(PGM_P source) {
    reconfigure(source);
    for(auto plugin: plugins) {
        if (plugin != this && plugin->hasReconfigureDependecy(plugin)) {
            plugin->reconfigure(source);
        }
    }
}


bool PluginComponent::hasStatus() const {
    return false;
}

const String PluginComponent::getStatus() {
    return _sharedEmptyString;
}


bool PluginComponent::canHandleForm(const String &formName) const {
    return false;
}

void PluginComponent::createConfigureForm(AsyncWebServerRequest *request, Form &form) {
}

void PluginComponent::prepareDeepSleep(uint32_t sleepTimeMillis) {
}


bool PluginComponent::hasAtMode() const {
    return false;
}

void PluginComponent::atModeHelpGenerator() {
}

bool PluginComponent::atModeHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
    return false;
}


PluginComponent *PluginComponent::getForm(const String &formName) {
    for(auto plugin: plugins) {
        if (plugin->canHandleForm(formName)) {
            return plugin;
        }
    }
    return nullptr;
}

PluginComponent *PluginComponent::getByName(PGM_P name) {
    for(auto plugin: plugins) {
        if (plugin->nameEquals(FPSTR(name))) {
            _debug_printf_P(PSTR("PluginComponent::getByName(%s) = %p\n"), name, plugin);
            return plugin;
        }
    }
    _debug_printf_P(PSTR("PluginComponent::getByName(%s) = nullptr\n"), name);
    return nullptr;
}

PluginComponent *PluginComponent::getByMemoryId(uint8_t memoryId) {
    for(auto plugin: plugins) {
        if (plugin->getRtcMemoryId() == memoryId) {
            _debug_printf_P(PSTR("PluginComponent::getByMemoryId(%u) = %p\n"), memoryId, plugin);
            return plugin;
        }
    }
    _debug_printf_P(PSTR("PluginComponent::getByMemoryId(%u) = nullptr\n"), memoryId);
    return nullptr;
}
