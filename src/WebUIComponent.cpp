/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUIComponent.h"
#include "plugins_menu.h"
#include "plugins.h"

#if DEBUG_WEBUI
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

/*

translation of shortcuts for webui.js

    short: function(from) {
        var to = {};
        for(var key in from) {
            to[from[key]] = key;
        }
        return {from: from, to: to};
    } ({
        bs: 'binary_sensor',
        bg: 'button_group',
        c: 'columns',
        hs: 'has_switch',
        // rmin: 'range_min',
        // rmax: 'range_max',
        rt: 'render_type',
        ue: 'update_events',
        zo: 'zero_off',
        // ht: 'heading_top',
        // hb: 'heading_bottom'
    }),


*/

WEBUI_PROGMEM_STRING_DEF(align)
WEBUI_PROGMEM_STRING_DEF(badge)
WEBUI_PROGMEM_STRING_DEFVAL(binary_sensor, "bs")
WEBUI_PROGMEM_STRING_DEFVAL(button_group, "bg")
WEBUI_PROGMEM_STRING_DEF(center)
WEBUI_PROGMEM_STRING_DEF(color)
WEBUI_PROGMEM_STRING_DEFVAL(columns, "c")
WEBUI_PROGMEM_STRING_DEF(events)
WEBUI_PROGMEM_STRING_DEF(data)
WEBUI_PROGMEM_STRING_DEF(group)
WEBUI_PROGMEM_STRING_DEFVAL(has_switch, "hs")
WEBUI_PROGMEM_STRING_DEF(head)
WEBUI_PROGMEM_STRING_DEF(height)
WEBUI_PROGMEM_STRING_DEF(id)
WEBUI_PROGMEM_STRING_DEF(items)
WEBUI_PROGMEM_STRING_DEF(left)
WEBUI_PROGMEM_STRING_DEF(listbox)
WEBUI_PROGMEM_STRING_DEF(max)
WEBUI_PROGMEM_STRING_DEF(medium)
WEBUI_PROGMEM_STRING_DEF(min)
WEBUI_PROGMEM_STRING_DEFVAL(range_min, "rmin")
WEBUI_PROGMEM_STRING_DEFVAL(range_max, "rmax")
WEBUI_PROGMEM_STRING_DEF(multi)
WEBUI_PROGMEM_STRING_DEF(offset)
WEBUI_PROGMEM_STRING_DEFVAL(render_type, "rt")
WEBUI_PROGMEM_STRING_DEF(right)
WEBUI_PROGMEM_STRING_DEF(rgb)
WEBUI_PROGMEM_STRING_DEF(row)
WEBUI_PROGMEM_STRING_DEF(screen)
WEBUI_PROGMEM_STRING_DEF(sensor)
WEBUI_PROGMEM_STRING_DEF(slider)
WEBUI_PROGMEM_STRING_DEF(state)
WEBUI_PROGMEM_STRING_DEF(switch)
WEBUI_PROGMEM_STRING_DEF(temp)
WEBUI_PROGMEM_STRING_DEF(title)
WEBUI_PROGMEM_STRING_DEF(top)
WEBUI_PROGMEM_STRING_DEF(type)
WEBUI_PROGMEM_STRING_DEF(unit)
WEBUI_PROGMEM_STRING_DEFVAL(update_events, "ue")
WEBUI_PROGMEM_STRING_DEF(value)
WEBUI_PROGMEM_STRING_DEF(vcc)
WEBUI_PROGMEM_STRING_DEF(width)
WEBUI_PROGMEM_STRING_DEFVAL(zero_off, "zo")
WEBUI_PROGMEM_STRING_DEF(name)
WEBUI_PROGMEM_STRING_DEFVAL(heading_top, "ht")
WEBUI_PROGMEM_STRING_DEFVAL(heading_bottom, "hb")

namespace WebUINS {

    void Root::addValues()
    {
        NamedArray values(F("values"));
#ifndef _MSC_VER
        for(auto plugin: PluginComponents::Register::getPlugins()) {
            __LDBG_printf("plugin=%s webui=%u", plugin->getName_P(), plugin->hasWebUI());
            if (plugin->hasWebUI()) {
                plugin->getValues(values);
            }
        }
#endif
        __LDBG_printf("append values length=%u json=%s", values.length(), values.toString().c_str());
        _json.append(values);
    }

}
