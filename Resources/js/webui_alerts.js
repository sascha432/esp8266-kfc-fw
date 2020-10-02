/**
 * Author: sascha_lammers@gmx.de
 */

 $.WebUIAlerts = {
    count: 0,
    icon: false,
    cookie_name: 'webui_hide_alerts',
    alerts: [],
    container: null,
    alert_html: null,
    next_alert_id: 1,
    alert_poll_time: 30000,
    alert_poll_time_on_error: 60000,
    update: function() {
        this.count = this.container.find('.alert').length;
        var icons = $('nav').find('.alerts-count')
        icons.html(this.count);
        icons = icons.parent();
        var icons_visible = icons.is(':visible');
        var container_parent = this.container.parent();
        var container_visible = container_parent.is(':visible');
        dbg_console.log("visible", container_visible, icons_visible, this.icon, this.count);
        if (this.count && this.icon && !icons_visible) {
            dbg_console.log("show icons");
            icons.clearQueue().stop().fadeIn();
        }
        else if ((!this.icon && icons_visible) || this.count == 0) {
            dbg_console.log("hide icons");
            icons.clearQueue().stop().fadeOut();
        }
        if (this.count && !this.icon && !container_visible) {
            dbg_console.log("show container");
            container_parent.clearQueue().stop().show();
        }
        else if ((this.icon && container_visible) || this.count == 0) {
            dbg_console.log("hide container");
            container_parent.clearQueue().stop().fadeOut(750);
            this.hide_container_toggle(-1);
        }
    },
    init_container: function() {
        this.container = $('#alert-container');
        if (this.container.length == 0) {
            dbg_console.error('#alert-container missing');
            return;
        }
        var prototype = this.container.find('div[role=alert]:first');
        if (prototype.length) {
            this.alert_html = prototype.clone().wrap('<div id="alert-prototype"></div>').parent().html();
            if (prototype.hasClass('hidden')) {
                prototype.remove();
            }
        }
        var self = this;
        var hide_container = $('#hide-alert-container');
        this.container.on('mouseenter', function() {
            self.hide_container_toggle(1);
        }).on('mouseleave', function() {
            self.hide_container_toggle(false);
        });
        hide_container.on('mouseenter', function() {
            self.hide_container_toggle(true);
        }).on('mouseleave', function() {
            self.hide_container_toggle(false);
        }).on('click', function() {
            self.icon = true;
            Cookies.set(self.cookie_name, 1);
            self.update();
        });
        $('span.alerts-count').parent().off('click').on('click', function(e) {
            e.preventDefault();
            self.icon = false;
            Cookies.set(self.cookie_name, 0);
            self.update();
        })
        this.alerts = window.webui_alerts_data;

        this.icon = Cookies.get(this.cookie_name) != 0;
        if (this.alerts.length) {
            $(this.alerts).each(function(key, val) {
                self.add(val);
            });
            this.update();
        }
        window.setTimeout(function() { self.poll(); }, this.alert_poll_time);
    },
    hide_container_toggle: function(state) {
        var hide_container = $('#hide-alert-container');
        hide_container.clearQueue();
        hide_container.stop();
        if (state === -1) {
            hide_container.fadeOut(750);
        } else if (state == 1) {
            hide_container.delay(250).fadeIn(500);
        } else if (state) {
            hide_container.fadeIn(500);
        } else {
            hide_container.delay(1500).fadeOut(1500);
        }
    },
    add: function(data) {
        // data[i]=id,[m]=message,[t]=type,[n=true]=no dismiss
        var i = parseInt(data['i']);
        if (i >= this.next_alert_id) {
            this.next_alert_id = i + 1;
        }
        dbg_console.log('add', data, 'next', this.next_alert_id);

        var alert = $('#webui-alert-id-' + i);
        dbg_console.log(alert,'#webui-alert-id-' + i);
        if (alert.length == 0) {
            alert = $(this.alert_html);
        }
        alert.attr('class', 'alert fade show alert-' + data['t']);
        alert.attr('id', 'webui-alert-id-' + i);
        alert.data('alert-id', i);
        alert.find('.alert-content').html(data['m']);
        var close = alert.find('.close');
        if (data['n']) {
            close.remove();
        } else {
            alert.addClass('alert-dismissible');
            var self = this;
            close.data('alert-id', data['i']).off('click').on('click', function(e) {
                e.preventDefault();
                $.get('/alerts?id=' + $(this).data('alert-id'), function(data) {
                });
                $(this).closest('.alert').remove();
                self.update();
            });
        }
        this.container.append(alert);
    },
    poll: function() {
        dbg_console.log('poll', this.next_alert_id, 'this', this);
        var self = this;
        $.get('/alerts?poll_id=' + this.next_alert_id, function(data) {
            dbg_console.log('get', data);
            $(data).each(function(key, val) {
                self.add(val);
            });
            self.update();
            window.setTimeout(function() { self.poll(); }, self.alert_poll_time);
        }, 'json').fail(function(error) {
            dbg_console.error('webui alerts poll failed', error);
            window.setTimeout(function() { self.poll(); }, self.alert_poll_time_on_error);
        });
    }
};

$('.generate-bearer-token').on('click', function(e) {
    e.preventDefault();
    var len = parseInt($(this).data('len'));
    if (len == 0) {
        len = 32;
    }
    $($(this).data('for')).val($.base64Encode(String.fromCharCode.apply(String, $.getRandomBytes(len))));
});

$(function() {
    $.WebUIAlerts.init_container();
});
