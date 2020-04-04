/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // dimmer_cfg.html
    if ($('#dimmer_settings').length) {
        $('#hass_yaml').on('focus', function() {
            $('#hass_yaml').animate({height: 300}, 1000);
        });
        $('#hass_yaml').on('blur', function() {
            $('#hass_yaml').animate({height: 40}, 1000);
        });
    }
});
