/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // network.html
    if ($('#network_settings').length) {
        function dhcp_change() {
            if ($('#dhcp_client').val() == "1") {
                $('#static_address').hide();
            } else {
                $('#static_address').show();
            }
        }
        $('#dhcp_client').change(dhcp_change);
        function dhcpd_change() {
            if ($('#softap_dhcpd').val() == "1") {
                $('#soft_ap_dhcpd').show();
            } else {
                $('#soft_ap_dhcpd').hide();
            }
        }
        $('#softap_dhcpd').change(dhcpd_change);
        dhcp_change();
        dhcpd_change();
        $('#safe_mode_reboot_time').on('blur', function() {
            var value = parseInt($(this).val());
            if (value == 0) {
                $(this).val('');
            }
        });
    }
});
