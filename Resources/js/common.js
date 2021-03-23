/**
 * Author: sascha_lammers@gmx.de
 */

 // generator in
 // Resources/html/__prototypes.html

$.__global_templates = window.__global_templates = {
    dismissible_alert: '<div><div class="pt-2"><div class="alert alert-dismissible fade show" role="alert"><h4 class="alert-heading"></h4><span></span><button type="button" class="close" data-dismiss="alert" aria-label="Close"><span aria-hidden="true">&times;</span></button></div></div></div>',
    modal_dialog: '<div><div class="modal" tabindex="-1" role="dialog"><div class="modal-dialog modal-lg" role="document"><div class="modal-content"><div class="modal-header"><h5 class="modal-title"></h5><button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button></div><div class="modal-body"></div><div class="modal-footer"><button type="button" class="btn btn-secondary" data-dismiss="modal">Close</button></div></div></div></div></div>',
    filemanager_upload_progress: '<p class="text-center">Uploading <span id="upload_percent"></span>...</p><div class="progress"><div class="progress-bar progress-bar-striped progress-bar-animated text-center" role="progressbar" aria-valuenow="0" aria-valuemin="0" aria-valuemax="10000" style="width:0%;" id="upload_progress"></div></div>',
};

(function() {
    return {
        cookie_name: 'dbg_console',
        null_console: {
            log: function() {
            },
            error: function() {
            },
            warn: function() {
            },
            debug: function() {
            },
            assert: function(args) {
            },
            set_debug: function() {
            },
            ready: function() {
            },
            register: function(name, object) {
                if (!Object.prototype.hasOwnProperty.call(object, 'console')) {
                    object.console = this;
                }
                return object.console;
            },
            list: function() {
                console.log(null);
            },
            is_debug_enabled: function() {
                return false;
            },
            isDebugEnabled: function() {
                return false;
            }
        },
        init_null: function() {
            $.null_console = window.null_console = this.null_console;
            $.dbg_console = window.dbg_console = this.null_console;
            this.init_debug_onoff();
            this.enabled = false;
        },
        init: function() {
            $.null_console = window.null_console = this.null_console;
            $.dbg_console = window.dbg_console = this;
            this.init_debug_onoff();
        },
        init_debug_onoff: function() {
            window.debug_on = function() {
                window.dbg_console.set_debug(true);
            };
            window.debug_off = function() {
                window.dbg_console.set_debug(false);
            };
            window.is_debug_enabled = this.is_debug_enabled;
            window.isDebugEnabled = this.isDebugEnabled;
            window.debugon = window.debug_on;
            window.dbg_on = window.debug_on;
            window.dbgon = window.debug_on;
            window.debugoff = window.debug_off;
            window.console.debug_on = window.debug_on;
            window.console.debug_off = window.debug_off;
            window.console.debugon = window.debug_on;
            window.console.debugoff = window.debug_off;
            window.console.is_debug_enabled = this.is_debug_enabled;
            window.console.isDebugEnabled = this.isDebugEnabled;
        },
        log: function() {
            if (this.enabled) {
                return console.log.apply(this, arguments);
            }
        },
        error: function() {
            if (this.enabled) {
                return console.error.apply(this, arguments);
            }
        },
        warn: function() {
            if (this.enabled) {
                return console.warn.apply(this, arguments);
            }
        },
        debug: function() {
            if (this.enabled) {
                return console.debug.apply(this, arguments);
            }
        },
        assert: function(args) {
            if (this.enabled) {
                return console.assert.apply(this, arguments);
            }
        },
        register: function(name, object, console_object) {
            if (console_object === undefined) {
                var cookie = this.get_cookie();
                var objs = cookie['objects'];
                if (Object.prototype.hasOwnProperty.call(objs, name)) {
                    // set from cookie
                    object.console = this.get_console_by_name(objs[name]);
                }
                else if (!Object.prototype.hasOwnProperty.call(object, 'console')) {
                    // set to null console if the object does not have the property
                    object.console = this.null_console;
                }
            }
            else if (console_object === null) {
                object.console = this.null_console;
            }
            else {
                object.console = console_object;
            }
            var item = this.find_item(name);
            if (item === null) {
                this.registered.push({
                    'name': name,
                    'object': object
                });
            }
            return object.console;
        },
        find_item: function(name) {
            for(key in this.registered) {
                if (this.registered[key]['name'] == name) {
                    return this.registered[key];
                }
            }
            return null;
        },
        get_cookie: function() {
            var cookie = Cookies.getJSON(this.cookie_name, {});
            if (typeof cookie !== 'object') {
                cookie = {};
            }
            if (!Object.prototype.hasOwnProperty.call(cookie, 'enabled')) {
                cookie.enabled = false;
            }
            if (!Object.prototype.hasOwnProperty.call(cookie, 'objects')) {
                cookie.objects = {};
            }
            // window.console.debug('get', this.cookie_name, cookie);
            return cookie;
        },
        set_cookie: function(cookie) {
            Cookies.set(this.cookie_name, cookie);
            // window.console.debug('set', this.cookie_name, cookie);
        },
        ready: function() {
            var cookie = this.get_cookie();
            this.set_debug(!!cookie.enabled);
            if (this.is_debug_enabled()) {
                this.list();
            }
        },
        set_debug: function(enable) {
            this.enabled = enable;
            try {
                var cookie = this.get_cookie();
                cookie.enabled = this.enabled;
                this.set_cookie(cookie);
                var restored = 0;
                var removed = 0;
                for(key in this.registered) {
                    var item = this.registered[key];
                    if (enable) {
                        if (Object.prototype.hasOwnProperty.call(item, 'prev_console')) {
                            item.object.console = item['prev_console'];
                            delete item.prev_console;
                            restored++;
                        }
                        }
                    else {
                        if (item.object.console !== this.null_console) {
                            if (!Object.prototype.hasOwnProperty.call(item, 'prev_console')) {
                                item.prev_console = item.object.console;
                            }
                            item.object.console = this.null_console;
                            removed++;
                        }
                    }
                }
            } catch(e) {
                console.error(e);
            }
            console.log('DEBUGGING ' + (this.enabled ? 'ENABLED' : 'DISABLED') + '\n' + (removed ? ('Removed console from ' + removed + ' objects') : (restored ? ('Restored console for ' + restored + ' objects') : '')));
        },
        set_console: function(name, console_object) {
            this.register(name, console_object);
            var console_name = this.get_console_name(console_object);
            console.log('SET DEBUG CONSOLE: ' + name + ' = ' + console_name);
            try {
                var cookie = this.get_cookie();
                cookie['objects'][name] = console_name;
                this.set_cookie(cookie);
            } catch(e) {
                console.error(e);
            }
        },
        get_console_by_name: function(name) {
            if (name === 'console') {
                return window.console;
            }
            if (name === 'dbg_console') {
                return this;
            }
            if (name === 'null_console' || name === '[null_console]') {
                return this.null_console;
            }
            return this.null_console;
        },
        get_console_name: function(console_object) {
            if (typeof console_object === 'string') {
                return console_object;
            }
            if (console_object === window.console) {
                return "console";
            }
            if (console_object === this.null_console) {
                return "null_console";
            }
            if (console_object === this) {
                return "dbg_console";
            }
            return '[null_console]';
        },
        list: function() {
            var list = [];
            for(key in this.registered) {
                var item = this.registered[key];
                list.push(item.name + '.console = ' + this.get_console_name(item.object.console));
            }
            console.log(this.__line_of_dashes + 'Registered objects with attached console\n' + this.__line_of_dashes + list.join('\n'));
        },
        is_debug_enabled: function() {
            return this.enabled;
        },
        isDebugEnabled: function() {
            return this.enabled;
        },
        registered: [],
        enabled: false,
        __line_of_dashes: '----------------------------------------------------------------------------\n'
    };
})().init();

