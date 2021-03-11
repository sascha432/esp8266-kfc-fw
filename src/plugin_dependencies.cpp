/**
  Author: sascha_lammers@gmx.de
*/

#include "PluginComponent.h"
#include "plugins_menu.h"

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

using namespace PluginComponents;

// ------------------------------------------------------------------------------------
// Dependency
// ------------------------------------------------------------------------------------

void Dependency::invoke(const PluginComponent *plugin) const
{
    __LDBG_printf("invoking callback name=%s plugin=%p source=%s", _name, plugin, _source->getName_P());
    __DBG_assert_printf(plugin, "plugin nullptr");
    __DBG_assert_printf(!!_callback, "invalid callback");
    auto &plugins = PluginComponents::RegisterEx::getPlugins();
    __DBG_assert_printf(std::find(plugins.begin(), plugins.end(), plugin) != plugins.end(), "plugin %p does not exists", plugin->getName_P());
    _callback(plugin);
}

// ------------------------------------------------------------------------------------
// Dependencies
// ------------------------------------------------------------------------------------

void PluginComponents::Dependencies::check()
{
    _dependencies.erase(std::remove_if(_dependencies.begin(), _dependencies.end(), [](const Dependency &dep) {
        auto plugin = PluginComponent::findPlugin(dep._name, true);
        if (plugin) {
            __LDBG_printf("dependency callback type=call_delayed name=%s source=%s", (PGM_P)dep._name, dep._source->getName_P());
            dep.invoke(plugin);
            return true;
        }
        return false;
    }), _dependencies.end());

}

void PluginComponents::Dependencies::cleanup()
{
}

void PluginComponents::Dependencies::destroy()
{
    for(const auto &dep: _dependencies) {
        __DBG_printf("unresolved dependency name=%s source=%p", dep._name, dep._source->getName_P());
    }
}

// void PluginComponent::invokeDependencies(NameType name, const PluginComponent *plugin)
// {
//     if (!_dependencies) {
//         return;
//     }
//     _dependencies->erase(std::remove_if(_dependencies->begin(), _dependencies->end(), [name, plugin](const Dependency &dep) {
//         if (dep == name) {
//             __LDBG_printf("dependency callback type=previously delayed name=%s plugin=%p source=%s", (PGM_P)name, plugin, dep._source.getName_P());
//             dep._callback(plugin);
//             return true;
//         }
//         return false;
//     }), _dependencies->end());
// }


bool PluginComponents::Dependencies::dependsOn(NameType name, DependencyCallback callback, const PluginComponent *source)
{
    auto plugin = PluginComponent::findPlugin(name, false);
    if (plugin) {
        if (plugin->_setupTime) {
            // check if there is any dependencies left
            check();
            __LDBG_printf("dependency callback type=call name=%s source=%s", (PGM_P)name, source->getName_P());
            callback(plugin);
        }
        else {
            // create delayed dendency
            __LDBG_printf("dependency callback type=delayed name=%s source=%s", (PGM_P)name, source->getName_P());
            _dependencies.emplace_back(name, callback, source);
        }
        return true;
    }
    __LDBG_printf("dependency cannot be resolved, plugin=%s not found", name);
    return false;
}
