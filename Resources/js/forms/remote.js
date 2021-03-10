/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // remote.html
    if ($('#remote_settings').length) {
        function httpmode_changed() {
            var port = parseInt($('#port').val());
            var type = parseInt($('#mode').val());
            if (type == 1) {
                $('#disable-warning').addClass('hidden');
                form_set_disabled($('#ssl_cert,#ssl_key,#port'), false)
                $('#port').attr('placeholder', '80');
                if (port == 443 || port == 80 || port == 0) {
                    $('#port').val('');
                }
            } else if (type == 2) {
                $('#disable-warning').addClass('hidden');
                form_set_disabled($('#ssl_cert,#ssl_key,#port'), false)
                $('#port').attr('placeholder', '443');
                if (port == 80 || port == 443 || port == 0) {
                    $('#port').val('');
                }
            } else {
                $('#disable-warning').removeClass('hidden');
                form_set_disabled($('#ssl_cert,#ssl_key,#port'), true)
            }
        }
        $('#mode,#port').change(httpmode_changed);
        httpmode_changed();
    }
});
