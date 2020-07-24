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
            if ($t.parent().hasClass('form-enable-slider')) {
                $t = $t.parent();
                console.log($t);
            }
            $t.removeClass('is-valid').addClass('is-invalid');
            var parent = $t.closest('.input-group,.form-group');
            var error = '<div class="invalid-feedback" style="display:block">' + e.error + '</div>';
            var field = parent.children().last();
            field.after(error)
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
        // i: '#input', 't': '#target', 's': { '#input_value': 'code to execute', 'm': 'code to execute if value is not found in s', 'e': 'code to always execute before' }
        // code: $I input, $T target, $V value
        //
        // #group-div-class is the target, #my_input_field: value 0 and 1 hide the div, 2 shows it
        // auto &group = form.addDivGroup(F("group-div-class"), F("{'i':'#my_input_field','s':{'0':'$T.hide()','1':'$T.hide()','2':'$T.show()'}},'m':'alert(\\'Invalid value: \\'+$V)'"));
        var dep;
        try {
            dep = JSON.parse($(this).data('action').replace(/'/g, '"'));
        } catch(e) {
            dbg_console.error(e)
            return;
        }
        var states = dep.s;
        var miss = dep.m ? dep.m : '';
        var always_execute = dep.e ? dep.e : '';
        var $I = $(dep.i);
        var $T = dep.t ? $(dep.t) : $(this);
        $I.on('change', function() {
            var $V = $I.val();
            var code = miss;
            var vKey = 'NO KEY FOUND';
            $.each(states, function(key, val) {
                if ($V == key) {
                    code = val;
                    vKey = key;
                }
            });
            try {
                code_x = always_execute + ';;;' + code;
                eval(code_x);
            } catch(e) {
                dbg_console.debug(e, {'value': $V, 'key': vKey, 'states': states, 'code': code_x});
            }
        }).trigger('change');
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

    // https://bootsnipp.com/snippets/featured/jquery-checkbox-buttons
    $('.button-checkbox').each(function () {

        // Settings
        var $widget = $(this),
            $button = $widget.find('button'),
            $checkbox = $widget.find('input:checkbox'),
            color = $button.data('color'),
            settings = {
                on: {
                    icon: $(this).data('on-icon')
                },
                off: {
                    icon: $(this).data('off-icon')
                }
            };

        // Event Handlers
        $checkbox.on('change', function () {
            updateDisplay();
        });
        $button.on('click', function () {
            $checkbox.prop('checked', !$checkbox.is(':checked'));
            $checkbox.triggerHandler('change');
            //updateDisplay();
        });

        // Actions
        function updateDisplay() {
            var isChecked = $checkbox.is(':checked');

            // Set the button's state
            $button.data('state', (isChecked) ? "on" : "off");

            // Set the button's icon
            $button.find('.state-icon')
                .removeClass()
                .addClass('state-icon ' + settings[$button.data('state')].icon);

            // Update the button's color
            if (isChecked) {
                $button
                    .removeClass('btn-default')
                    .addClass('btn-' + color + ' active');
            }
            else {
                $button
                    .removeClass('btn-' + color + ' active')
                    .addClass('btn-default');
            }
        }

        // Initialization
        function init() {
            updateDisplay();
            // Inject the icon if applicable
            if ($button.find('.state-icon').length == 0 && settings[$button.data('state')].icon) {
                $button.prepend('<i class="state-icon ' + settings[$button.data('state')].icon + '"></i> ');
            }
        }
        init();
    });
});
