/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // remote.html
    if ($('#remote_settings').length) {
        function http_enabled_changed() {
            var port = parseInt($('#http_port').val());
            var type = parseInt($('#http_enabled').val());
            if (type == 1) {
                $('#http_port').attr('placeholder', '80');
                if (port == 443 || port == 80) {
                    $('#http_port').val('');
                }
            } else if (type == 2) {
                $('#http_port').attr('placeholder', '443');
                if (port == 80 || port == 443) {
                    $('#http_port').val('');
                }
            }
        }
        $('#http_enabled').change(http_enabled_changed);
        $('#http_port').change(http_enabled_changed);
        http_enabled_changed();
    }
});
