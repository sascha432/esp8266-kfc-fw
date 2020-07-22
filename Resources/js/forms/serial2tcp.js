/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // serial2tcp.html
    if ($('#serial2tcp_settings').length) {
        // function serial2tcp_mode_changed() {
        //     var val = parseInt($(this).val());
        //     if (val == 1) {
        //         $('.serial2tcp_client_settings').hide();
        //         $('.serial2tcp_server_settings').show();
        //     } else if (val == 2) {
        //         $('.serial2tcp_client_settings').hide();
        //         $('.serial2tcp_server_settings').show();
        //     } else {
        //         $('.serial2tcp_settings').hide();
        //         $('.serial2tcp_server_settings').hide();
        //         $('.serial2tcp_client_settings').hide();
        //     }
        //     serial2tcp_port_changed();
        //     serial2tcp_auth_mode_changed();
        // }
        // function serial2tcp_serial_port_changed() {
        //     var val = parseInt($(this).val());
        //     if (val == 0) {
        //         $('.scustom').hide();
        //     } else if (val == 1) {
        //         $('.scustom').hide();
        //     } else if (val == 2) {
        //         $('.scustom').show();
        //     }
        // }
        // function serial2tcp_auth_mode_changed() {
        //     var val = parseInt($(this).val());
        //     if (val == 0) {
        //         $('#auth_group').hide();
        //     } else if (val != 0) {
        //         $('#auth_group').show();
        //     }
        // }
        // $('#mode').change(serial2tcp_mode_changed);
        // $('#s_port').change(serial2tcp_serial_port_changed);
        // $('#authg').change(serial2tcp_auth_mode_changed);
        // serial2tcp_mode_changed();
        // serial2tcp_serial_port_changed();
        // serial2tcp_auth_mode_changed();
        // // $('form').on('submit', function() {
        // //     $('#serial2tcp_auto_connect').val($('#_serial2tcp_auto_connect').prop('checked') ? '1' : '0');
        // //     $('#serial2tcp_auto_reconnect').val($('#_serial2tcp_auto_reconnect').prop('checked') ? '1' : '0');
        // // });

    }
});
