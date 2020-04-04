/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // ntp.html
    if ($('#ntp_settings').length) {
        function ntp_change() {
            if ($('#ntp_enabled').val() == "1") {
                $('#ntp_settings').show();
            } else {
                $('#ntp_settings').hide();
            }
        }
        $('#ntp_enabled').change(ntp_change);
        ntp_change();
    }
});
