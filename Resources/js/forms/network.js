/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // network.html
    if ($('#network_settings').length) {
        $('#st_dhcp').off('change').on('change', function() {
            form_set_disabled($('#st_ip,#st_subnet,#st_gw,#st_dns1,#st_dns2'), parseInt($(this).val()) != 0);
        }).trigger('change');
        $('#ap_dhcpd').on('change', function() {
            form_set_disabled($('#ap_dhcpds,#ap_dhcpde'), parseInt($(this).val()) == 0);
        }).trigger('change');
    }
});
