/**
 * Author: sascha_lammers@gmx.de
 */

function form_invalid_feedback(selector, message) {
    $(selector).addClass('is-invalid');
    $(selector).parent().find(".invalid-feedback").remove();
    $(selector).parent().append('<div class="invalid-feedback">' + message + '</div>');
}

$.visible_password_options = {};

$.formValidator = {
    name: 'form:last',
    errors: [],
    validated: false,
    markAsValid: false,
    validate: function() {
        $.formValidator.validated = true;
        var $form = $($.formValidator.form);
        if ($.formValidator.markAsValid) {
            $form.find('select,input').addClass('is-valid').remove('.invalid-feedback');
        }
        for(var i = 0; i < $.formValidator.errors.length; i++) {
            var e = $.formValidator.errors[i];
            var $t = $(e.target);
            if ($t.length == 0) {
                dbg_console.error('target invalid', $t);
            }
            if ($t.parent().hasClass('form-enable-slider')) {
                $t = $t.parent();
                dbg_console.debug('slider', $t);
            }
            $t.removeClass('is-valid').addClass('is-invalid');
            var parent = $t.closest('.input-group,.form-group');
            if (parent.length == 0) {
                dbg_console.error('parent group not found', $t);
            }
            var error = '<div class="invalid-feedback" style="display:block">' + e.error + '</div>';
            var field = parent.children().last();
            var added = field.after(error);
            if (!$(added).is(":visible") || $(added).is(":hidden")) {
                dbg_console.error('error is hodden', $(added));
            }
        }
    },
    addErrors: function(errors, target) {
        $.formValidator.errors = errors;
        if (target) {
            $.formValidator.form = target;
        } else {
            $.formValidator.form = $($.formValidator.name);
        }
        $.formValidator.validate();
    }
};
// $.formValidator.addErrors([{'target':'#colon_sp','error':'This fields value must be between 50 and 65535 or 0'}]);
// $.formValidator.addErrors([{'target':'#brightness','error':'This fields value must be between 50 and 65535 or 0'}]);

$.addFormHelp = function(data) {
    for (key in data) {
        var label = $('label[for="' +key +'"]');
        if (label.length) {
            var tmp = label.html();
            var pos = tmp.indexOf(':');
            if (pos != -1) {
                tmp = tmp.substring(0, pos + 1);
            }
            tmp = tmp + '<br><small>' + data[key] + '</small>';
            label.html(tmp);
        }
    }
};

$.urlParam = function(name, remove) {
    var results = new RegExp('([\?&])' + name + '=([^&#]*)([&#]?)').exec(window.location.href);
    if (results == null) {
       return null;
    }
    if (remove) {
        var str = name + '=' + results[2];
        if (results[3] == '&') {
            if (results[1] == '?' || results[1] == '&') {
                str += '&';
            }
        }
        if (results[1] == '&' && results[3] == '#') {
            str = '&' + str;
        }
        str = window.location.href.replace(str, '');
        try {
            history.pushState({}, null, str);
        } catch(e) {
            console.log(e);
            console.log(str);
        }
    }
    return decodeURI(results[2]) || 0;
}

