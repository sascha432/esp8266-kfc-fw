/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // device.html
    function normalize_0_value() { // display placeholder if int value is 0
        var value = parseInt($(this).val());
        if (value == 0) {
            $(this).val('');
        }
    }
    if ($('#device_settings').length) {
        $('#safe_mode_reboot_time').on('blur', normalize_0_value).each(normalize_0_value);
        $('#webui_keep_logged_in_days').on('blur', normalize_0_value).each(normalize_0_value);
    }
});
