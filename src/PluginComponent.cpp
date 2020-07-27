/**
  Author: sascha_lammers@gmx.de
*/

#include "PluginComponent.h"
#include "plugins.h"
#include <ESPAsyncWebServer.h>
#include <LoopFunctions.h>
#include <Form.h>
#include <misc.h>
#include "kfc_fw_config_classes.h"

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

#if AT_MODE_SUPPORTED
#include "at_mode.h"
#endif

PluginComponent::DependencyVector *PluginComponent::_dependencies;

PROGMEM_STRING_DEF(__pure_virtual, "pure virtual call: %s\n");

PluginComponent *PluginComponent::findPlugin(NameType name, bool isSetup)
{
    for(const auto plugin: plugins) {
        if (plugin->nameEquals(name) && (!isSetup || plugin->_setupTime)) {
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
//     StringVector deps;
//     deps.push_back(source);
//     size_t size;
//     do {
//         size = deps.size();
//         __DBG_printf("resolve deps size=%u", size);
//         for(const auto &name: deps) {
//             for(const auto plugin: plugins) {
//                 if (plugin != this && plugin->hasReconfigureDependecy(name)) {
//                     String name = plugin->getName();
//                     if (std::find(deps.begin(), deps.end(), name) == deps.end()) {
//                         deps.emplace_back(std::move(name));
//                     }
//                 }
//             }
//         }
//         __DBG_printf("1 deps: %s", implode(',', deps).c_str());
//         xtra_containers::remove_duplicates(deps);
//         __DBG_printf("2 deps: %s", implode(',', deps).c_str());
//     } while(size != deps.size())

//     debug_printf_P(PSTR("resolved deps: %s\n"), implode(',', deps).c_str());

//     for(auto iterator = plugins.rbegin(); iterator != plugins.rend(); ++iterator) {
//         const auto plugin = *iterator;
//         if (plugin != this) {
//             for(const auto &name: deps) {
//                 if (plugin->hasReconfigureDependecy(name)) {
//                     //plugin->reconfigure(name);
//                     __DBG_printf("reconfigure name=%s source=%s", plugin->getName_P(), name.c_str());
//                 }
//             }
//         }
//     }

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

bool PluginComponent::dependsOn(NameType name, DependencyCallback callback)
{
    auto plugin = findPlugin(name, false);
    if (plugin) {
        if (plugin->_setupTime) {
            _debug_printf_P(PSTR("dependecy callback type=call name=%s callback=%p\n"), (PGM_P)name, &callback);
            callback(plugin);
        }
        else {
            _debug_printf_P(PSTR("dependecy callback type=delayed name=%s callback=%p\n"), (PGM_P)name, &callback);
            _dependencies->emplace_back(name, callback);
        }
        return true;
    }
    return false;
}

void PluginComponent::checkDependencies()
{
    _dependencies->erase(std::remove_if(_dependencies->begin(), _dependencies->end(), [](const Dependency &dep) {
        auto plugin = findPlugin(dep._name, true);
        if (plugin) {
            _debug_printf_P(PSTR("dependecy callback type=call_delayed name=%s callback=%p\n"), (PGM_P)dep._name, &dep._callback);
            dep._callback(plugin);
            return true;
        }
        return false;
    }), _dependencies->end());
}

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


uint32_t PluginComponent::getSetupTime() const
{
    return _setupTime;
}

void PluginComponent::setSetupTime()
{
    _setupTime = millis();
}

void PluginComponent::clearSetupTime()
{
    _setupTime = 0;
}

using Firmware = KFCConfigurationClasses::System::Firmware;

bool PluginComponent::addToBlacklist(const String &name)
{
    String _blacklist = getBlacklist();
    if (_blacklist.length()) {
        if (stringlist_find_P_P(_blacklist.c_str(), name.c_str(), ',') != -1) {
            // already on the list
            return false;
        }
        // we already have items on the list
        _blacklist += ',';
    }
    _blacklist += name;
    Firmware::setPluginBlacklist(_blacklist.c_str());
    return true;
}

bool PluginComponent::removeFromBlacklist(const String &name)
{
    String _blacklist = getBlacklist();
    auto len = _blacklist.length();
    if (len < name.length() || stringlist_find_P_P(_blacklist.c_str(), name.c_str(), ',') == -1) {
        return false;
    }
    if (_blacklist.equals(name)) {
        _blacklist = String();
    }
    else if (_blacklist.length() > name.length()) {
        // first item
        if (_blacklist.charAt(name.length()) == ',' && _blacklist.startsWith(name)) {
            _blacklist.remove(0, name.length() + 1);
        }
        else {
            String tmp = String(',');
            tmp += name;
            // last item
            if (_blacklist.endsWith(tmp)) {
                _blacklist.remove(_blacklist.length() - tmp.length());
            }
            else {
                // somewhere in the middle
                tmp += ',';
                _blacklist.replace(tmp, String(','));
                String_trim(_blacklist, ',');
            }
        }
    }
    auto result = _blacklist.length() != len;
    if (result) {
        Firmware::setPluginBlacklist(_blacklist.c_str());
    }
    return result;
}

const char *PluginComponent::getBlacklist()
{
    return Firmware::getPluginBlacklist();
}

