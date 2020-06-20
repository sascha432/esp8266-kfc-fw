/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintHtmlEntities.h>
#include "progmem_data.h"

PROGMEM_STRING_DEF(mime_text_html, "text/html");
PROGMEM_STRING_DEF(mime_text_xml, "text/xml");
PROGMEM_STRING_DEF(mime_text_plain, "text/plain");
PROGMEM_STRING_DEF(mime_text_css, "text/css");
PROGMEM_STRING_DEF(mime_application_javascript, "application/javascript");
PROGMEM_STRING_DEF(mime_application_json, "application/json");
PROGMEM_STRING_DEF(mime_application_zip, "application/zip");
PROGMEM_STRING_DEF(mime_application_x_gzip, "application/x-gzip");
PROGMEM_STRING_DEF(mime_image_jpeg, "image/jpeg");
PROGMEM_STRING_DEF(mime_image_png, "image/png");
PROGMEM_STRING_DEF(mime_image_gif, "image/gif");
PROGMEM_STRING_DEF(mime_image_bmp, "image/bmp");
PROGMEM_STRING_DEF(_hidden, " hidden");
PROGMEM_STRING_DEF(_selected, " selected");
PROGMEM_STRING_DEF(_checked, " checked");
PROGMEM_STRING_DEF(status, "status");
PROGMEM_STRING_DEF(Device_is_rebooting, "Device is rebooting...\n");
PROGMEM_STRING_DEF(Not_supported, "Not supported");
PROGMEM_STRING_DEF(default_password_warning, "WARNING! Default password has not been changed");
PROGMEM_STRING_DEF(empty, "");
PROGMEM_STRING_DEF(0, "0");
PROGMEM_STRING_DEF(1, "1");
PROGMEM_STRING_DEF(On, "On");
PROGMEM_STRING_DEF(Off, "Off");
PROGMEM_STRING_DEF(on, "on");
PROGMEM_STRING_DEF(off, "off");
PROGMEM_STRING_DEF(OK, "OK");
PROGMEM_STRING_DEF(Yes, "Yes");
PROGMEM_STRING_DEF(No, "No");
PROGMEM_STRING_DEF(yes, "yes");
PROGMEM_STRING_DEF(no, "no");
PROGMEM_STRING_DEF(hidden, "hidden");
PROGMEM_STRING_DEF(enabled, "enabled");
PROGMEM_STRING_DEF(disabled, "disabled");
PROGMEM_STRING_DEF(Enabled, "Enabled");
PROGMEM_STRING_DEF(Disabled, "Disabled");
PROGMEM_STRING_DEF(application_json, "application/json");
PROGMEM_STRING_DEF(auto_discovery_html, HTML_S(tr) HTML_S(td) "%s" HTML_E(td) HTML_S(td) "%s" HTML_E(td) HTML_E(tr));
PROGMEM_STRING_DEF(slash, "/");
PROGMEM_STRING_DEF(dot, ".");
PROGMEM_STRING_DEF(comma, ",");
PROGMEM_STRING_DEF(http, "http");
PROGMEM_STRING_DEF(https, "https");
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
PROGMEM_STRING_DEF(crash_counter_file, "/crash_counter");
PROGMEM_STRING_DEF(crash_dump_file, "/crash.%03x");
PROGMEM_STRING_DEF(tcp, "tcp");
PROGMEM_STRING_DEF(udp, "udp");
PROGMEM_STRING_DEF(KFC_Firmware, "KFC Firmware");
