/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintHtmlEntities.h>
#include "progmem_data.h"

PROGMEM_STRING_DEF(default_password_warning, "WARNING! Default password has not been changed");
PROGMEM_STRING_DEF(empty, "");
PROGMEM_STRING_DEF(0, "0");
PROGMEM_STRING_DEF(1, "1");
PROGMEM_STRING_DEF(OK, "OK");
PROGMEM_STRING_DEF(Enabled, "Enabled");
PROGMEM_STRING_DEF(Disabled, "Disabled");
PROGMEM_STRING_DEF(application_json, "application/json");
PROGMEM_STRING_DEF(text_plain, "text/plain");
PROGMEM_STRING_DEF(text_html, "text/html");
PROGMEM_STRING_DEF(auto_discovery_html, HTML_S(tr) HTML_S(td) "%s" HTML_E(td) HTML_S(td) "%s" HTML_E(td) HTML_E(tr));
PROGMEM_STRING_DEF(slash, "/");
PROGMEM_STRING_DEF(dot, ".");
PROGMEM_STRING_DEF(comma, ",");
PROGMEM_STRING_DEF(slash_dot, "/.");
PROGMEM_STRING_DEF(filename, "filename");
PROGMEM_STRING_DEF(dir, "dir");
PROGMEM_STRING_DEF(SPIFFS_tmp_dir, "/tmp/");
PROGMEM_STRING_DEF(SID, "SID");
PROGMEM_STRING_DEF(login_html, "/login.html");
PROGMEM_STRING_DEF(Accept_Encoding, "Accept-Encoding");
PROGMEM_STRING_DEF(server_crt, "/server.crt");
PROGMEM_STRING_DEF(server_key, "/server.key");
PROGMEM_STRING_DEF(kfcfw, "kfcfw");
PROGMEM_STRING_DEF(Failed_to_reserve_string_size, "Failed to reserve string size %d\n");
PROGMEM_STRING_DEF(invalid_flash_ptr, "INVALID_FLASH_PTR");

// const char _shared_progmem_string_webui_mappings[] PROGMEM = "/webui/.mappings";

String spgm_concat(PGM_P str1, PGM_P str2) {
    size_t len = strlen_P(str1) + strlen_P(str2) + 1;
    String tmp;

    if (!tmp.reserve(len)) {
        debug_printf_P(SPGM(Failed_to_reserve_string_size), len);
    }
    tmp = str1;
    tmp += str2;
    return tmp;
}

String spgm_concat(PGM_P str1, int i) {
    size_t len = strlen_P(str1) + 7;
    String tmp;

    if (!tmp.reserve(len)) {
        debug_printf_P(SPGM(Failed_to_reserve_string_size), len);
    }
    tmp = str1;
    tmp += String(i);
    return tmp;
}

String spgm_concat(PGM_P str1, char c) {
    size_t len = strlen_P(str1) + 2;
    String tmp;

    if (!tmp.reserve(len)) {
        debug_printf_P(SPGM(Failed_to_reserve_string_size), len);
    }
    tmp = str1;
    tmp += c;
    return tmp;
}
