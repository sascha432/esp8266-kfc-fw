/**
 * Author: sascha_lammers@gmx.de
 */

// strings that are used but not scanned by FlashStringGenerator

#if defined(MISSING_FLASH_STRINGS_ENABLED) && MISSING_FLASH_STRINGS_ENABLED

#include <Arduino_compat.h>

void dummy() {
    // KFCForms
    Serial.print(FSPGM(FormRangeValidator_default_message));//, "This fields value must be between %min% and %max%"));
    Serial.print(FSPGM(FormRangeValidator_default_message_zero_allowed));//, "This fields value must be between %min% and %max% or 0"));
    Serial.print(FSPGM(FormLengthValidator_default_message));//, "This field must be between %min% and %max% characters"));
    Serial.print(FSPGM(FormEnumValidator_default_message));//, "Invalid value: %allowed%"));
    Serial.print(FSPGM(FormHostValidator_default_message));//, "Invalid hostname or IP address"));
    Serial.print(FSPGM(Form_value_missing_default_message));//, "This field is missing"));
    Serial.print(FSPGM(FormValidator_allowed_macro, "%allowed%"));
    Serial.print(FSPGM(FormValidator_min_macro, "%min%"));
    Serial.print(FSPGM(FormValidator_max_macro, "%max%"));

    // Login Failure Counter
    Serial.print(FSPGM(login_failure_file));//, "/.pvt/login_failures"));

    // HttpHeaders
    Serial.print(FSPGM(Pragma, "Pragma"));
    Serial.print(FSPGM(Link, "Link"));
    Serial.print(FSPGM(Location, "Location"));
    Serial.print(FSPGM(RFC7231_date, "%a, %d %b %Y %H:%M:%S GMT"));
    Serial.print(FSPGM(Cache_Control, "Cache-Control"));
    Serial.print(FSPGM(Content_Length, "Content-Length"));
    Serial.print(FSPGM(Content_Encoding, "Content-Encoding"));
    Serial.print(FSPGM(Connection, "Connection"));
    Serial.print(FSPGM(Cookie, "Cookie"));
    Serial.print(FSPGM(Set_Cookie, "Set-Cookie"));
    Serial.print(FSPGM(Last_Modified, "Last-Modified"));
    Serial.print(FSPGM(Expires, "Expires"));
    Serial.print(FSPGM(no_cache, "no-cache"));
    Serial.print(FSPGM(close, "close"));
    Serial.print(FSPGM(keep_alive, "keep-alive"));
    Serial.print(FSPGM(public, "public"));
    Serial.print(FSPGM(private, "private"));
    Serial.print(FSPGM(Authorization, "Authorization"));
    Serial.print(FSPGM(Bearer_, "Bearer "));

    // FSMapping
    Serial.print(FSPGM(fs_mapping_dir));//, "/webui/"));
    Serial.print(FSPGM(fs_mapping_listings));//, "/webui/.listings"));

    // JsonTools
    Serial.print(FSPGM(true, "true"));
    Serial.print(FSPGM(false, "false"));
    Serial.print(FSPGM(null, "null"));


    // Serial.print(FSPGM());
}


#endif