$(function() {
    $.dbg_console.ready();

    $('.dropdown-submenu .dropdown-menu .dropdown-item:first').each(function() {
        var submenu = $(this).closest('.dropdown-submenu');
        var parent = submenu.find('a:first');
        var href = $(this).attr('href');
        parent.attr('href', href);
    });

});

function config_init(show_ids, update_form) {
    if (update_form === true) {
         $('input,select').addClass('setting-requires-restart');
    }
    if (show_ids !== undefined && show_ids !== null && show_ids.indexOf('%') === -1) {
        $(show_ids).show();
    }
    $('body')[0].onload = null;
}

function pop_error_clear(_target) {
    if (!_target) {
        _target = $('.container:first');
    }
    _target.find('.alert-dismissible').remove();
}

function pop_error(error_type, title, message, _target, clear) {
	var id = "#alert-" + random_str();
    var html = $(window.__global_templates.dismissible_alert);
    html.find('div').addClass('alert-' + error_type).attr('id', id.substr(1));
    html.find('h4').html(title);
    html.find('span:first').html(message);
    if (!_target) {
        _target = $('.container:first');
    }
    if (clear) {
        pop_error_clear(_target);
    }
    _target.prepend('<div class="row"><div class="col">' + html.html() + '</div></div>');
    html.on('closed.bs.alert', function () {
        $(this).closest('div').remove();
    });
    html.alert();
}

