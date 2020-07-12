/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // ntp.html
    if ($('#ntp_settings').length) {
        function ntp_change() {
            if (parseInt($('#ntp_enabled').val()) == 1) {
                $('#ntp_settings').show();
            } else {
                $('#ntp_settings').hide();
            }
        }
        $('#ntp_enabled').on('change', ntp_change);
        $('#ntp_posix_tz').on('change', function() {
            $('#ntp_timezone').val($(this).find('option:selected').text());
        });
        ntp_change();
    }
});
