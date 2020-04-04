/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // serial2tcp.html
    if ($('#serial2tcp_settings').length) {
        function serial2tcp_enabled_changed() {
            if ($('#serial2tcp_enabled').val() == "1") {
                $('.serial2tcp_client_settings').hide();
                $('.serial2tcp_server_settings').show();
            } else if ($('#serial2tcp_enabled').val() == "2") {
                $('.serial2tcp_client_settings').hide();
                $('.serial2tcp_server_settings').show();
            } else if ($('#serial2tcp_enabled').val() == "3") {
                $('.serial2tcp_server_settings').hide();
                $('.serial2tcp_client_settings').show();
            } else if ($('#serial2tcp_enabled').val() == "4") {
                $('.serial2tcp_server_settings').hide();
                $('.serial2tcp_client_settings').show();
            } else {
                $('.serial2tcp_settings').hide();
                $('.serial2tcp_server_settings').hide();
                $('.serial2tcp_client_settings').hide();
            }
            serial2tcp_port_changed();
            serial2tcp_auth_mode_changed();
        }
        function serial2tcp_port_changed() {
            if ($('#serial2tcp_serial_port').val() == "0") {
                $('.serial2tcp_custom').hide();
            } else if ($('#serial2tcp_serial_port').val() == "1") {
                $('.serial2tcp_custom').hide();
            } else if ($('#serial2tcp_serial_port').val() == "2") {
                $('.serial2tcp_custom').show();
            }
        }
        function serial2tcp_auth_mode_changed() {
            if ($('#serial2tcp_auth').val() == "0") {
                $('.serial2tcp_authentication').hide();
            } else if ($('#serial2tcp_auth').val() == "1") {
                $('.serial2tcp_authentication').show();
            }
        }
        $('#serial2tcp_enabled').change(serial2tcp_enabled_changed);
        $('#serial2tcp_serial_port').change(serial2tcp_port_changed);
        $('#serial2tcp_auth').change(serial2tcp_auth_mode_changed);
        serial2tcp_enabled_changed();
        serial2tcp_port_changed();
        serial2tcp_auth_mode_changed();
        $('form').on('submit', function() {
            $('#serial2tcp_auto_connect').val($('#_serial2tcp_auto_connect').prop('checked') ? '1' : '0');
            $('#serial2tcp_auto_reconnect').val($('#_serial2tcp_auto_reconnect').prop('checked') ? '1' : '0');
        });

    }
});