function random_str() {
    return Math.random().toString(36).substring(7);
}

$.randomStr = function(len) {
    var s = random_str();
    if (len === undefined) {
        return s;
    }
    while(s.length < len) {
        s += random_str();
    }
    return s.substring(0, len);
}

$.addModalDialog = function(id, title, body, onremove) {
    $('#' + id).remove();
    var html = $(window.__global_templates.modal_dialog);
    html.find('.modal').attr('id', id);
    html.find('.modal-title').html(title);
    html.find('.modal-body').html(body);
    $('body').append(html);
    var dialog = $('#' + id);
    dialog.on('hidden.bs.modal', function () {
        dialog.remove();
        if (onremove) {
            onremove();
        }
    });
    dialog.modal('show');
    return dialog;
};

$.getSessionId = function() {
    try {
        return Cookies.get("SID");
    } catch(e) {
        return '';
    }
}

$.getHttpLocation = function(uri) {
    var url = window.location.protocol == 'http:' ? 'http://' + window.location.host : 'https://' + window.location.host;
    return url + (uri ? uri : '');
}

$.getWebSocketLocation = function(uri) {
    var hasPort = window.location.host.indexOf(':') != -1;
    var url =  window.location.protocol == 'http:' ? 'ws://' + window.location.host + (hasPort ? '' : ':80') : 'wss://' + window.location.host + (hasPort ? '' : ':443');
    return url + (uri ? uri : '');
}

$.getRandomBytes = function(n) {
    var crypto = (self.crypto || self.msCrypto), QUOTA = 65536;
    var a = new Uint8Array(n);
    for (var i = 0; i < n; i += QUOTA) {
        crypto.getRandomValues(a.subarray(i, i + Math.min(n - i, QUOTA)));
    }
    return a;
};

// str          match str and get all matches for a single group
// regex_str    regexp string
// group        group or property name of the capture group
// options      RegExp options
// returns array of strings or undefined if the property for a match does not exist
$.matchAll = function(str, regexp_str, group, options) {
	var regex = new RegExp(regexp_str, options === undefined ? '' : options);
	var index = 0;
    var results = [];
	do {
        var match = str.substring(index).match(regex);
		if (match) {
            var groups = match.groups ? match.groups : match;
            results.push(
                Object.prototype.hasOwnProperty.call(groups, group) ?
                    groups[group] :
                    groups[Object.keys(groups)[0]]
            );;
			index += match['index'] + match[0].length;
		}
	} while(match);
	return results;
};

