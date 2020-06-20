/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // device.html
    function normalize_safe_mode_reboot_time() {
        var field = $('#safe_mode_reboot_time');
        var value = parseInt(field.val());
        if (value == 0) {
            field.val('');
        }
    }
    if ($('#device_settings').length) {
        $('#safe_mode_reboot_time').on('blur', normalize_safe_mode_reboot_time);
    }
    normalize_safe_mode_reboot_time();
});
