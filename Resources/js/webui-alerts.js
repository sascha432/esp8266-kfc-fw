/**
 * Author: sascha_lammers@gmx.de
 */

 $.WebUIAlerts = {
    enabled: $('#webui-alerts-enabled').length!=0,
    locked: false,
    console: window.console,
    count: 0,
    icon: false,
    ignore_cookie: false,
    cookie_name: 'webui_hide_alerts',
    container: null,
    alert_html: null,
    next_alert_id: 1,
    default_alert_poll_time: 15000,
    alert_poll_time: 15000,
    alert_poll_time_on_error: 30000,
    iconize: function() {
        if (this.enabled) {
            this.icon = true;
            this.ignore_cookie = true;
        }
    },
    update: function() {
        this.count = this.container.find('.alert').length;
        var icons = $('nav').find('.alerts-count')
        icons.html(this.count);
        icons = icons.parent();
        var icons_visible = icons.is(':visible');
        var container_parent = this.container.parent();
        var container_visible = container_parent.is(':visible');
        this.console.log("visible", container_visible, icons_visible, this.icon, this.count);
        if (this.count && this.icon && !icons_visible) {
            this.console.log("show icons");
            icons.clearQueue().stop().fadeIn();
        }
        else if ((!this.icon && icons_visible) || this.count == 0) {
            this.console.log("hide icons");
            icons.clearQueue().stop().fadeOut();
        }
        if (this.count && !this.icon && !container_visible) {
            this.console.log("show container");
            container_parent.clearQueue().stop().show();
        }
        else if ((this.icon && container_visible) || this.count == 0) {
            this.console.log("hide container");
            container_parent.clearQueue().stop().fadeOut(750);
            this.hide_container_toggle(-1);
        }
    },
    init_container: function() {
        if (!this.enabled) {
            return;
        }
        this.container = $('#alert-container');
        if (this.container.length == 0) {
            this.console.error('#alert-container missing');
            return;
        }
        var prototype = this.container.find('div[role=alert]:first');
        if (prototype.length) {
            this.alert_html = prototype.clone()[0].outerHTML; //.wrap('<div id="alert-prototype"></div>').parent().html();
            if (prototype.hasClass('hidden')) {
                prototype.remove();
            }
            var tmp = $(this.alert_html);
            tmp.find('.alert-content').html('No Message');
            this.alert_html = tmp[0].outerHTML;
        }
        this.data_url = '/alerts?SID=' + $.getSessionId();
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
            self.write_cookie();
            self.update();
        });
        $('span.alerts-count').parent().off('click').on('click', function(e) {
            e.preventDefault();
            self.icon = false;
            self.write_cookie();
            self.update();
        });
        $('#remove-all-alerts').on('click', function(e) {
            $.get(self.data_url + '&clear=1', function(text) {
                //TODO alerts are displayed until the page is reloaded
            });
            e.preventDefault();
        });
        this.read_cookie();
        this.get_json();
    },
    read_cookie: function() {
        if (!this.ignore_cookie) {
            this.icon = Cookies.get(this.cookie_name) == 1;
        }
    },
    write_cookie: function() {
        if (!this.ignore_cookie) {
            Cookies.set(this.cookie_name, this.icon == true ? 1 : 0);
        }
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
    schedule_get_json: function(timeout) {
        if (timeout === undefined) {
            timeout = this.alert_poll_time;
        }
        if (timeout == 0) {
            return;
        }
        window.setTimeout(this.get_json.bind(this), timeout);
    },
    get_json: function() {
        if (!this.enabled) {
            return;
        }
        if (this.locked) {
            window.setTimeout(this.schedule_get_json.bind(this), this.alert_poll_time_on_error * 2);
            return;
        }
        this.locked = true;
        var self = this;
        $.get(this.data_url + '&poll_id=' + this.next_alert_id, function(text) {
            self.locked = false;
            if (text == undefined || text == '') { // text is undefined for response code 204
                self.schedule_get_json();
                return;
            }
            var data;
            try {
                try {
                    data = JSON.parse(text);
                } catch(e) {
                    // console.log('json parse failed, trying to filter non ascii characters', e, text);
                    var filtered = '';
                    for(var i = 0; i < text.length; i++) {
                        var char = text.charCodeAt(i);
                        if (char >= 32 && char < 128) {
                            filtered += String.fromCharCode(char);
                        }
                    }
                    data = JSON.parse(filtered);
                }
            } catch(e) {
                console.log(e, text);
                self.schedule_get_json(self.alert_poll_time_on_error);
                return;
            }
            if (typeof data === 'object' && data.length) {
                var remove = {};
                // get alerts and update next_alert_id
                var alerts = data.filter(function(value, index, arr) {
                    if (value['i'] === undefined) {
                        return false;
                    }
                    var i = parseInt(value['i']);
                    if (i < self.next_alert_id) {
                        return false;
                    }
                    if (value['d'] !== undefined) {
                        var d = parseInt(value['d']);
                        if (d > 0) {
                            remove[d] = d;
                        }
                    }
                    if (i >= self.next_alert_id) {
                        self.next_alert_id = i + 1;
                    }
                    return true;
                });
                // console.log('alerts', alerts, 'remove', remove);

                // remove alerts that have been marked as deleted but keep the delete marked
                // to remove existing alerts
                alerts = alerts.filter(function(value, index, arr) {
                    return (value['d'] !== undefined || !remove.hasOwnProperty(value['i']));
                });

                $(alerts).each(function(key, val) {
                    self.add(val);
                });
                self.update();

                var animate = self.container.find('.alert-animate');
                $(animate.get().reverse()).each(function() {
                    self.container.prepend($(this).closest('.alert').detach());
                }).removeClass('alert-animate').
                    css('color', 'red').
                    stop().
                    clearQueue().
                    fadeTo(500, 0.5).fadeTo(500, 1).
                    fadeTo(500, 0.5).fadeTo(500, 1).
                    fadeTo(500, 0.5).fadeTo(500, 1, function() {
                        $(this).css('color','');
                    }
                );
            }
            self.schedule_get_json();

        }, 'text').fail(function(error) {
            self.locked = false;
            if (error.status == 503) {
                console.log("WebUIAlerts disabled");
            }
            else {
                console.error('webui get json failed', error);
                self.schedule_get_json(self.alert_poll_time_on_error);
            }
        });
    },
    add: function(data) {
        // data[i]=id,[m]=message,[t]=type,[ts]=unix timestamp

        if (data['d'] !== undefined) {
            $('#webui-alert-id-' + data['d']).remove();
            return;
        }

        var self = this;
        var i = parseInt(data['i']);
        // check if we have the alert already
        var alert = $('#webui-alert-id-' + i);
        if (alert.length) {
            return;
        }

        // check for duplicates
        this.container.find('.alert-content').each(function(key, val) {
            if (data !== null && $(this).html() == data['m']) {
                var alert = $(this).closest('.alert');
                var list = alert.data('alert-ids');
                for (key in list) {
                    if (list[key] == i) {
                        return;
                    }
                }
                n = list.length;
                list.push(i);
                alert.find('.alert-extra-content').addClass('alert-animate').html('Message repeated ' + ((n == 1) ? 'once' : (n + ' times')) + (data['ts'] ? (', last ' + (new Date(data['ts'] * 1000)).toLocaleString()) : ''));
                data = null;
                // if (!self.container.find('.alert:first').is(alert)) {
                //     self.container.prepend(alert.detach());
                // }
            }
        });
        if (data === null) {
            return;
        }

        // create new alert
        alert = $(this.alert_html);
        alert.attr('class', 'alert fade show alert-' + data['t'] + ' alert-new');
        alert.attr('id', 'webui-alert-id-' + i);
        alert.data('alert-ids', [i]);
        alert.find('.alert-content').html(data['m']);
        if (data['ts']) {
            alert.find('.alert-extra-content').append((new Date(data['ts'] * 1000)).toLocaleString());
        }

        alert.find('.close').addClass('alert-dismissible').off('click').on('click', function(e) {
            e.preventDefault();
            $.get('/alerts?dismiss_id=' + alert.data('alert-ids').join(','), function(data) {});
            alert.remove();
            self.update();
        });
        this.container.prepend(alert);
    }
};
dbg_console.register('$.WebUIAlerts', $.WebUIAlerts);

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
