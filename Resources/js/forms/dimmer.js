/**
 * Author: sascha_lammers@gmx.de
 */

 $(function() {
    // dimmer.html
    if ($('#dimmer_settings').length) {
        $('#lptime').change(function() {
            var items = $('#sstep').closest('.input-group').find('input');
            if ($(this).val() == 0) {
                form_set_disabled(items, true);
            }
            else {
                form_set_disabled(items, false);
            }
        }).trigger('change');
    }
});
