/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // ntp.html
    if ($('#ntp_settings').length) {
        $('#posix_TZ').on('change', function() {
            $('#TZ_name').val($(this).find('option:selected').text());
        });
    }
});
