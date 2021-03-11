/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "plugins.h"
#include <BootstrapMenu.h>

struct NavMenu {
    BootstrapMenu::menu_item_id_t home;
    BootstrapMenu::menu_item_id_t status;
    BootstrapMenu::menu_item_id_t config;
    BootstrapMenu::menu_item_id_t device;
    BootstrapMenu::menu_item_id_t admin;
    BootstrapMenu::menu_item_id_t util;
};

#define bootstrapMenu       PluginComponents::RegisterEx::getBootstrapMenu()
#define navMenu             PluginComponents::RegisterEx::getNavMenu()

namespace PluginComponents {

    class RegisterEx : public Register {
    public:
        using Register::Register;

        static RegisterEx &getInstance();
        static BootstrapMenu &getBootstrapMenu();
        static NavMenu &getNavMenu();

    private:
        friend Register;
        
        void _createMenu();
        BootstrapMenu &_getBootstrapMenu();
        NavMenu &_getNavMenu();

    private:
        BootstrapMenu _bootstrapMenu;
        NavMenu _navMenu;
    };

    inline __attribute__((__always_inline__))
    RegisterEx &RegisterEx::getInstance()
    {
        return *reinterpret_cast<RegisterEx *>(Register::getInstance());
    }

    inline __attribute__((__always_inline__))
    BootstrapMenu &RegisterEx::getBootstrapMenu()
    {
        return getInstance()._getBootstrapMenu();
    }

    inline __attribute__((__always_inline__))
    NavMenu &RegisterEx::getNavMenu()
    {
        return getInstance()._getNavMenu();
    }

    inline __attribute__((__always_inline__))
    BootstrapMenu &RegisterEx::_getBootstrapMenu()
    {
        return _bootstrapMenu;
    }

    inline __attribute__((__always_inline__))
    NavMenu &RegisterEx::_getNavMenu()
    {
        return _navMenu;
    }

}
