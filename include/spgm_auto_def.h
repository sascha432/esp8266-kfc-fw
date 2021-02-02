/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <Arduino_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

FLASH_STRING_GENERATOR_AUTO_INIT(
    // lib/KFCBaseLibrary/include/misc.h:619 (SPGM)
    AUTO_STRING_DEF(0x_08x, "0x%08x")
    //
    AUTO_STRING_DEF(AP, "AP")
    //
    AUTO_STRING_DEF(Accept_Encoding, "Accept-Encoding")
    //
    AUTO_STRING_DEF(Access_Point, "Access Point")
    AUTO_STRING_DEF(Action, "Action")
    // src/async_web_response.cpp:786 (SPGM)
    AUTO_STRING_DEF(Address, "Address")
    //
    AUTO_STRING_DEF(Admin, "Admin")
    //
    AUTO_STRING_DEF(Alarm, "Alarm")
    //
    AUTO_STRING_DEF(Animation, "Animation")
    //
    AUTO_STRING_DEF(Anonymous, "Anonymous")
    //
    AUTO_STRING_DEF(Authentication, "Authentication")
    // lib/KFCWebFramework/include/HttpHeaders.h:300 (SPGM)
    AUTO_STRING_DEF(Authorization, "Authorization")
    //
    AUTO_STRING_DEF(Auto, "Auto")
    // lib/KFCWebFramework/include/HttpHeaders.h:308 (SPGM)
    AUTO_STRING_DEF(Bearer_, "Bearer ")
    //
    AUTO_STRING_DEF(Brightness, "Brightness")
    //
    AUTO_STRING_DEF(Busy, "Busy")
    // lib/KFCWebFramework/include/HttpHeaders.h:137 (SPGM)
    AUTO_STRING_DEF(Cache_Control, "Cache-Control")
    //
    AUTO_STRING_DEF(Change_Password, "Change Password")
    //
    AUTO_STRING_DEF(Channel, "Channel")
    //
    AUTO_STRING_DEF(Client, "Client")
    //
    AUTO_STRING_DEF(Closed, "Closed")
    //
    AUTO_STRING_DEF(Color, "Color")
    //
    AUTO_STRING_DEF(Configuration, "Configuration")
    //
    AUTO_STRING_DEF(Connection, "Connection")
    //
    AUTO_STRING_DEF(Content_Encoding, "Content-Encoding")
    // lib/KFCWebFramework/include/HttpHeaders.h:112 (SPGM)
    AUTO_STRING_DEF(Content_Length, "Content-Length")
    //
    AUTO_STRING_DEF(Cookie, "Cookie")
    //
    AUTO_STRING_DEF(DHCP, "DHCP")
    //
    AUTO_STRING_DEF(DHCP_Client, "DHCP Client")
    //
    AUTO_STRING_DEF(DHCP_Server, "DHCP Server")
    //
    AUTO_STRING_DEF(DNS_1, "DNS 1")
    //
    AUTO_STRING_DEF(DNS_2, "DNS 2")
    //
    AUTO_STRING_DEF(Default, "Default")
    //
    AUTO_STRING_DEF(Delay, "Delay")
    //
    AUTO_STRING_DEF(Device, "Device")
    //
    AUTO_STRING_DEF(Device_Configuration, "Device Configuration")
    //
    AUTO_STRING_DEF(Device_is_rebooting, "Device is rebooting...\\n")
    // lib/KFCWebFramework/include/WebUI/Containers.h:711 (SPGM)
    AUTO_STRING_DEF(Disabled, "Disabled")
    //
    AUTO_STRING_DEF(EN, "EN")
    //
    AUTO_STRING_DEF(ERROR_, "ERROR:")
    // lib/KFCWebFramework/include/WebUI/Containers.h:711 (SPGM)
    AUTO_STRING_DEF(Enabled, "Enabled")
    //
    AUTO_STRING_DEF(Encryption, "Encryption")
    //
    AUTO_STRING_DEF(Expires, "Expires")
    //
    AUTO_STRING_DEF(Fading, "Fading")
    //
    AUTO_STRING_DEF(Flashing, "Flashing")
    // lib/KFCWebFramework/include/Validator/Enum.h:26 (SPGM)
    AUTO_STRING_DEF(FormEnumValidator_default_message, "Invalid value: %allowed%")
    // lib/KFCWebFramework/include/Validator/Host.h:24 (SPGM)
    AUTO_STRING_DEF(FormHostValidator_default_message, "Invalid hostname or IP address")
    // lib/KFCWebFramework/include/Validator/Length.h:19 (SPGM), lib/KFCWebFramework/include/Validator/Length.h:26 (SPGM)
    AUTO_STRING_DEF(FormLengthValidator_default_message, "This field must be between %min% and %max% characters")
    //
    AUTO_STRING_DEF(FormRangeValidator_default_message, "This fields value must be between %min% and %max%")
    //
    AUTO_STRING_DEF(FormRangeValidator_default_message_zero_allowed, "This fields value must be 0 or between %min% and %max%")
    //
    AUTO_STRING_DEF(FormRangeValidator_invalid_port, "Invalid Port")
    //
    AUTO_STRING_DEF(FormRangeValidator_invalid_port_range, "Invalid Port (%min%-%max%)")
    //
    AUTO_STRING_DEF(FormValidator_allowed_macro, "%allowed%")
    // lib/KFCWebFramework/include/Validator/Range.h:65 (SPGM)
    AUTO_STRING_DEF(FormValidator_max_macro, "%max%")
    // lib/KFCWebFramework/include/Validator/Range.h:64 (SPGM)
    AUTO_STRING_DEF(FormValidator_min_macro, "%min%")
    //
    AUTO_STRING_DEF(Form_value_missing_default_message, "This field is missing")
    //
    AUTO_STRING_DEF(Gateway, "Gateway")
    //
    AUTO_STRING_DEF(HIDDEN, "HIDDEN")
    //
    AUTO_STRING_DEF(HTTP_Server, "HTTP Server")
    //
    AUTO_STRING_DEF(Home, "Home")
    // src/async_web_response.cpp:790 (SPGM)
    AUTO_STRING_DEF(Hostname, "Hostname")
    //
    AUTO_STRING_DEF(Hz, "Hz")
    //
    AUTO_STRING_DEF(IP_Address, "IP Address")
    //
    AUTO_STRING_DEF(Interval, "Interval")
    //
    AUTO_STRING_DEF(Invalid_brightness, "Invalid brightness")
    //
    AUTO_STRING_DEF(Invalid_mode, "Invalid mode")
    //
    AUTO_STRING_DEF(Invalid_port, "Invalid port")
    //
    AUTO_STRING_DEF(Invalid_time, "Invalid time")
    //
    AUTO_STRING_DEF(Invalid_username_or_password, "Invalid username or password")
    //
    AUTO_STRING_DEF(KFCLabs, "KFCLabs")
    //
    AUTO_STRING_DEF(KFC_Firmware, "KFC Firmware")
    //
    AUTO_STRING_DEF(Keep_Alive, "Keep Alive")
    //
    AUTO_STRING_DEF(Last_Modified, "Last-Modified")
    //
    AUTO_STRING_DEF(Link, "Link")
    //
    AUTO_STRING_DEF(Location, "Location")
    //
    AUTO_STRING_DEF(Mode, "Mode")
    //
    AUTO_STRING_DEF(Move, "Move")
    //
    AUTO_STRING_DEF(Multiplier, "Multiplier")
    //
    AUTO_STRING_DEF(NTP_Client, "NTP Client")
    //
    AUTO_STRING_DEF(NTP_Client_Configuration, "NTP Client Configuration")
    //
    AUTO_STRING_DEF(NTP_Server___, "NTP Server %u")
    //
    AUTO_STRING_DEF(Name, "Name")
    //
    AUTO_STRING_DEF(Network, "Network")
    //
    AUTO_STRING_DEF(Network_Configuration, "Network Configuration")
    //
    AUTO_STRING_DEF(No, "No")
    //
    AUTO_STRING_DEF(None, "None")
    //
    AUTO_STRING_DEF(Not_supported, "Not supported")
    // src/AtModeArgs.cpp:190 (SPGM)
    AUTO_STRING_DEF(OK, "OK")
    //
    AUTO_STRING_DEF(Off, "Off")
    //
    AUTO_STRING_DEF(Offline, "Offline")
    //
    AUTO_STRING_DEF(On, "On")
    //
    AUTO_STRING_DEF(Open, "Open")
    //
    AUTO_STRING_DEF(Passphrase, "Passphrase")
    //
    AUTO_STRING_DEF(Password, "Password")
    //
    AUTO_STRING_DEF(Port, "Port")
    //
    AUTO_STRING_DEF(Pragma, "Pragma")
    //
    AUTO_STRING_DEF(Protection, "Protection")
    //
    AUTO_STRING_DEF(RFC7231_date, "%a, %d %b %Y %H:%M:%S GMT")
    //
    AUTO_STRING_DEF(Rainbow, "Rainbow")
    //
    AUTO_STRING_DEF(Reboot_Device, "Reboot Device")
    //
    AUTO_STRING_DEF(Refresh_Interval, "Refresh Interval")
    //
    AUTO_STRING_DEF(Running, "Running")
    //
    AUTO_STRING_DEF(SID, "SID")
    //
    AUTO_STRING_DEF(SPIFFS_tmp_dir, "/.tmp/")
    //
    AUTO_STRING_DEF(SRV, "SRV")
    //
    AUTO_STRING_DEF(SSDP_Discovery, "SSDP Discovery")
    //
    AUTO_STRING_DEF(SSID, "SSID")
    //
    AUTO_STRING_DEF(STA, "STA")
    //
    AUTO_STRING_DEF(Save_Changes, "Save Changes")
    //
    AUTO_STRING_DEF(Serial_Console, "Serial Console")
    //
    AUTO_STRING_DEF(Server, "Server")
    // lib/KFCWebFramework/include/HttpHeaders.h:198 (SPGM)
    AUTO_STRING_DEF(Set_Cookie, "Set-Cookie")
    //
    AUTO_STRING_DEF(Software_Serial, "Software Serial")
    //
    AUTO_STRING_DEF(Solid_Color, "Solid Color")
    //
    AUTO_STRING_DEF(Speed, "Speed")
    //
    AUTO_STRING_DEF(Station_Mode, "Station Mode")
    //
    AUTO_STRING_DEF(Status, "Status")
    //
    AUTO_STRING_DEF(Status_LED_Mode, "Status LED Mode")
    //
    AUTO_STRING_DEF(Stopped, "Stopped")
    //
    AUTO_STRING_DEF(Subnet, "Subnet")
    //
    AUTO_STRING_DEF(Success, "Success")
    //
    AUTO_STRING_DEF(Timeout, "Timeout")
    //
    AUTO_STRING_DEF(Timezone, "Timezone")
    //
    AUTO_STRING_DEF(Title, "Title")
    //
    AUTO_STRING_DEF(Topic, "Topic")
    //
    AUTO_STRING_DEF(Turn, "Turn")
    //
    AUTO_STRING_DEF(Type, "Type")
    //
    AUTO_STRING_DEF(Username, "Username")
    //
    AUTO_STRING_DEF(WebUI, "Web UI")
    //
    AUTO_STRING_DEF(Web_Alerts, "Web Alerts")
    //
    AUTO_STRING_DEF(WiFi, "WiFi")
    //
    AUTO_STRING_DEF(WiFi_Configuration, "WiFi Configuration")
    //
    AUTO_STRING_DEF(WiFi_Mode, "WiFi Mode")
    //
    AUTO_STRING_DEF(Yes, "Yes")
    //
    AUTO_STRING_DEF(Your_session_has_expired, "Your session has expired")
    //
    AUTO_STRING_DEF(Zeroconf_Logging, "Zeroconf Logging")
    //
    AUTO_STRING_DEF(Zeroconf_Timeout, "Zeroconf Timeout")
    //
    AUTO_STRING_DEF(_02x, "%02x")
    // src/PluginComponent.cpp:26 (AUTO_INIT), src/PluginComponent.cpp:140 (SPGM), src/PluginComponent.cpp:145 (SPGM), src/PluginComponent.cpp:150 (SPGM), src/PluginComponent.cpp:156 (SPGM), src/PluginComponent.cpp:161 (SPGM), src/PluginComponent.cpp:192 (SPGM)
    AUTO_STRING_DEF(__pure_virtual, "pure virtual call: %s\\n")
    //
    AUTO_STRING_DEF(__s_, "/%s/")
    //
    AUTO_STRING_DEF(_brightness_set, "/brightness/set")
    //
    AUTO_STRING_DEF(_brightness_state, "/brightness/state")
    //
    AUTO_STRING_DEF(_checked, " checked")
    //
    AUTO_STRING_DEF(_color_set, "/color/set")
    //
    AUTO_STRING_DEF(_color_state, "/color/state")
    //
    AUTO_STRING_DEF(degree_utf8, "°")
    //
    AUTO_STRING_DEF(degree_html, "&deg;")
    //
    AUTO_STRING_DEF(degree_unicode, "\\u00b0C")
    //
    AUTO_STRING_DEF(degree_Celsius_utf8, "°C")
    //
    AUTO_STRING_DEF(degree_Celsius_html, "&deg;C")
    //
    AUTO_STRING_DEF(degree_Celsius_unicode, "\\u00b0C")
    //
    AUTO_STRING_DEF(_hidden, " hidden")
    //
    AUTO_STRING_DEF(_html, ".html")
    //
    AUTO_STRING_DEF(_login_html, "/login.html")
    //
    AUTO_STRING_DEF(_selected, " selected")
    //
    AUTO_STRING_DEF(_serial_console, "/serial-console")
    //
    AUTO_STRING_DEF(_set, "/set")
    //
    AUTO_STRING_DEF(_state, "/state")
    //
    AUTO_STRING_DEF(_var_gateway, "${gateway}")
    // lib/KFCWebFramework/include/Validator/Host.h:50 (SPGM), src/async_web_response.cpp:804 (SPGM)
    AUTO_STRING_DEF(_var_zeroconf, "${zeroconf:")
    //
    AUTO_STRING_DEF(_xml, ".xml")
    //
    AUTO_STRING_DEF(address, "address")
    //
    AUTO_STRING_DEF(alarm, "alarm")
    //
    AUTO_STRING_DEF(alert, "alert")
    // src/WebUIAlerts.cpp:46 (DEFINE), src/WebUIAlerts.cpp:385 (SPGM), src/WebUIAlerts.cpp:387 (SPGM), src/WebUIAlerts.cpp:389 (SPGM), src/WebUIAlerts.cpp:566 (SPGM), src/WebUIAlerts.cpp:569 (SPGM), src/WebUIAlerts.cpp:606 (SPGM), src/WebUIAlerts.cpp:611 (SPGM), src/WebUIAlerts.cpp:613 (SPGM), src/WebUIAlerts.cpp:624 (SPGM), src/WebUIAlerts.cpp:625 (SPGM), src/WebUIAlerts.cpp:627 (SPGM), src/WebUIAlerts.cpp:628 (SPGM), src/WebUIAlerts.cpp:637 (SPGM), src/WebUIAlerts.cpp:640 (SPGM)
    AUTO_STRING_DEF(alerts_storage_filename, "/.pvt/alerts.json")
    //
    AUTO_STRING_DEF(ap_mode, "ap_mode")
    //
    AUTO_STRING_DEF(applying_html, "applying.html")
    //
    AUTO_STRING_DEF(auth, "auth")
    //
    AUTO_STRING_DEF(binary, "binary")
    //
    AUTO_STRING_DEF(boolean, "boolean")
    //
    AUTO_STRING_DEF(brightness, "brightness")
    //
    AUTO_STRING_DEF(busy, "busy")
    //
    AUTO_STRING_DEF(bytes, "bytes")
    //
    AUTO_STRING_DEF(cfg, "cfg")
    //
    AUTO_STRING_DEF(channel__u, "channel_%u")
    //
    AUTO_STRING_DEF(channels, "channels")
    //
    AUTO_STRING_DEF(client, "client")
    //
    AUTO_STRING_DEF(close, "close")
    //
    AUTO_STRING_DEF(comma_, ", ")
    //
    AUTO_STRING_DEF(config, "config")
    //
    AUTO_STRING_DEF(config_object_name, "config")
    //
    AUTO_STRING_DEF(cookie, "cookie")
    // src/SaveCrash.cpp:18 (SPGM), src/SaveCrash.cpp:22 (SPGM), src/SaveCrash.cpp:36 (SPGM)
    AUTO_STRING_DEF(crash_counter_file, "/.pvt/crash_counter")
    // src/SaveCrash.cpp:77 (SPGM), src/SaveCrash.cpp:125 (SPGM)
    AUTO_STRING_DEF(crash_dump_file, "/.pvt/crash.%03x")
    //
    AUTO_STRING_DEF(crit, "crit")
    //
    AUTO_STRING_DEF(critical, "critical")
    //
    AUTO_STRING_DEF(current, "current")
    //
    AUTO_STRING_DEF(daemon, "daemon")
    //
    AUTO_STRING_DEF(day, "day")
    //
    AUTO_STRING_DEF(days, "days")
    //
    AUTO_STRING_DEF(debug, "debug")
    //
    AUTO_STRING_DEF(defaultPassword, "12345678")
    //
    AUTO_STRING_DEF(default_password_warning, "WARNING! Default password has not been changed")
    //
    AUTO_STRING_DEF(deflate, "deflate")
    //
    AUTO_STRING_DEF(description_xml, "description.xml")
    //
    AUTO_STRING_DEF(dev_hostn, "dev_hostn")
    //
    AUTO_STRING_DEF(dev_title, "dev_title")
    //
    AUTO_STRING_DEF(device, "device")
    //
    AUTO_STRING_DEF(device_html, "device.html")
    //
    AUTO_STRING_DEF(dir, "dir")
    // lib/KFCWebFramework/include/WebUI/Containers.h:705 (SPGM)
    AUTO_STRING_DEF(disabled, "disabled")
    //
    AUTO_STRING_DEF(display, "display")
    //
    AUTO_STRING_DEF(domain, "domain")
    //
    AUTO_STRING_DEF(ds3231_id_lost_power, "ds3231_lost_power")
    //
    AUTO_STRING_DEF(ds3231_id_temp, "ds3231_temp")
    //
    AUTO_STRING_DEF(ds3231_id_time, "ds3231_time")
    //
    AUTO_STRING_DEF(emerg, "emerg")
    //
    AUTO_STRING_DEF(emergency, "emergency")
    //
    AUTO_STRING_DEF(enabled, "enabled")
    //
    AUTO_STRING_DEF(encryption, "encryption")
    //
    AUTO_STRING_DEF(energy, "energy")
    //
    AUTO_STRING_DEF(energy_total, "energy_total")
    //
    AUTO_STRING_DEF(err, "err")
    //
    AUTO_STRING_DEF(error, "error")
    //
    AUTO_STRING_DEF(factory_html, "factory.html")
    // src/at_mode.cpp:1407 (SPGM)
    AUTO_STRING_DEF(failure, "failure")
    // lib/KFCJson/include/JsonVariant.h:79 (SPGM)
    AUTO_STRING_DEF(false, "false")
    //
    AUTO_STRING_DEF(file_manager_base_uri, "/file_manager/")
    //
    AUTO_STRING_DEF(file_manager_html_uri, "file-manager.html")
    //
    AUTO_STRING_DEF(filename, "filename")
    //
    AUTO_STRING_DEF(float, "float")
    //
    AUTO_STRING_DEF(frequency, "frequency")
    //
    AUTO_STRING_DEF(fs_mapping_dir, "/webui/")
    //
    AUTO_STRING_DEF(fs_mapping_listings, "/webui/.listings")
    //
    AUTO_STRING_DEF(get_, "get_")
    //
    AUTO_STRING_DEF(gzip, "gzip")
    //
    AUTO_STRING_DEF(hPa, "hPa")
    //
    AUTO_STRING_DEF(heap, "heap")
    //
    AUTO_STRING_DEF(hidden, "hidden")
    //
    AUTO_STRING_DEF(host, "host")
    //
    AUTO_STRING_DEF(hour, "hour")
    //
    AUTO_STRING_DEF(htmlentities_acute, "&acute;")
    //
    AUTO_STRING_DEF(htmlentities_amp, "&amp;")
    //
    AUTO_STRING_DEF(htmlentities_apos, "&apos;")
    //
    AUTO_STRING_DEF(htmlentities_copy, "&copy;")
    //
    AUTO_STRING_DEF(htmlentities_deg, "&deg;")
    //
    AUTO_STRING_DEF(htmlentities_equals, "&equals;")
    //
    AUTO_STRING_DEF(htmlentities_gt, "&gt;")
    //
    AUTO_STRING_DEF(htmlentities_lt, "&lt;")
    //
    AUTO_STRING_DEF(htmlentities_micro, "&plusmn;")
    //
    AUTO_STRING_DEF(htmlentities_num, "&num;")
    //
    AUTO_STRING_DEF(htmlentities_percnt, "&percnt;")
    //
    AUTO_STRING_DEF(htmlentities_plusmn, "&micro;")
    //
    AUTO_STRING_DEF(htmlentities_quest, "&quest;")
    //
    AUTO_STRING_DEF(htmlentities_quot, "&quot;")
    //
    AUTO_STRING_DEF(http, "http")
    //
    AUTO_STRING_DEF(httpmode, "httpmode")
    //
    AUTO_STRING_DEF(https, "https")
    //
    AUTO_STRING_DEF(humidity, "humidity")
    //
    AUTO_STRING_DEF(id, "id")
    //
    AUTO_STRING_DEF(image_type, "image_type")
    //
    AUTO_STRING_DEF(index_html, "index.html")
    //
    AUTO_STRING_DEF(info, "info")
    //
    AUTO_STRING_DEF(int, "int")
    //
    AUTO_STRING_DEF(interval, "interval")
    //
    AUTO_STRING_DEF(iot_blinds_control_state_file, "/.pvt/blinds_ctrl.state")
    //
    AUTO_STRING_DEF(iot_sensor_hlw80xx_state_file, "")
    //
    AUTO_STRING_DEF(iot_switch_states_file, "/.pvt/switch.states")
    //
    AUTO_STRING_DEF(ip_8_8_8_8, "8.8.8.8")
    //
    AUTO_STRING_DEF(is_, "is_")
    //
    AUTO_STRING_DEF(kWh, "kWh")
    //
    AUTO_STRING_DEF(keep, "keep")
    //
    AUTO_STRING_DEF(keep_alive, "keep-alive")
    //
    AUTO_STRING_DEF(keepalive, "keepalive")
    //
    AUTO_STRING_DEF(kern, "kern")
    //
    AUTO_STRING_DEF(kfcfw, "kfcfw")
    //
    AUTO_STRING_DEF(kfcmdns, "kfcmdns")
    //
    AUTO_STRING_DEF(lifetime, "lifetime")
    //
    AUTO_STRING_DEF(light_sensor, "light_sensor")
    //
    AUTO_STRING_DEF(local0, "local0")
    //
    AUTO_STRING_DEF(local1, "local1")
    //
    AUTO_STRING_DEF(local2, "local2")
    //
    AUTO_STRING_DEF(local3, "local3")
    //
    AUTO_STRING_DEF(local4, "local4")
    //
    AUTO_STRING_DEF(local5, "local5")
    //
    AUTO_STRING_DEF(local6, "local6")
    //
    AUTO_STRING_DEF(local7, "local7")
    //
    AUTO_STRING_DEF(lock_channels, "lock_channels")
    //
    AUTO_STRING_DEF(logger_filename_access, "/.logs/access")
    //
    AUTO_STRING_DEF(logger_filename_debug, "/.logs/debug")
    //
    AUTO_STRING_DEF(logger_filename_error, "/.logs/error")
    //
    AUTO_STRING_DEF(logger_filename_messags, "/.logs/messages")
    //
    AUTO_STRING_DEF(logger_filename_security, "/.logs/security")
    //
    AUTO_STRING_DEF(logger_filename_warning, "/.logs/warning")
    //
    AUTO_STRING_DEF(login_failure_file, "/.pvt/login_failures")
    //
    AUTO_STRING_DEF(login_html, "/login.html")
    //
    AUTO_STRING_DEF(mA, "mA")
    //
    AUTO_STRING_DEF(mail, "mail")
    //
    AUTO_STRING_DEF(main, "main")
    //
    AUTO_STRING_DEF(max, "max")
    //
    AUTO_STRING_DEF(message, "message")
    //
    AUTO_STRING_DEF(metrics, "metrics")
    //
    AUTO_STRING_DEF(milliseconds, "milliseconds")
    //
    AUTO_STRING_DEF(mime_application_javascript, "application/javascript")
    // src/async_web_response.cpp:166 (SPGM), src/async_web_response.cpp:302 (SPGM), src/async_web_response.cpp:472 (SPGM)
    AUTO_STRING_DEF(mime_application_json, "application/json")
    //
    AUTO_STRING_DEF(mime_application_pdf, "application/pdf")
    //
    AUTO_STRING_DEF(mime_application_x_gzip, "application/x-gzip")
    //
    AUTO_STRING_DEF(mime_application_zip, "application/zip")
    //
    AUTO_STRING_DEF(mime_font_eot, "font/eot")
    //
    AUTO_STRING_DEF(mime_font_ttf, "font/ttf")
    //
    AUTO_STRING_DEF(mime_font_woff, "font/woff")
    //
    AUTO_STRING_DEF(mime_font_woff2, "font/woff2")
    //
    AUTO_STRING_DEF(mime_image_bmp, "image/bmp")
    //
    AUTO_STRING_DEF(mime_image_gif, "image/gif")
    //
    AUTO_STRING_DEF(mime_image_icon, "image/x-icon")
    //
    AUTO_STRING_DEF(mime_image_jpeg, "image/jpeg")
    //
    AUTO_STRING_DEF(mime_image_png, "image/png")
    //
    AUTO_STRING_DEF(mime_image_svg_xml, "image/svg+xml")
    //
    AUTO_STRING_DEF(mime_text_css, "text/css")
    // src/async_web_response.cpp:705 (SPGM)
    AUTO_STRING_DEF(mime_text_html, "text/html")
    // src/async_web_handler.cpp:35 (SPGM), src/async_web_handler.cpp:50 (SPGM), src/async_web_handler.cpp:53 (SPGM)
    AUTO_STRING_DEF(mime_text_plain, "text/plain")
    //
    AUTO_STRING_DEF(mime_text_xml, "text/xml")
    //
    AUTO_STRING_DEF(min, "min")
    //
    AUTO_STRING_DEF(minutes, "minutes")
    //
    AUTO_STRING_DEF(minutes__, "minutes \\u00b1")
    //
    AUTO_STRING_DEF(minutes__5_, "minutes \\u00b15%")
    //
    AUTO_STRING_DEF(mode, "mode")
    //
    AUTO_STRING_DEF(moon_phase_0, "Full Moon")
    //
    AUTO_STRING_DEF(moon_phase_1, "Waning Gibbous")
    //
    AUTO_STRING_DEF(moon_phase_2, "Last Quarter")
    //
    AUTO_STRING_DEF(moon_phase_3, "Old Crescent")
    //
    AUTO_STRING_DEF(moon_phase_4, "New Moon")
    //
    AUTO_STRING_DEF(moon_phase_5, "New Crescent")
    //
    AUTO_STRING_DEF(moon_phase_6, "First Quarter")
    //
    AUTO_STRING_DEF(moon_phase_7, "Waxing Gibbous")
    //
    AUTO_STRING_DEF(mqtt, "mqtt")
    //
    AUTO_STRING_DEF(mqtt_availability_topic, "avty_t")
    //
    AUTO_STRING_DEF(mqtt_brightness_command_topic, "bri_cmd_t")
    //
    AUTO_STRING_DEF(mqtt_brightness_scale, "bri_scl")
    //
    AUTO_STRING_DEF(mqtt_brightness_state_topic, "bri_stat_t")
    //
    AUTO_STRING_DEF(mqtt_color_temp_command_topic, "clr_temp_cmd_t")
    //
    AUTO_STRING_DEF(mqtt_color_temp_state_topic, "clr_temp_stat_t")
    //
    AUTO_STRING_DEF(mqtt_command_topic, "cmd_t")
    //
    AUTO_STRING_DEF(mqtt_component_binary_sensor, "binary_sensor")
    //
    AUTO_STRING_DEF(mqtt_component_light, "light")
    //
    AUTO_STRING_DEF(mqtt_component_sensor, "sensor")
    //
    AUTO_STRING_DEF(mqtt_component_storage, "storage")
    //
    AUTO_STRING_DEF(mqtt_component_switch, "switch")
    //
    AUTO_STRING_DEF(mqtt_expire_after, "exp_aft")
    //
    AUTO_STRING_DEF(mqtt_payload_available, "pl_avail")
    //
    AUTO_STRING_DEF(mqtt_payload_not_available, "pl_not_avail")
    //
    AUTO_STRING_DEF(mqtt_payload_off, "pl_off")
    //
    AUTO_STRING_DEF(mqtt_payload_on, "pl_on")
    //
    AUTO_STRING_DEF(mqtt_rgb_command_topic, "rgb_cmd_t")
    //
    AUTO_STRING_DEF(mqtt_rgb_state_topic, "rgb_stat_t")
    //
    AUTO_STRING_DEF(mqtt_state_topic, "stat_t")
    //
    AUTO_STRING_DEF(mqtt_status_topic, "/status")
    //
    AUTO_STRING_DEF(mqtt_switch_set, "/switch/set")
    //
    AUTO_STRING_DEF(mqtt_switch_state, "/switch/state")
    //
    AUTO_STRING_DEF(mqtt_unique_id, "uniq_id")
    //
    AUTO_STRING_DEF(mqtt_unit_of_measurement, "unit_of_meas")
    //
    AUTO_STRING_DEF(mqtt_value_template, "val_tpl")
    //
    AUTO_STRING_DEF(ms, "ms")
    //
    AUTO_STRING_DEF(n_a, "n/a")
    //
    AUTO_STRING_DEF(name, "name")
    //
    AUTO_STRING_DEF(network, "network")
    //
    AUTO_STRING_DEF(network_html, "network.html")
    //
    AUTO_STRING_DEF(network_settings, "network_settings")
    // src/at_mode.cpp:1265 (SPGM)
    AUTO_STRING_DEF(no, "no")
    //
    AUTO_STRING_DEF(no_cache, "no-cache")
    //
    AUTO_STRING_DEF(notice, "notice")
    //
    AUTO_STRING_DEF(npwd, "npwd")
    //
    AUTO_STRING_DEF(ntp, "ntp")
    // lib/KFCBaseLibrary/include/misc.h:625 (SPGM), lib/KFCBaseLibrary/include/misc.h:626 (SPGM), lib/KFCBaseLibrary/include/misc.h:627 (SPGM), lib/KFCJson/include/JsonVariant.h:82 (SPGM)
    AUTO_STRING_DEF(null, "null")
    // src/at_mode.cpp:1243 (SPGM), src/at_mode.cpp:1262 (SPGM), src/at_mode.cpp:1263 (SPGM), src/at_mode.cpp:1269 (SPGM), src/at_mode.cpp:1270 (SPGM)
    AUTO_STRING_DEF(off, "off")
    // src/at_mode.cpp:1238 (SPGM), src/at_mode.cpp:1262 (SPGM), src/at_mode.cpp:1263 (SPGM), src/at_mode.cpp:1269 (SPGM), src/at_mode.cpp:1270 (SPGM)
    AUTO_STRING_DEF(on, "on")
    //
    AUTO_STRING_DEF(open, "open")
    //
    AUTO_STRING_DEF(openweathermap_api_url, "http://api.openweathermap.org/data/2.5/{api_type}?q={query}&appid={api_key}")
    //
    AUTO_STRING_DEF(password, "password")
    //
    AUTO_STRING_DEF(password2, "password2")
    //
    AUTO_STRING_DEF(password_html, "password.html")
    //
    AUTO_STRING_DEF(performance, "performance")
    //
    AUTO_STRING_DEF(performance_mode, "performance_mode")
    //
    AUTO_STRING_DEF(pf, "pf")
    //
    AUTO_STRING_DEF(ping_monitor, "ping_monitor")
    //
    AUTO_STRING_DEF(ping_monitor_cancelled, "Ping cancelled")
    //
    AUTO_STRING_DEF(ping_monitor_end_response, "Total answer from %s sent %d recevied %d time %ld ms")
    //
    AUTO_STRING_DEF(ping_monitor_ethernet_detected, "Detected eth address %s")
    //
    AUTO_STRING_DEF(ping_monitor_ping_for_hostname_failed, "Pinging %s failed")
    //
    AUTO_STRING_DEF(ping_monitor_request_timeout, "Request timed out.")
    //
    AUTO_STRING_DEF(ping_monitor_response, "%d bytes from %s: icmp_seq=%d ttl=%d time=%ld ms")
    //
    AUTO_STRING_DEF(ping_monitor_service, "Ping Monitor Service")
    //
    AUTO_STRING_DEF(ping_monitor_service_status, "Ping monitor service has been %s")
    //
    AUTO_STRING_DEF(ping_monitor_unknown_service, "ping: %s: Name or service not known")
    //
    AUTO_STRING_DEF(placeholder, "placeholder")
    //
    AUTO_STRING_DEF(plugin_name_rd, "rd")
    //
    AUTO_STRING_DEF(port, "port")
    //
    AUTO_STRING_DEF(power, "power")
    //
    AUTO_STRING_DEF(prefix, "prefix")
    //
    AUTO_STRING_DEF(pressure, "pressure")
    //
    AUTO_STRING_DEF(private, "private")
    //
    AUTO_STRING_DEF(public, "public")
    //
    AUTO_STRING_DEF(pwm, "pwm")
    // lib/KFCWebFramework/include/WebUI/Containers.h:699 (SPGM)
    AUTO_STRING_DEF(readonly, "readonly")
    //
    AUTO_STRING_DEF(reboot, "reboot")
    //
    AUTO_STRING_DEF(reboot_html, "reboot.html")
    //
    AUTO_STRING_DEF(rebooting_html, "rebooting.html")
    //
    AUTO_STRING_DEF(refresh, "refresh")
    //
    AUTO_STRING_DEF(remote_html, "remote.html")
    //
    AUTO_STRING_DEF(safe_mode, "safe_mode")
    //
    AUTO_STRING_DEF(safe_mode_enabled, "Device started in SAFE MODE")
    //
    AUTO_STRING_DEF(sec, "sec")
    //
    AUTO_STRING_DEF(seconds, "seconds")
    //
    AUTO_STRING_DEF(secure, "secure")
    //
    AUTO_STRING_DEF(sensors, "sensors")
    //
    AUTO_STRING_DEF(serial_console_html, "serial-console.html")
    //
    AUTO_STRING_DEF(server, "server")
    //
    AUTO_STRING_DEF(server_crt, "/server.crt")
    //
    AUTO_STRING_DEF(server_key, "/server.key")
    //
    AUTO_STRING_DEF(set_, "set_")
    //
    AUTO_STRING_DEF(set_all, "set_all")
    //
    AUTO_STRING_DEF(size, "size")
    //
    AUTO_STRING_DEF(slash, "/")
    //
    AUTO_STRING_DEF(softap, "softap")
    //
    AUTO_STRING_DEF(softap_, "softap_")
    //
    AUTO_STRING_DEF(standby, "standby")
    //
    AUTO_STRING_DEF(standby_mode, "standby_mode")
    //
    AUTO_STRING_DEF(started, "started")
    //
    AUTO_STRING_DEF(state, "state")
    //
    AUTO_STRING_DEF(states, "states")
    //
    AUTO_STRING_DEF(station, "station")
    //
    AUTO_STRING_DEF(station_mode, "station_mode")
    //
    AUTO_STRING_DEF(status, "status")
    //
    AUTO_STRING_DEF(status__u, "status=%u")
    //
    AUTO_STRING_DEF(status_html, "status.html")
    //
    AUTO_STRING_DEF(stk500v1_log_file, "/stk500v1/debug.log")
    //
    AUTO_STRING_DEF(stk500v1_sig_file, "/stk500v1/atmega.csv")
    //
    AUTO_STRING_DEF(stk500v1_tmp_file, "/stk500v1/firmware_tmp.hex")
    //
    AUTO_STRING_DEF(stopped, "stopped")
    //
    AUTO_STRING_DEF(strftime_date_time_zone, "%FT%T %Z")
    //
    AUTO_STRING_DEF(string, "string")
    // src/at_mode.cpp:1407 (SPGM)
    AUTO_STRING_DEF(success, "success")
    //
    AUTO_STRING_DEF(sys, "sys")
    //
    AUTO_STRING_DEF(syslog, "syslog")
    //
    AUTO_STRING_DEF(syslog_nil_value, "- ")
    //
    AUTO_STRING_DEF(tcp, "tcp")
    //
    AUTO_STRING_DEF(temperature, "temperature")
    //
    AUTO_STRING_DEF(ticks, "ticks")
    //
    AUTO_STRING_DEF(timeout, "timeout")
    //
    AUTO_STRING_DEF(timezone, "timezone")
    //
    AUTO_STRING_DEF(title, "title")
    //
    AUTO_STRING_DEF(topic, "topic")
    // lib/KFCJson/include/JsonVariant.h:79 (SPGM), src/WebUISocket.cpp:128 (SPGM)
    AUTO_STRING_DEF(true, "true")
    //
    AUTO_STRING_DEF(udp, "udp")
    //
    AUTO_STRING_DEF(update_fw_html, "update-fw.html")
    //
    AUTO_STRING_DEF(upload, "upload")
    //
    AUTO_STRING_DEF(upload_file, "upload_file")
    //
    AUTO_STRING_DEF(uptime, "uptime")
    //
    AUTO_STRING_DEF(user, "user")
    //
    AUTO_STRING_DEF(username, "username")
    //
    AUTO_STRING_DEF(value, "value")
    //
    AUTO_STRING_DEF(values, "values")
    //
    AUTO_STRING_DEF(vcc, "vcc")
    //
    AUTO_STRING_DEF(version, "version")
    //
    AUTO_STRING_DEF(voltage, "voltage")
    //
    AUTO_STRING_DEF(warn, "warn")
    //
    AUTO_STRING_DEF(warning, "warning")
    //
    AUTO_STRING_DEF(weather_station_webui_id, "ws_tft")
    //
    AUTO_STRING_DEF(webserver, "webserver")
    //
    AUTO_STRING_DEF(webui, "webui")
    // src/WebUISocket.cpp:28 (DEFINE), src/WebUISocket.cpp:42 (SPGM)
    AUTO_STRING_DEF(webui_socket_uri, "/webui-ws")
    //
    AUTO_STRING_DEF(wifi, "wifi")
    //
    AUTO_STRING_DEF(wifi_html, "wifi.html")
    //
    AUTO_STRING_DEF(wifi_mode, "wifi_mode")
    //
    AUTO_STRING_DEF(wifi_settings, "wifi_settings")
    //
    AUTO_STRING_DEF(www_google_com, "www.google.com")
    // src/at_mode.cpp:1265 (SPGM)
    AUTO_STRING_DEF(yes, "yes")
);

#ifdef __cplusplus
} // extern "C"
#endif
