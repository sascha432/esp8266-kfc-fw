
/**
  Author: sascha_lammers@gmx.de
*/

#include "PluginComponent.h"
#include "plugins.h"
#include <ESPAsyncWebServer.h>
#include <LoopFunctions.h>
#include <Form.h>
#include <misc.h>

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

#if AT_MODE_SUPPORTED
#include "at_mode.h"
#endif

PROGMEM_STRING_DEF(__pure_virtual, "pure virtual call: %s\n");

PluginComponent *PluginComponent::findPlugin(const __FlashStringHelper *name)
{
    for(auto plugin: plugins) {
        if (plugin->nameEquals(name)) {
            return plugin;
        }
    }
    return nullptr;
}

const __FlashStringHelper *PluginComponent::getFriendlyName() const
{
    return FPSTR(getName());
}

bool PluginComponent::nameEquals(const __FlashStringHelper *name) const
{
    return strcmp_P_P(getName(), RFPSTR(name)) == 0;
}

bool PluginComponent::nameEquals(const char *name) const
{
    return strcmp_P(name, getName()) == 0;
}

bool PluginComponent::nameEquals(const String &name) const
{
    return strcmp_P(name.c_str(), getName()) == 0;
}

PluginComponent::PluginPriorityEnum_t PluginComponent::getSetupPriority() const
{
    return DEFAULT_PRIORITY;
}

uint8_t PluginComponent::getRtcMemoryId() const
{
    return 0;
}

bool PluginComponent::allowSafeMode() const
{
    return false;
}

bool PluginComponent::autoSetupAfterDeepSleep() const
{
    return false;
}


void PluginComponent::setup(PluginSetupMode_t mode) {
}

void PluginComponent::reconfigure(PGM_P source) {
}

bool PluginComponent::hasReconfigureDependecy(PluginComponent *plugin) const {
    return false;
}

void PluginComponent::invokeReconfigure(PGM_P source)
{
    LoopFunctions::callOnce([this, source]() {
        this->invokeReconfigureNow(source);
    });
}

void PluginComponent::invokeReconfigureNow(PGM_P source)
{
    reconfigure(source);
    for(auto plugin: plugins) {
        if (plugin != this && plugin->hasReconfigureDependecy(plugin)) {
            plugin->reconfigure(source);
        }
    }
}


bool PluginComponent::hasStatus() const
{
    return false;
}

void PluginComponent::getStatus(Print &output)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName());
}


PGM_P PluginComponent::getConfigureForm() const
{
    return nullptr;
}

bool PluginComponent::canHandleForm(const String &formName) const
{
    if (!getConfigureForm()) {
        return false;
    }
    return strcmp_P(formName.c_str(), getConfigureForm()) == 0;
}

void PluginComponent::createConfigureForm(AsyncWebServerRequest *request, Form &form)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName());
}

PluginComponent::MenuTypeEnum_t PluginComponent::getMenuType() const
{
    return AUTO;
}

void PluginComponent::createMenu()
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName());
}


bool PluginComponent::hasWebTemplate(const String &formName) const
{
    return false;
}

WebTemplate *PluginComponent::getWebTemplate(const String &formName)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName());
    return nullptr;
}

bool PluginComponent::hasWebUI() const
{
    return false;
}

void PluginComponent::createWebUI(WebUI &webUI)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName());
}

WebUIInterface *PluginComponent::getWebUIInterface()
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName());
    return nullptr;
}

void PluginComponent::prepareDeepSleep(uint32_t sleepTimeMillis)
{
}

#if AT_MODE_SUPPORTED

bool PluginComponent::hasAtMode() const
{
    return false;
}

void PluginComponent::atModeHelpGenerator() {
}

bool PluginComponent::atModeHandler(AtModeArgs &args)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName());
    return false;
}

#endif

void PluginComponent::restart()
{
}

PluginComponent *PluginComponent::getForm(const String &formName)
{
    for(auto plugin: plugins) {
        if (plugin->canHandleForm(formName)) {
            _debug_printf_P(PSTR("form=%s result=%s\n"), formName.c_str(), plugin->getName());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("form=%s result=null\n"), formName.c_str());
    return nullptr;
}

PluginComponent *PluginComponent::getTemplate(const String &formName)
{
    for(auto plugin: plugins) {
        if (plugin->hasWebTemplate(formName)) {
            _debug_printf_P(PSTR("template=%s result=%s\n"), formName.c_str(), plugin->getName());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("template=%s result=null\n"), formName.c_str());
    return nullptr;
}

PluginComponent *PluginComponent::getByName(PGM_P name)
{
    for(auto plugin: plugins) {
        if (plugin->nameEquals(FPSTR(name))) {
            _debug_printf_P(PSTR("name=%s result=%s\n"), name, plugin->getName());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("name=%s result=null\n"), name);
    return nullptr;
}

PluginComponent *PluginComponent::getByMemoryId(uint8_t memoryId)
{
    for(auto plugin: plugins) {
        if (plugin->getRtcMemoryId() == memoryId) {
            _debug_printf_P(PSTR("id=%u result=%s\n"), memoryId, plugin->getName());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("id=%u result=null\n"), memoryId);
    return nullptr;
}
