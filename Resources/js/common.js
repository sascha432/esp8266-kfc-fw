/**
 * Author: sascha_lammers@gmx.de
 */

 // generator in
 // Resources/html/__prototypes.html
 $.__prototypes = {
    dismissible_alert: '<div><div class="pt-2"><div class="alert alert-dismissible fade show" role="alert"><h4 class="alert-heading"></h4><span></span><button type="button" class="close" data-dismiss="alert" aria-label="Close"><span aria-hidden="true">&times;</span></button></div></div></div>',
    modal_dialog: '<div><div class="modal" tabindex="-1" role="dialog"><div class="modal-dialog modal-lg" role="document"><div class="modal-content"><div class="modal-header"><h5 class="modal-title"></h5><button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button></div><div class="modal-body"></div><div class="modal-footer"><button type="button" class="btn btn-secondary" data-dismiss="modal">Close</button></div></div></div></div></div>',
    filemanager_upload_progress: '<p class="text-center">Uploading <span id="upload_percent"></span>...</p><div class="progress"><div class="progress-bar progress-bar-striped progress-bar-animated text-center" role="progressbar" aria-valuenow="0" aria-valuemin="0" aria-valuemax="10000" style="width:0%;" id="upload_progress"></div></div>',
};

window.dbg_console = {
    init: function() {
        window.dbg_console = this;
        window.debug_on = function() {
            window.dbg_console.set_debug(true);
        };
        window.debug_off = function() {
            window.dbg_console.set_debug(false);
        };
        window.debugon = window.debug_on;
        window.dbg_on = window.debug_on;
        window.dbgon = window.debug_on;
        window.debugoff = window.debug_off;
        console.debug_on = window.debug_on;
        console.debug_off = window.debug_off;
        console.debugon = window.debug_on;
        console.debugoff = window.debug_off;
    },
    ready: function() {
        try {
            if (Cookies.get("dbg_console")) {
                this.set_debug(true);
            }
        } catch(e) {
        }
    },
    set_debug: function(enable) {
        this.vars.enabled = enable;
        console.log('DEBUGGING ' + (this.vars.enabled ? 'ENABLED' : 'DISABLED'));
    },
    log: function() {
        if (this.vars.enabled) {
            return console.log.apply(this, arguments);
        }
    },
    error: function() {
        if (this.vars.enabled) {
            return console.error.apply(this, arguments);
        }
    },
    warn: function() {
        if (this.vars.enabled) {
            return console.warn.apply(this, arguments);
        }
    },
    debug: function() {
        if (this.vars.enabled) {
            return console.debug.apply(this, arguments);
        }
    },
    assert: function(args) {
        if (this.vars.enabled) {
            return console.assert.apply(this, arguments);
        }
    },
    called: function(name, args) {
        var a = [ 'function: ' + name + '(), args:' ];
        for(var i = 0; i < args.length; i++) {
            a.push(args[i]);
        }
        this.debug.apply(this, a);
    },
    vars: {
        enabled: false,
    },
};
window.dbg_console.init();
$(function() {
    window.dbg_console.ready();
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
    var html = $($.__prototypes.dismissible_alert);
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


$.addModalDialog = function(id, title, body, onremove) {
    $('#' + id).remove();
    var html = $($.__prototypes.modal_dialog);
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
$.matchAll = function(str, regex_str, group, options) {
	var regex = new RegExp(regex_str, options === undefined ? '' : options.replace(/g/g, ''));
	var index = 0;
    var results = [];
	do {
        var match = str.substring(index).match(regex);
		if (match) {
            var groups = match.groups ? match.groups : match;
            results.push(
                Reflect.has(groups, group) ?
                    Reflect.get(groups, group) :
                    Reflect.get(groups, Reflect.ownKeys(groups)[0])
            );
			index += match.index + match.length + 1;
		}
	} while(match);
	return results;
};
