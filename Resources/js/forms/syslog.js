/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // syslog.html
    if ($('#syslog_settings').length) {
        $('#sl_proto').change(function() {
            var protocol = parseInt($(this).val());
            if (protocol == 3) {
                $('#syslog_settings').show();
                $('#sl_port').attr('placeholder', 6514);
            }
            else if (protocol != 0) {
                $('#syslog_settings').show();
                $('#sl_port').attr('placeholder', 514);
            } else {
                $('#syslog_settings').hide();
            }
        }).trigger('change');
    }
});
