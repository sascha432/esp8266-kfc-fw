/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // network.html
    if ($('#network_settings').length) {
        function set_items(root, ex_id, state) {
            root.each(function() {
                var id = $(this).attr('id');
                if (id != ex_id) {
                    var element = $('#' + id);
                    if (state) {
                        element.attr('disabled', 'disabled');
                        element.prop('disabled', true);
                    } else {
                        element.removeAttr('disabled');
                        element.prop('disabled', false);
                      }
                }
            });
        }
        $('#ap_dhcpd').on('change', function() {
            var group = $(this).closest('.card-body');
            set_items(group.find('input,select,button'), $(this).attr('id'), parseInt($(this).val()) == 0);
        }).trigger('change');
        $('#st_dhcp').on('change', function() {
            var group = $(this).closest('.card-body');
            set_items(group.find('input,select,button'), $(this).attr('id'), parseInt($(this).val()) == 0);
        }).trigger('change');
    }
});
