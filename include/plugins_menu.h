/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "plugins.h"
#include <BootstrapMenu.h>

typedef struct {
    BootstrapMenu::menu_item_id_t home;
    BootstrapMenu::menu_item_id_t status;
    BootstrapMenu::menu_item_id_t config;
    BootstrapMenu::menu_item_id_t device;
    BootstrapMenu::menu_item_id_t admin;
    BootstrapMenu::menu_item_id_t util;
} NavMenu_t;

extern BootstrapMenu bootstrapMenu;
extern NavMenu_t navMenu;
