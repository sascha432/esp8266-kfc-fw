
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

PluginComponent *PluginComponent::findPlugin(NameType name)
{
    for(auto plugin: plugins) {
        if (plugin->nameEquals(name)) {
            return plugin;
        }
    }
    return nullptr;
}

bool PluginComponent::nameEquals(NameType name) const
{
    return strcmp_P_P(getName_P(), RFPSTR(name)) == 0;
}

bool PluginComponent::nameEquals(const char *name) const
{
    return strcmp_P_P(name, getName_P()) == 0;
}

bool PluginComponent::nameEquals(const String &name) const
{
    return strcmp_P(name.c_str(), getName_P()) == 0;
}

void PluginComponent::setup(SetupModeType mode)
{
}

void PluginComponent::shutdown()
{
}

#if ENABLE_DEEP_SLEEP

void PluginComponent::prepareDeepSleep(uint32_t sleepTimeMillis)
{
}

#endif

void PluginComponent::reconfigure(const String &source)
{
}

void PluginComponent::invokeReconfigure(const String &source)
{
    LoopFunctions::callOnce([this, source]() {
        invokeReconfigureNow(source);
    });
}

void PluginComponent::invokeReconfigureNow(const String &source)
{
    reconfigure(source);
    for(auto plugin: plugins) {
        if (plugin != this && plugin->hasReconfigureDependecy(source)) {
            plugin->reconfigure(String(source).c_str());
        }
    }
}

void PluginComponent::getStatus(Print &output)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
}

void PluginComponent::createConfigureForm(FormCallbackType type, const String &formName, Form &form, AsyncWebServerRequest *request)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
}

void PluginComponent::createMenu()
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
}

WebTemplate *PluginComponent::getWebTemplate(const String &formName)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
    return nullptr;
}

void PluginComponent::createWebUI(WebUI &webUI)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
}

void PluginComponent::getValues(JsonArray &array)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
}

void PluginComponent::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
}

#if AT_MODE_SUPPORTED

void PluginComponent::atModeHelpGenerator()
{
}

bool PluginComponent::atModeHandler(AtModeArgs &args)
{
    __debugbreak_and_panic_printf_P(SPGM(__pure_virtual), getName_P());
    return false;
}

#endif

PluginComponent *PluginComponent::getForm(const String &name)
{
    for(const auto plugin: plugins) {
        if (plugin->canHandleForm(name)) {
            _debug_printf_P(PSTR("form=%s plugin=%s\n"), name.c_str(), plugin->getName_P());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("form=%s result=null\n"), name.c_str());
    return nullptr;
}

PluginComponent *PluginComponent::getTemplate(const String &name)
{
    for(const auto plugin: plugins) {
        if (plugin->hasWebTemplate(name)) {
            _debug_printf_P(PSTR("template=%s plugin=%s\n"), name.c_str(), plugin->getName_P());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("template=%s result=null\n"), name.c_str());
    return nullptr;
}

PluginComponent *PluginComponent::getByName(NameType name)
{
    for(const auto plugin: plugins) {
        if (plugin->nameEquals(name)) {
            _debug_printf_P(PSTR("name=%s plugin=%s\n"), name, plugin->getName_P());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("name=%s result=null\n"), name);
    return nullptr;
}

PluginComponent *PluginComponent::getByMemoryId(RTCMemoryId memoryId)
{
    for(const auto plugin: plugins) {
        if (plugin->getOptions().memory_id == memoryId) {
            _debug_printf_P(PSTR("id=%u result=%s\n"), memoryId, plugin->getName_P());
            return plugin;
        }
    }
    _debug_printf_P(PSTR("id=%u result=null\n"), memoryId);
    return nullptr;
}
