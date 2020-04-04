/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // rebooting.html
	if ($('#rebooting_device').length) {
		function checkIfAvailable() {
			$.get('/is_alive?p=' + Math.floor(Math.random() * 100000), function() {
                var hash = $(location).attr('hash');
                if (hash.substr(0, 3) == "#u_") {
                    window.location = '/update_fw.html' + hash;
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
    }
});
