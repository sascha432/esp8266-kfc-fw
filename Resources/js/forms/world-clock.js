/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // world-clock.html
    if ($('#w_clock').length) {
        $('.posix_tz').on('change', function() {
            var tz_name = $(this).closest('.form-group').find("input[type='hidden']");
            tz_name.val($(this).find('option:selected').text());
        });
    }
});
