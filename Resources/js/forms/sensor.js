/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
	if ($('#heading-alscfg').length && $('#dis_auto_br').length) {
        $('#dis_auto_br').off('click').on('click', function() {
                $('#auto_br').val(-1);
        });
        var sensor_id = parseInt($('#dis_auto_br').attr('data-sensor-id'));
        function load_sensor_value() {
            $.get('/ambient_light_sensor?SID=' + $.getSessionId() + '&id=' + sensor_id, function (value) {
                value = parseInt(value);
                if (value > 0) {
                    $('#abr_sv').html('Sensor&nbsp;<strong>' + value + '</strong>');
                }
                window.setTimeout(load_sensor_value, 2000);
            }).fail(function(jqXHR, textStatus, error) {
                console.error('failed to load sensor value ', error);
                window.setTimeout(load_sensor_value, 30000);
            });
        }
        window.setTimeout(load_sensor_value, 2000);
    }
});
