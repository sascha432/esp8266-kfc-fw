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

$.addModalDialog = function(id, title, body, onremove) {
    $('#' + id).remove();
    $('body').append('<div class="modal" tabindex="-1" role="dialog" id="' + id + '"><div class="modal-dialog modal-lg" role="document"><div class="modal-content"><div class="modal-header"><h5 class="modal-title">' + title +  '</h5><button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button></div><div class="modal-body">' + body + '</div><div class="modal-footer"><button type="button" class="btn btn-secondary" data-dismiss="modal">Close</button></div></div></div></div>');
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

    $('.resolve-zerconf-button').on('click', function() {
        var button = $(this);
        var target = $(button.data('target'));
        if (target.length == 0) {
            target = button.closest('.input-group').find('input[type=text]');
        }
        var value = $(target).val();
        var dlg_id = 'resolve_zeroconf_dialog';
        var dlg_title = 'Zeroconf Resolver';
        var dlg = function(body) {
            $.addModalDialog(dlg_id, dlg_title, body, function() {
                button.prop('disabled', false);
            });
        };
        button.prop('disabled', true);
        $.get('/zeroconf?SID=' + $.getSessionId() + '&value=' + value, function (data) {
            dlg(data);
        }).fail(function(jqXHR, textStatus, error) {
            dlg('An error occured loading results:<br>' + error);
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

    $.visible_password_options = $.extend({
        disabled: false,
        protected: 'auto',
        icon_protected: 'oi-eye',
        icon_unprotected: 'oi-shield',
    }, $.visible_password_options);

    $('.visible-password').each(function() {

        var options = $.visible_password_options;
        var $wrappedElement = $(this).wrap('<div class="input-group visible-password-group"></div>');
        var $button;
        var $span;
        var auto_protected_state;

        $.each(options, function(key, value) {
            var val = $wrappedElement.data(key.replace('_', '-'));
            if (val !== undefined) {
                options[key] = val;
            }
        });

        if (options.disabled) {
            return;
        }

        function is_true(value) {
            return value == 'true' || value == '1' || value == true || value == 1;
        }
        function set_type(type) {
            $wrappedElement.attr('type', type ? 'password' : 'text');
        }
        function update_icon() {
            if (auto_protected_state) {
                set_type(true);
                $span.removeClass(options.icon_unprotected).addClass(options.icon_protected); // protected
            } else {
                $span.addClass(options.icon_unprotected).removeClass(options.icon_protected); // visible/unprotected
            }
        }

        auto_protected_state = is_true(options.protected) || ($wrappedElement.attr('type') == 'password');
        var $newElement = $('<div class="input-group-append"><button class="btn btn-default btn-visible-password" type="button"><span class="oi"></span></button>');
        $button = $newElement.find('button');
        $span = $button.find('span');
        if ($wrappedElement.attr('autocomplete') == '') {
            $wrappedElement.attr('autocomplete', 'new-password')
        }
        $wrappedElement.attr('spellcheck', 'false').after($newElement)
        update_icon();

        $button.on('click', function(e) {
            e.preventDefault();
            auto_protected_state = !auto_protected_state;
            update_icon();
        });
        $wrappedElement.on('focus', function() {
            if (!auto_protected_state) {
                set_type(false);
            }
        }).on('blur', function() {
            set_type(true);
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
        if (on_icon !== undefined) {
            on_icon = 'oi oi-task';
        }
        if (off_icon !== undefined) {
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

        button.on('click', function() {
            value = value ? 0 : 1;
            button.removeClass('btn-default ' + on_class).addClass(value ? on_class : 'btn-default');
            button.find('.state-icon').attr('class', 'state-icon ' + icons[value]);
            hiddenInput.val(value);
        });

        // prepend icon
        if (widget.find('.state-icon').length == 0) {
            button.prepend('<i class="state-icon"></i> ');
        }

        // update button
        button.trigger('click');

    });
});
