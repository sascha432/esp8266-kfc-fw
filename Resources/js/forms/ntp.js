/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // ntp.html
    if ($('#ntp_settings').length) {
        $('#posix_tz').on('change', function() {
            $('#tz_name').val($(this).find('option:selected').text());
        });
    }
});
