/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
	if ($('#clock_settings').length) {
        $('#dis_auto_br').off('click').on('click', function() {
            $('#auto_br').val(-1);
        });
        function load_sensor_value() {
            var SID = $.getSessionId();
            $.get('/ambient_light_sensor?SID=' + SID + '&id=' + random_str(), function (value) {
                value = parseInt(value);
                if (value > 0) {
                    $('#abr_sv').html('Sensor&nbsp;<strong>' + value + '</strong>');
                }
                window.setTimeout(load_sensor_value, 15000);
            });
        }
        window.setTimeout(load_sensor_value, 1000);
    }
});
