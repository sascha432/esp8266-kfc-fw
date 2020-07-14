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
            $(e.target).removeClass('is-valid').addClass('is-invalid');
            var parent = $(e.target).closest('.input-group');
            if (parent.length == 0) {
                parent = $(e.target).closest('.form-group');
            }
            var field = parent.children().last();
            if (field.length) {
                field.after('<div class="invalid-feedback">' + e.error + '</div>')
            }
            else {
                alert('Form error: ' + e.error); // TODO add to top of the form, usually an error in the form code itself
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
        console.log(e);
    }

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
        always_visible: false,
        protected: 'auto',
        icon_protected: 'oi-eye',
        icon_unprotected: 'oi-shield',
    }, $.visible_password_options);

    $('.visible-password').each(function() {

        var options = $.visible_password_options;
        var $wrappedElement = $(this).wrap('<div class="input-group visible-password-group"></div>');

        $.each( options, function( key, value ) {
            var val = $wrappedElement.data(key.replace('_', '-'));
            if (val !== undefined) {
                options[key] = val;
            }
        });

        if (options.disabled) {
            return;
        }

        var $button, $span;
        function is_password() {
            return $wrappedElement.attr('type') == 'password';
        }
        function is_protected() {
            return $button.data('protected') != '0';
        }
        function set_type(type) {
            $wrappedElement.attr('type', type);
        }
        function update_icon() {
            if ((!options.always_visible && is_protected()) || (options.always_visible && is_password())) {
                $span.removeClass(options.icon_unprotected).addClass(options.icon_protected); // protected
            } else {
                $span.addClass(options.icon_unprotected).removeClass(options.icon_protected); // visible/unprotected
            }
        }

        var $newElement = $('<div class="input-group-append"><button class="btn btn-default btn-visible-password" type="button" data-protected="' + (is_password() || options.protected === true || (options.always_visible && options.protected === 'auto') ? '1' : '0') + '"><span class="oi"></span></button>');
        $button = $newElement.find('button');
        $span = $button.find('span');
        $wrappedElement
            .attr('autocomplete', 'new-password')
            .attr('spellcheck', 'false')
            .after($newElement)

        if (!options.always_visible || options.protected === true || (options.always_visible && options.protected === 'auto')) {
            set_type('password');
        }
        update_icon();

        $button.on('click', function(e) {
            e.preventDefault();
            $button.data('protected', is_protected() ? '0' : '1');
            if (options.always_visible) {
                set_type(is_protected() ? 'password' : 'text');
            }
            update_icon();
        });
        if (!options.always_visible) {
            $wrappedElement.on('focus', function() {
                if (!is_protected()) {
                    set_type('text');
                }
            })
            .on('blur', function() {
                set_type('password');
            });
        }

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
