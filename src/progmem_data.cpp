/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintHtmlEntities.h>
#include "progmem_data.h"

const char _shared_progmem_string_empty[] PROGMEM = "";
const char _shared_progmem_string_0[] PROGMEM = "0";
const char _shared_progmem_string_1[] PROGMEM = "1";
const char _shared_progmem_string_OK[] PROGMEM = "OK";
const char _shared_progmem_string_Enabled[] PROGMEM = "Enabled";
const char _shared_progmem_string_Disabled[] PROGMEM = "Disabled";
const char _shared_progmem_string_application_json[] PROGMEM = "application/json";
const char _shared_progmem_string_text_plain[] PROGMEM = "text/plain";
const char _shared_progmem_string_text_html[] PROGMEM = "text/html";
const char _shared_progmem_string_auto_discovery_html[] PROGMEM = HTML_S(tr) HTML_S(td) "%s" HTML_E(td) HTML_S(td) "%s" HTML_E(td) HTML_E(tr);
const char _shared_progmem_string_slash[] PROGMEM = "/";
const char _shared_progmem_string_dot[] PROGMEM = ".";
const char _shared_progmem_string_comma[] PROGMEM = ",";
const char _shared_progmem_string_slash_dot[] PROGMEM = "/.";
const char _shared_progmem_string_filename[] PROGMEM = "filename";
const char _shared_progmem_string_dir[] PROGMEM = "dir";
const char _shared_progmem_string_SPIFFS_tmp_dir[] PROGMEM = "/tmp/";
const char _shared_progmem_string_SID[] PROGMEM = "SID";
const char _shared_progmem_string_login_html[] PROGMEM = "/login.html";
// const char _shared_progmem_string_webui_mappings[] PROGMEM = "/webui/.mappings";
const char _shared_progmem_string_Accept_Encoding[] PROGMEM = "Accept-Encoding";
const char _shared_progmem_string_server_crt[] PROGMEM = "/server.crt";
const char _shared_progmem_string_server_key[] PROGMEM = "/server.key";
const char _shared_progmem_string_kfcfw[] PROGMEM = "kfcfw";

const char _shared_progmem_string_Failed_to_reserve_string_size[] PROGMEM = "Failed to reserve string size %d\n";
const char _shared_progmem_string_invalid_flash_ptr[] PROGMEM = "INVALID_FLASH_PTR";



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
