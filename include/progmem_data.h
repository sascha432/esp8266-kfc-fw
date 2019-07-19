/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

extern const char _shared_progmem_string_empty[] PROGMEM;
extern const char _shared_progmem_string__checked[] PROGMEM;
extern const char _shared_progmem_string__selected[] PROGMEM;
extern const char _shared_progmem_string__hidden[] PROGMEM;
extern const char _shared_progmem_string_0[] PROGMEM;
extern const char _shared_progmem_string_1[] PROGMEM;
extern const char _shared_progmem_string_OK[] PROGMEM;
extern const char _shared_progmem_string_Not_supported[] PROGMEM;
extern const char _shared_progmem_string_enabled[] PROGMEM;
extern const char _shared_progmem_string_disabled[] PROGMEM;
extern const char _shared_progmem_string_Enabled[] PROGMEM;
extern const char _shared_progmem_string_Disabled[] PROGMEM;
extern const char _shared_progmem_string_auto_discovery_html[] PROGMEM;
extern const char _shared_progmem_string_application_json[] PROGMEM;
extern const char _shared_progmem_string_text_plain[] PROGMEM;
extern const char _shared_progmem_string_text_html[] PROGMEM;
extern const char _shared_progmem_string_slash[] PROGMEM;
extern const char _shared_progmem_string_dot[] PROGMEM;
extern const char _shared_progmem_string_comma[] PROGMEM;
extern const char _shared_progmem_string_comma_[] PROGMEM;
extern const char _shared_progmem_string_slash_dot[] PROGMEM;
extern const char _shared_progmem_string_filename[] PROGMEM;
extern const char _shared_progmem_string_dir[] PROGMEM;
extern const char _shared_progmem_string_Location[] PROGMEM;
extern const char _shared_progmem_string_Link[] PROGMEM;
extern const char _shared_progmem_string_Pragma[] PROGMEM;
extern const char _shared_progmem_string_Cache_Control[] PROGMEM;
extern const char _shared_progmem_string_Content_Length[] PROGMEM;
extern const char _shared_progmem_string_Content_Encoding[] PROGMEM;
extern const char _shared_progmem_string_Connection[] PROGMEM;
extern const char _shared_progmem_string_Cookie[] PROGMEM;
extern const char _shared_progmem_string_Set_Cookie[] PROGMEM;
extern const char _shared_progmem_string_Last_Modified[] PROGMEM;
extern const char _shared_progmem_string_Expires[] PROGMEM;
extern const char _shared_progmem_string_no_cache[] PROGMEM;
extern const char _shared_progmem_string_close[] PROGMEM;
extern const char _shared_progmem_string_keep_alive[] PROGMEM;
extern const char _shared_progmem_string_RFC7231_date[] PROGMEM;
extern const char _shared_progmem_string_SPIFFS_tmp_dir[] PROGMEM;
extern const char _shared_progmem_string_SID[] PROGMEM;
extern const char _shared_progmem_string_login_html[] PROGMEM;
// extern const char _shared_progmem_string_webui_mappings[] PROGMEM;
extern const char _shared_progmem_string_Accept_Encoding[] PROGMEM;
extern const char _shared_progmem_string_server_crt[] PROGMEM;
extern const char _shared_progmem_string_server_key[] PROGMEM;
extern const char _shared_progmem_string_component_light[] PROGMEM;
extern const char _shared_progmem_string_component_switch[] PROGMEM;
extern const char _shared_progmem_string_component_sensor[] PROGMEM;
extern const char _shared_progmem_string_component_binary_sensor[] PROGMEM;
extern const char _shared_progmem_string_mqtt_prefix[] PROGMEM;
extern const char _shared_progmem_string_mqtt_auto_discovery[] PROGMEM;
extern const char _shared_progmem_string_mqtt_remote_config[] PROGMEM;
extern const char _shared_progmem_string_kfcfw[] PROGMEM;
extern const char _shared_progmem_string_invalid_flash_ptr[] PROGMEM;

String spgm_concat(PGM_P *str1, PGM_P *str2);
String spgm_concat(PGM_P *str1, int i);
String spgm_concat(PGM_P *str1, char c);
