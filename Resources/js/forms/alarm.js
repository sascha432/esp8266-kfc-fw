/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // alarm.html
    if ($('#alarm_settings').length) {

        var max_alerts = 10;
        var last_enabled = 2; // show at least 3 alarms
        var prefix = 'alarm_';
        var id_prefix = '#' + prefix;

        var prototype = $(id_prefix + 'prototype').html();
        $(id_prefix + 'prototype').remove();
        var container = $(id_prefix + 'container');

        var i;
        var hours = '';
        var minutes = '';
        for(i = 0; i < 60; i++) {
            var s = i;
            if (s < 10) {
                s = '0' + s;
            }
            if (i < 24) {
                hours += '<option value="' + i + '">' + s + '</option>';
            }
            minutes += '<option value="' + i + '">' + s + '</option>';
        }

        for(i = 0; i < max_alerts; i++) {
            var tmp = prototype.replace('$$H$$', hours).replace('$$M$$', minutes).replace(/\$\$/g, i);
            var tmp = container.append('<div id="' + prefix + i + '_container">' + tmp + '</div>').find(id_prefix + i + '_container');
            var data = window.alarm_values_array[i];
            // store local variables
            (function() {
                var id_prefix_num = id_prefix + i;
                var repeat = $(id_prefix_num + '_repeat');
                var time = repeat.find('.time');
                var once = repeat.find('.once');
                var enabled_input = $(id_prefix_num + '_enabled')
                var enabled = parseInt(data.e) ? 1 : 0;
                var weekdays = parseInt(data.w) || 0;
                function auto_enable() {
                    enabled_input.val(1).trigger('change');
                }
                function update_repeat() {
                    time.remove();
                    if (weekdays || !enabled) {
                        once.hide();
                    } else {
                        once.show();
                    }
                }
                // weekdays check box buttons with single hidden input field
                var input = id_prefix_num + '_weekdays';
                $(input).val(weekdays);
                tmp.find('.alarm-week-day-button').each(function(n) {
                    var mask = (1 << n);
                    var btn = $(this);
                    btn.on('click', function() {
                        weekdays = weekdays ^ mask;
                        $(input).val(weekdays);
                        btn.removeClass('btn-primary btn-default').addClass('btn-' + (weekdays & mask ? 'primary' : 'default'));
                        update_repeat();
                        auto_enable();
                    })
                    btn.addClass('btn-' + (weekdays & mask ? 'primary' : 'default'));
                });
                // other fields
                $(id_prefix_num + '_hour').val(data.h);
                $(id_prefix_num + '_minute').val(data.m);
                $(id_prefix_num + '_duration').val(data.d);
                if (enabled && weekdays == 0) {
                    repeat.find('.time').html(' ' + data.ts).show();
                } else {
                    time.remove();
                }
                enabled_input.val(enabled).rangeslider({
                    polyfill : false
                }).on('change', function() {
                    enabled = $(this).val() == 1;
                    update_repeat();
                });
                // add auto enable
                tmp.find('.alarm-auto-enable').on('change', auto_enable);
                // find last enabled alarm
                if (i > last_enabled && enabled) {
                    last_enabled = i;
                }
            })();
        }

        // hide all disabled alarms after the last enabled one
        last_enabled++;
        for(i = last_enabled; i < max_alerts; i++) {
            $(id_prefix + i + '_container').hide();
        }

        // add button to add more of the disabled alarms
        if (last_enabled < max_alerts) {
            $('#add_alarm_btn').show().on('click', function() {
                $(id_prefix + 'container').find('.row').show();
                $(id_prefix + last_enabled + '_container').show().find('.row:visible:last').hide();
                if (last_enabled < max_alerts) {
                    if (++last_enabled == max_alerts) {
                        $(this).hide(); // hide button if none left
                    }
                }
            });
        }

        // wait for form to load before showing its contents
        $('#spinner_container').hide();
        container.show().find('.row:visible:last').hide();
    }
});
