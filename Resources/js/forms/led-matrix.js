/**
 * Author: sascha_lammers@gmx.de
 */

 $.initFormLedMatrixFunc = function() {
    if ($('#collapse-rainbow').length) {
        $.initFormLedMatrixFunc();
    }
 };

 $(function() {
    // clock.html
    if ($('#led-matrix-settings').length) {
        $('#posix_tz').on('change', function() {
            $('#pes').on('change', function(e) {
                var self = $(this);
                var valueStr = self.val();
	            var value = parseInt(valueStr * 1.0);
                if (valueStr === '') {
                    e.preventDefault();
                    return;
                }
                if (isNaN(value) || value <= 1) {
                    window.setTimeout(function() {
                        self.val(0).trigger('change');
                    }, 1);
                    e.preventDefault();
                    return;
                }
                self.val(value);
            });
        });
    }
});