$(function() {

    // execute first
    if ($('body')[0].onload) {
        $('body')[0].onload();
        $('body')[0].onload = null;
    }

    try {
        var message;
        if (message = $.urlParam('_message', true)) {
            pop_error($.urlParam('_type', true) || 'danger', $.urlParam('_title', true) || 'ERROR!', message, null, true);
        }
    } catch(e) {
        dbg_console.error(e);
    }
    $('.form-enable-slider input[type="range"]').rangeslider({
        polyfill : false
    });

    $('.form-dependency-group').each(function() {
        var dep;
        var actionStr;
        try {
            actionStr = $(this).data('action');
            var str = actionStr.replace(/'/g, '"');
            str = str.replace(/&quot;/g, '\\"');
            str = str.replace(/&apos;/g, "'");
            try {
                dep = JSON.parse(str);
            } catch(e) {
                dbg_console.error(e, $(this).data('action'), 'raw', actionStr, 'replaced', str, 'json', dep);
                return;
            }
        } catch(e) {
            dbg_console.error(e, $(this).data('action'));
            return;
        }
        var states = dep.s;
        var miss = dep.m ? dep.m : '';
        var always_execute = dep.e ? dep.e : '';
        var $I = $(dep.i);
        var $T = dep.t ? $(dep.t) : $(this);
        if ($I.length == 0) {
            dbg_console.error('$I not found', dep.i);
        }
        if ($T.length == 0) {
            dbg_console.error('$T not found', dep.t);
        }
        $I.on('change', function() {
            var $V = $I.val();
            // var $Vint = parseInt($V);
            var code = miss;
            var vKey = 'NO KEY FOUND';
            $.each(states, function(key, val) {
                if ($V == key) {
                //if ($V === key || parseInt(key) === $Vint) {
                    code = val;
                    vKey = key;
                }
            });
            try {
                code_x = always_execute + ';;;' + code;
                // dbg_console.debug('onChange', 'V', $V, $Vint, 'I', $I, 'T', $T, 'key', vKey, 'code', code_x);
                eval(code_x);
            } catch(e) {
                dbg_console.error(e, {'value': $V, 'target': $T, 'input': $I, 'key': vKey, 'states': states, 'code': code_x});
            }
        }).trigger('change');
    });

    (function() {
        var cookies = {}; // cookie cache
        $('.card .collapse').each(function() {
            var card = $(this);
            var parent = $(card.data('cookie'));
            if (parent.length) {
                var name = card.attr('id').split('-', 2);
                if (name.length == 2) {
                    var cookie_name = 'card_state_' + parent.attr('id');
                    if (cookies[cookie_name] === undefined) {
                        cookies[cookie_name] = Cookies.getJSON(cookie_name, {});
                    }
                    name = name[1];
                    if (cookies[cookie_name][name] === undefined) {
                        cookies[cookie_name][name] = card.hasClass('show');
                    } else {
                        (cookies[cookie_name][name]) ? card.addClass('show') : card.removeClass('show');
                    }

                    function save() {
                        // dbg_console.debug('set_cookie', cookie_name, JSON.stringify(cookies[cookie_name]));
                        Cookies.set(cookie_name, cookies[cookie_name]);
                    }
                    card.on('hide.bs.collapse', function() {
                        cookies[cookie_name][name] = false;
                        save();
                    }).on('show.bs.collapse', function() {
                        cookies[cookie_name][name] = true;
                        save();
                    });
                }
            }
        });
    })();

    $('.resolve-zerconf-button').on('click', function() {
        var button = $(this);
        var target = $(button.data('target'));
        if (target.length == 0) {
            target = button.closest('.input-group').find('input[type=text]');
        }
        var value = $(target).val();
        var dlg_id = 'resolve_zeroconf_dialog';
        var dlg_title = 'Zeroconf Resolver';
        var show_dlg = function(body) {
            $.addModalDialog(dlg_id, dlg_title, body, function() {
                button.prop('disabled', false);
            });
        };
        button.prop('disabled', true);
        $.get('/zeroconf?SID=' + $.getSessionId() + '&value=' + value, function (data) {
            dbg_console.log(data);
            show_dlg(data);
        }).fail(function(jqXHR, textStatus, error) {
            dbg_console.error(error);
            show_dlg('An error occured loading results:<br>' + error);
        });
    });


    // all forms
    $('.setting-requires-restart').change(function(e) {
        var $this = $(this);
        var id = "#alert-for-" + $this.attr('id');
        if ($this.attr('type') != 'hidden' && $this.val() != this.defaultValue && $(id).length === 0) {
            var label = $('label[for="' + $this.attr('id') + '"]').html();
            if (!label) {
                label = $this.data('label');
            }
            var $html = $($.__prototypes.dismissible_alert);
            $html.find('div').addClass('alert-warning').attr('id', id.substr(1));
            $html.find('h4').html("WARNING!");
            $html.find('span:first').html("Changing <strong>" + label.replace(":", "") + "</strong> requires a restart...");
            $(this).closest("form").prepend('<div class="form-group">' + $html.html() + '</div>');
            $html.on('closed.bs.alert', function () {
                $(this).closest('div').remove();
            });
            $html.alert();
        } else if ($(id).length !== 0) {
            $(id).alert('close');
        }
    });

    // global presets
    // override with data-protected="true", data-icon-protected="oi-xyz" ...
    // if disabled=false, the input is type password until focused
    $.visible_password_options = $.extend({
        protected: 'auto',              // auto: type=input type, false: type=text, true: type=password
        disabled: false,                // disabled=true sets protected state and does not add a toggle button
        type: 'password',               // set to input type while focused
        iconProtected: 'oi oi-eye',
        iconUnprotected: 'oi oi-shield',
    }, $.visible_password_options);

    $('.visible-password').each(function() {
        var input = $(this);
        var options = $.extend({}, $.visible_password_options);
        var wrapper = input.wrap('<div class="input-group visible-password-group"></div>').closest('.visible-password-group');

        $.extend(options, input.data());
        if (input.attr('autocomplete') == '') {
            input.attr('autocomplete', 'new-password')
        }
        input.attr('spellcheck', 'false');

        // set initial input type
        options.type = options.protected === true ? 'password' : (options.protected === false ? 'text' : input.attr('type'));
        if (options.disabled) {
            input.attr('type', options.type); // static
            return;
        }

        // toggle button
        input.attr('type', 'password');
        input.after('<div class="input-group-append"><button class="btn btn-default btn-visible-password" type="button"><span></span></button>');
        var button = wrapper.find('button');
        var icon = button.find('span');
        function add_icon() {
            icon.addClass((options.type == 'text') ? options.iconProtected : options.iconUnprotected);
        }
        add_icon();
        // toggle focused type
        button.on('click', function() {
            options.type = options.type == 'password' ? 'text' : 'password';
            icon.removeClass(options.iconProtected + ' ' + options.iconUnprotected);
            add_icon();
        });
        // change type onfocs/blur
        input.on('focus', function() {
            input.attr('type', options.type);
        }).on('blur', function() {
            input.attr('type', 'password')
        });
    });

    $('.button-checkbox').each(function () {
        var widget = $(this);
        var button = widget.find('button');
        var color = button.data('color'); // btn-<color> class
        var on_icon = button.data('on-icon');
        var off_icon = button.data('off-icon');
        if (!color) {
            color = 'primary';
        }
        var on_class = 'btn-' + color;
        if (on_icon === undefined) {
            on_icon = 'oi oi-task';
        }
        if (off_icon === undefined) {
            off_icon = 'oi oi-ban';
        }
        var icons = [off_icon, on_icon];

        var id = '#' + button.attr('id').substr(1); // id of hidden field = _<id>
        var hiddenInput = $(id);
        if (hiddenInput.length == 0) {
            dbg_console.error('cannot find hidden input field', id);
            return;
        }

        // get inverted initial state from input field
        var value = parseInt(hiddenInput.val()) ? 0 : 1;
        var icon = widget.find('.state-icon');

        button.on('click', function() {
            value = value ? 0 : 1;
            button.removeClass('btn-default ' + on_class).addClass(value ? on_class : 'btn-default');
            icon.attr('class', 'state-icon ' + icons[value]);
            hiddenInput.val(value);
        });

        // prepend icon
        if (icon.length == 0) {
            button.prepend('<i class="state-icon"></i> ');
            icon = widget.find('.state-icon');
        }

        // update button
        button.trigger('click');

    });
});
