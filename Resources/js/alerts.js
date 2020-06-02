/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {

    var container = $('#alert_container');
    var alert = container.html();
    var nextAlertId = 1;
    var alertPollTime = 10000;
    function show_alerts() {
        if (nextAlertId == 1) {
            nextAlertId = 2;
            container.removeClass('hidden');
        }
    }
    function add_alert(val) {
        console.log(val);
        // val[i]=id,[m]=message,[t]=type,[n=true]=no dismiss
        show_alerts();
        if (val['i'] >= nextAlertId) {
            nextAlertId = val['i'] + 1;
        }
        var new_alert = $(alert);
        new_alert.removeClass('hidden').addClass('fade show alert-' + val['t']);
        new_alert.find('.alert-content').html(val['m']);
        var close = new_alert.find('.close');
        if (val['n']) {
            close.remove();
        } else {
            close.data('alert-id', val['i']).on('click', function() {
                $.get('/alerts?id=' + $(this).data('alert-id'), function(data) {
                });
            });
        }
        container.append(new_alert);
    }
    function poll_alerts() {
        $.get('/alerts?poll_id=' + nextAlertId, function(data) {
            console.log(data);
            // $(json).each(function(key, val) {
            //     add_alert(val);
            // });
        }).always(function() {
            window.setTimeout(poll_alerts, alertPollTime);
        });
    }
    if (window.alerts && window.alerts.length) {
        $(window.alerts).each(function(key, val) {
            add_alert(val);
        });
    }
    window.setTimeout(poll_alerts, alertPollTime);

});
