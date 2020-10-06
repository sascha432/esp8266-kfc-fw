/**
 * Author: sascha_lammers@gmx.de
 */

 $.WebUIAlerts = {
    console: window.console,
    count: 0,
    icon: false,
    cookie_name: 'webui_hide_alerts',
    container: null,
    alert_html: null,
    next_alert_id: 1,
    alert_poll_time: 5000,
    alert_poll_time_on_error: 30000,
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
            Cookies.set(self.cookie_name, 1);
            self.update();
        });
        $('span.alerts-count').parent().off('click').on('click', function(e) {
            e.preventDefault();
            self.icon = false;
            Cookies.set(self.cookie_name, 0);
        })

        this.icon = Cookies.get(this.cookie_name) == 1;
        this.get_json();
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
    get_json: function() {
        var self = this;
        $.get(this.data_url + '&poll_id=' + this.next_alert_id, function(data) {
            if (typeof data === 'object' && data.length) {
                var remove = {};
                // get alerts and update next_alert_id
                var alerts = data.filter(function(value, index, arr){
                    if (value['i'] === undefined) {
                        return false;
                    }
                    var i = parseInt(value['i']);
                    if (i < 1) {
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
            }
            window.setTimeout(function() { self.get_json(); }, self.alert_poll_time);

        }, 'json').fail(function(error) {
            console.log(arguments);
            if (error.status == 503) {
                console.log("WebUIAlerts disabled");
            }
            else {
                console.error('webui get json failed', error);
                window.setTimeout(function() { self.get_json(); }, self.alert_poll_time_on_error);
            }
        });
    },
    add: function(data) {
        // data[i]=id,[m]=message,[t]=type,[n=true]=no dismiss

        if (data['d'] !== undefined) {
            // this.console.log('remove', data, 'next', this.next_alert_id);
            $('#webui-alert-id-' + data['d']).remove();
            return;
        }
        //this.console.log('add', data, 'next', this.next_alert_id);

        // this.container.find('.alert-content').each(function(key, val) {
        //     if ($(this).html() == data['m']) {
        //         console.log($(this).html());
        //     }
        // });


        var i = parseInt(data['i']);
        var alert = $('#webui-alert-id-' + i);
        // this.console.log(alert,'#webui-alert-id-' + i);
        if (alert.length == 0) {
            alert = $(this.alert_html);
        }

        alert.attr('class', 'alert fade show alert-' + data['t']);
        alert.attr('id', 'webui-alert-id-' + i);
        alert.data('alert-id', i);
        alert.find('.alert-content').html(data['m']);
        var close = alert.find('.close');
        // if (data['n']) {
        //     close.remove();
        // } else
        {
            alert.addClass('alert-dismissible');
            var self = this;
            close.data('alert-id', data['i']).off('click').on('click', function(e) {
                e.preventDefault();
                $.get('/alerts?dismiss_id=' + $(this).data('alert-id'), function(data) {
                });
                $(this).closest('.alert').remove();
                self.update();
            });
        }
        this.container.prepend(alert);
    }
    // poll: function() {
    //     this.console.log('poll', this.next_alert_id, 'this', this);
    //     var self = this;
    //     $.get('/alerts?poll_id=' + this.next_alert_id, function(data) {
    //         self.console.log('get', data);
    //         $(data).each(function(key, val) {
    //             self.add(val);
    //         });
    //         self.update();
    //         window.setTimeout(function() { self.poll(); }, self.alert_poll_time);
    //     }, 'json').fail(function(error) {
    //         self.console.error('webui alerts poll failed', error);
    //         window.setTimeout(function() { self.poll(); }, self.alert_poll_time_on_error);
    //     });
    // }
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
    dbg_console.register('$.WebUIAlerts',  $.WebUIAlerts);
    $.WebUIAlerts.init_container();
});
