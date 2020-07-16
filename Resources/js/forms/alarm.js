/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // alarm.html
    if ($('#alarm_settings').length) {

        var max_alerts = 10;
        var last_enabled = 2; // show at least 3 alarms

        function check_days(num) {
            var count = 0;
            for (j = 0; j < 7; j++) {
                var checkbox_id = '#_alarm_' + num + '_weekday_' + j;
                if ($(checkbox_id).is(':checked')) {
                    count++;
                }
            }
            return count;
        }

        function update_one_time_alert(num, init) {
            // check if any days are checked and the alarm is enabled
            var count = check_days(num);
            var enabled = parseInt($('#alarm_' + num + '_enabled').val());
            // if not, display one time alert
            var element = $('#alarm_' + num + '_repeat');
            if (count == 0 && enabled == 1) {
                element.show();
                if (!init) {
                    element.find('.time').remove();
                }
            }
            else {
                element.hide();
            }
        }

        var i, j;
        for(i = 0; i < max_alerts; i++) {
            // update checkboxes from hidden field
            var var_id = '#alarm_' + i + '_weekdays';
            var value = parseInt($(var_id).val());
            for (j = 0; j < 7; j++) {
                var checkbox_id = '#_alarm_' + i + '_weekday_' + j;
                if (value & (1 << j)) {
                    // check box and trigger change handler for button-checkbox
                    $(checkbox_id).prop('checked', true).triggerHandler('change');
                }
            }
            if (i > last_enabled && parseInt($('#alarm_' + i + '_enabled').val())) {
                last_enabled = i;
            }
            update_one_time_alert(i, true);
        }

        // hide all disabled alarms
        last_enabled++;
        for(i = last_enabled; i < max_alerts; i++) {
            $('#alarm_' + i + '_container').hide();
        }

        if (last_enabled < max_alerts) {
            $('#add_alarm_btn').show().on('click', function() {
                $('#alarm_' + last_enabled + '_container').show();
                if (last_enabled < max_alerts) {
                    last_enabled++;
                    if (last_enabled == max_alerts) {
                        $(this).hide();
                    }
                }
            });
        }

        $('.button-checkbox input').on('change', function() {
            var items = $(this).attr('id').split('_');
            update_one_time_alert(items[2]);
        });

        $('.alarm-auto-enable').on('change', function() {
            var items = $(this).attr('id').split('_');
            var enable = $('#alarm_' + items[1] + '_enabled');
            $('#alarm_' + items[1] + '_repeat').find('.time').remove();
            if (parseInt(enable.val()) == 0) {
                enable.val(1).trigger('change');
            }
        });

        $('form').on('submit', function () {
            var i, j;
            // update hidden field from checkboxes
            for(i = 0; i < max_alerts; i++) {
                var value = 0;
                for (j = 0; j < 7; j++) {
                    var checkbox_id = '#_alarm_' + i + '_weekday_' + j;
                    value |= $(checkbox_id).prop('checked') ? (1 << j) : 0;
                }
                var var_id = '#alarm_' + i + '_weekdays';
                $(var_id).val(value);
            }
        });

        $('.alarm-enable-slider input').rangeslider({
            polyfill : false
        }).on('change', function() {
            var items = $(this).attr('id').split('_');
            update_one_time_alert(items[1]);
        });

        // wait for form to load before showing its contents
        $('#spinner_container').hide();
        $('#alarm_container').show();

    }
});