$.matchReverse = function(str, regexp_str, options) {
    var regex = new RegExp(regexp_str, options === undefined ? 'm' : options);
	var index = 0;
    var results = [];
	do {
        var match = str.substring(index).match(regex);
		if (match) {
            results.unshift(match);
            index += match['index'] + match[0].length;
		}
	} while(match);
	return results;
}

$._____rgb565_to_888 = function(color) {
    var b5 = (color & 0x1f);
    var g6 = (color >> 5) & 0x3f;
    var r5 = (color >> 11);
    return [(r5 * 527 + 23) >> 6, (g6 * 259 + 33) >> 6, (b5 * 527 + 23) >> 6];
}

// the example requires jQuery Color
//
// flash stores the css properties and runs animation 'repeat' times to change back and forth
// bewteen the values. each phase can have a different speed and a hold delay before
//
// $('button').flash({color: 'red'}, 3);
// $('button').flash({color: 'red'}, 1, 'slow');
// $('button').flash({color: 'red'}, 5 'fast');
// $('button').flash({color: 'red'}, 1, 750, 300, 750, 150);
//
jQuery.fn.extend({
	flash: function(css, repeat, speed, hold, reverse, repeat_delay) {
		var repeat = repeat || 1;
        if (speed === undefined || parseInt(speed) == 0) {
            speed = 333;
        }
        else if (speed == 'slow') {
            speed = 750;
        }
        else if (speed == 'fast') {
            speed = 100;
        }
		var hold = hold || Math.round(speed / 2);
		var reverse = reverse || speed;
		var repeat_delay = repeat_delay || Math.round(speed / 4);
		jQuery.each(this, function(key, val) {
			var element = $(val);
			if (element.queue().length == 0) {
				var restore_css = {};
				$(Object.keys(css)).each(function(key, val) {
					restore_css[val] = element.css(val);
				});
				for(var i = 0; i < repeat; i++) {
					element.animate(css, speed);
					element.delay(hold);
					element.animate(restore_css, reverse);
					if (repeat_delay) {
						element.delay(repeat_delay);
					}
				}
			}
		});
        return this;
	}
});


//
// copy text to clipboard when the on click event is executed and
// use $.flash() to signal success. default animation is changing
// to white text on a green background 3 times
//
// $.clipboard($('button'), 'copy this to the clipboard');
// $.clipboard($('button'), 'copy this to the clipboard', {color: 'red'} 300, 3});
// $.clipboard($('button'), function() {
//     return 'lazy loading';
// }, {color: 'red'} 300, 3});
//
$.clipboard = function(element, text, flash_css_or_color, flash_speed, flash_repeat) {
    css = flash_css_or_color || {backgroundColor: '#28a745', color: 'white'};
    if (typeof css !== 'object') {
        css = {color: css };
    }
    flash_speed = flash_speed || 200;
    flash_repeat = flash_repeat || 3;
    element.on('click', function(e) {
        e.preventDefault();
        if (typeof text === 'function') {
            text = text();
        }
        function success() {
            element.flash(css, flash_repeat, flash_speed);
            window.setTimeout(function() {
                element.blur();
            }, flash_speed * flash_repeat * 2.75 + 50);
        }
        function fail() {
            alert('Failed to copy text to clipboard');
        }
        try {
            navigator.clipboard.writeText(text).then(function() {
                success();
            }, function() {
                fail();
            });
        } catch(e) {
            try {
                var ta = $('<textarea style="width:1px;height:1px;position:absolute:right-100px;overeflow:hidden">');
                ta.text(text);
                $('body').append(ta);
                ta.select();
                document.execCommand('copy');
                ta.remove();
                success();
            } catch(e) {
                fail();
            }
        }
    });
};
