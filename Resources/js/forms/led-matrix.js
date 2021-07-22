/**
 * Author: sascha_lammers@gmx.de
 */

 $.initFormLedMatrixFunc = function() {
    var items = {
        0: [ 'rb_mul', 'rb_incr', 'rb_min', 'rb_max', 'rb_sp', 'rb_cf', 'rb_mv', 'rb_cre', 'rb_cgr', 'rb_cbl' ],
        1: [ 'rb_bpm', 'rb_hue' ]
    }
    $('#rb_mode').on('change', function() {
        var value = parseInt($(this).val());
        var enable = value == 0 ? 0 : 1;
        var disable = value == 0 ? 1 : 0;
        var list = items[enable];
        for (var i = 0; i < list.length; i++) {
            $('#' + list[i]).closest('.form-group').show();
        }
        list = items[disable];
        for (var i = 0; i < list.length; i++) {
            $('#' + list[i]).closest('.form-group').hide();
        }
    }).trigger('change');
 };

if ($('#collapse-rainbow').length) {
    $.initFormLedMatrixFunc();
}
