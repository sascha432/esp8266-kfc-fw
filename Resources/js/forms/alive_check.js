/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
	if ($('#alive_check').length) {
		function checkIfAvailable() {
			$.get('/is_alive?p=' + Math.floor(Math.random() * 100000), function() {
                var target = $('#alive_check').attr('href');
                if (target) {
                    window.location = target;
                } else {
                    window.location = '/index.html';
                }
			});
		}
		$.ajaxSetup({
            error: function(x, e) {
				window.setTimeout(checkIfAvailable, 1000);
            }
        });
        window.setTimeout(checkIfAvailable, 3000);
        window.setTimeout(function() {
            $('#alive_check').fadeIn(2000);
        }, 10000);
    }
});
