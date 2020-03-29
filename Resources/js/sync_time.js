/**
 * Author: sascha_lammers@gmx.de
 */

if ($('#system_time').length) {
    var h, m, s;
    var system_time = $('#system_time');
    function parse_time(system_time) {
        var time = system_time.html().split(':');
        h = parseInt(time[0]);
        m = parseInt(time[1]);
        s = parseInt(time[2]);
    }
    function fmt(value) {
        var str = '' + value;
        if (str.length == 1) {
            return '0' + str;
        }
        return str;
    }
    function sync_system_date() {
        if ($('#system_date').length) {
            var system_date = $('#system_date');
            $.get('/sync_time', function(data) {
                system_date.html(data);
                system_time = $('#system_time');
                parse_time(system_time);
            });
        }
    }
    parse_time(system_time);
    window.setInterval(function() {
        if (++s == 60) {
            s = 0;
            sync_system_date();
            if (++m == 60) {
                m = 0;
                sync_system_date(); // sync date/time once an hour
                if (++h == 24) {
                    h = 0;
                }
            }
        }
        system_time.html(fmt(h) + ':' + fmt(m) + ':' + fmt(s));
    }, 1000);
}
