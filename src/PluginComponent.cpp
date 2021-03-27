/**
  Author: sascha_lammers@gmx.de
*/

#if AT_MODE_SUPPORTED
#include "at_mode.h"
#endif
#include "PluginComponent.h"
#include "plugins_menu.h"
#include "plugins.h"
#include <ESPAsyncWebServer.h>
#include <LoopFunctions.h>
#include <Form.h>
#include <misc.h>
#include <kfc_fw_config.h>
#include <ReadADC.h>
#include <KFCForms.h>
#if MQTT_SUPPORT
#include "../src/plugins/mqtt/mqtt_strings.h"
#endif

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

// FLASH_STRING_GENERATOR_AUTO_INIT(
//     AUTO_STRING_DEF(__pure_virtual, "pure virtual call: %s\n")
// );

#define __DBG_panic_pure_virtual() \
    DEBUG_OUTPUT.printf_P(SPGM(__pure_virtual), getName_P()); \
    __debugbreak_and_panic()

PluginComponent *PluginComponent::findPlugin(NameType name, bool isSetup)
{
    for(const auto plugin: PluginComponents::Register::getPlugins()) {
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

void PluginComponent::preSetup(SetupModeType mode)
{
}

void PluginComponent::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
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
    for(auto plugin: PluginComponents::Register::getPlugins()) {
        if (plugin != this && plugin->hasReconfigureDependecy(source)) {
            plugin->reconfigure(source);
        }
    }
}

void PluginComponent::getStatus(Print &output)
{
}

void PluginComponent::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    __DBG_panic_pure_virtual();
}

void PluginComponent::createMenu()
{
    __DBG_panic_pure_virtual();
}

WebTemplate *PluginComponent::getWebTemplate(const String &formName)
{
    __DBG_panic_pure_virtual();
    return nullptr;
}

void PluginComponent::createWebUI(WebUINS::Root &webUI)
{
    __DBG_panic_pure_virtual();
}

void PluginComponent::getValues(NamedArray &array)
{
    __DBG_panic_pure_virtual();
}

void PluginComponent::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __DBG_panic_pure_virtual();
}

#if AT_MODE_SUPPORTED

ATModeCommandHelpArrayPtr PluginComponent::atModeCommandHelp(size_t &size) const
{
    size = 0;
    return nullptr;
}

void PluginComponent::atModeHelpGenerator()
{
    if (isEnabled()) {
        size_t size;
        auto help = atModeCommandHelp(size);
        if (help) {
            for(size_t i = 0; i < size; i++) {
                at_mode_add_help(help[i], getName_P());
            }
        }
    }
}

bool PluginComponent::atModeHandler(AtModeArgs &args)
{
    __DBG_panic_pure_virtual();
    return false;
}

#endif

PluginComponent *PluginComponent::getForm(const String &name)
{
    if (name.length() == 0) {
        return nullptr;
    }
    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        if (plugin->canHandleForm(name)) {
            __LDBG_printf("form=%s plugin=%s", name.c_str(), plugin->getName_P());
            return plugin;
        }
    }
    __LDBG_printf("form=%s result=null", name.c_str());
    return nullptr;
}

PluginComponent *PluginComponent::getTemplate(const String &name)
{
    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        if (plugin->hasWebTemplate(name)) {
            __LDBG_printf("template=%s plugin=%s", name.c_str(), plugin->getName_P());
            return plugin;
        }
    }
    __LDBG_printf("template=%s result=null", name.c_str());
    return nullptr;
}

PluginComponent *PluginComponent::getByName(NameType name)
{
    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        if (plugin->nameEquals(name)) {
            __LDBG_printf("name=%s plugin=%s", name, plugin->getName_P());
            return plugin;
        }
    }
    __LDBG_printf("name=%s result=null", name);
    return nullptr;
}

PluginComponent *PluginComponent::getByMemoryId(RTCMemoryId memoryId)
{
    for(const auto plugin: PluginComponents::Register::getPlugins()) {
        if (plugin->getOptions().memory_id == memoryId) {
            __LDBG_printf("id=%u result=%s", memoryId, plugin->getName_P());
            return plugin;
        }
    }
    __LDBG_printf("id=%u result=null", memoryId);
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
                _blacklist.trim(',');
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
