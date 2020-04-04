/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // mqtt.html
    if ($('#mqtt_settings').length) {
        function mqtt_change() {
            var port = parseInt($('#mqtt_port').val());
            if ($('#mqtt_enabled').val() == "1") {
                $('#mqtt_port').attr('placeholder', '1883');
                if (port == 1883) {
                    $('#mqtt_port').val('');
                }
                $('#mqtt_settings').show();
            } else if ($('#mqtt_enabled').val() == "2") {
                $('#mqtt_port').val('placeholder', '8883');
                if (port == 8883) {
                    $('#mqtt_port').val('');
                }
                $('#mqtt_settings').show();
            } else {
                $('#mqtt_settings').hide();
            }
        }
        function mqtt_auto_discovery_change() {
            if ($('#mqtt_auto_discovery').val() == "1") {
                $('#mqtt_discovery').show()
            } else {
                $('#mqtt_discovery').hide()
            }
        }
        $('#mqtt_enabled').change(mqtt_change);
        mqtt_change();
        $('#mqtt_auto_discovery').change(mqtt_auto_discovery_change);
        mqtt_auto_discovery_change();
    }
});
