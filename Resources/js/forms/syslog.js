/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // syslog.html
    if ($('#syslog_settings').length) {
        function syslog_change() {
            if ($('#syslog_enabled').val() != "0") {
                $('#syslog_settings').show();
            } else {
                $('#syslog_settings').hide();
            }
        }
        $('#syslog_enabled').change(syslog_change);
        syslog_change();
    }
});
