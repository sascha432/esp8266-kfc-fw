/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
	if ($('#clock_settings').length) {
        $('#dis_auto_br').off('click').on('click', function() {
            $('#auto_br').val(-1);
        });
        function load_sensor_value() {
            $.get('/ambient_light_sensor?SID=' + $.getSessionId(), function (value) {
                value = parseInt(value);
                if (value > 0) {
                    $('#abr_sv').html('Sensor&nbsp;<strong>' + value + '</strong>');
                }
                window.setTimeout(load_sensor_value, 2000);
            }).fail(function(jqXHR, textStatus, error) {
                dbg_console.error('failed to load sensor value ', error);
                window.setTimeout(load_sensor_value, 30000);
            });
        }
        window.setTimeout(load_sensor_value, 2000);
    }
});
