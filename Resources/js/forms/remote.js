/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // remote.html
    if ($('#remote_settings').length) {
        function httpmode_changed() {
            var port = parseInt($('#httport').val());
            var type = parseInt($('#httpmode').val());
            if (type == 1) {
                $('#http_settings').show();
                $('#ssl_settings').hide();
                $('#httport').attr('placeholder', '80');
                if (port == 443 || port == 80) {
                    $('#httport').val('');
                }
            } else if (type == 2) {
                $('#http_settings').show();
                $('#ssl_settings').show();
                $('#httport').attr('placeholder', '443');
                if (port == 80 || port == 443) {
                    $('#httport').val('');
                }
            } else {
                $('#http_settings').hide();
            }
        }
        $('#httpmode').change(httpmode_changed);
        $('#httport').change(httpmode_changed);
        httpmode_changed();
    }
});
