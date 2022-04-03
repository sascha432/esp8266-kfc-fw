/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // network.html
    if ($('#network_settings').length) {
        function update_fields(num) {
            var update_fields1 = function() {
                var state = $('#en_' + num).val() == 1 ? false : true;
                $('#prio_' + num).prop('disabled', state).next('input').prop('disabled', state);
                $('#ssid_' + num + ',#pass_' + num + ',#dhcp_' + num).prop('disabled', state);
                if (!state) {
                    state = $('#dhcp_' + num).val() == 1 ? true : false;
                }
                $('#ip_' + num + ',#sn_' + num + ',#gw_' + num + ',#dns1_' + num + ',#dns2_' + num).prop('disabled', state);
            };
            $('#en_' + num).on('change', function() {
                    update_fields1();
                }
            );
            $('#_en_' + num).on('click', function() {
                    update_fields1();
                }
            );
            $('#dhcp_' + num).on('change', function() {
                    update_fields1();
                }
            );
            update_fields1();
        }
        for(var n = 0; n < 8; n++) {
            if ($('#en_' + n).length) {
                update_fields(n);
            }
        }
        $("input[name^='dns1_'],input[name^='dns2_']").each(function() {
            var val = $(this).val();
            if (val === '(IP unset)' || val === '0.0.0.0') {
                $(this).val('');
            }
        });
    }
});
