/**
 * Author: sascha_lammers@gmx.de
 */

function form_invalid_feedback(selector, message) {
    $(selector).addClass('is-invalid');
    $(selector).parent().find(".invalid-feedback").remove();
    $(selector).parent().append('<div class="invalid-feedback">' + message + '</div>');
}

function form_set_disabled(items, state) {
    if (state) {
        items.attr('disabled', 'disabled');
        items.prop('disabled', true);
    } else {
        items.removeAttr('disabled');
        items.prop('disabled', false);
    }
}

$.visible_password_options = {};

// ---------------------------------------------------------------------------------
// marks fields as invalid and adds "invalid-feedback" divs when
// $.formValidator.addErrors() is called. if the input is inside a collapsed
// group, the group is shown
//
// example:
// $.formValidator.addErrors([{target:'#brightness',name:'Display Brightness',error:'This fields value must be between 50 and 65535 or 0'},{target:'#pwm_0','error':'This fields value must be between 50 and 65535 or 0'},{target:'#pwm_1',error:'This fields value must be between 50 and 65535 or 0'}, {target:'#missing_field_123',error:'missing target'}]);
// $.formValidator.addErrors([{target:'#pwm_0','error':'This fields value must be between 50 and 65535 or 0'}]);
// ---------------------------------------------------------------------------------

$.formValidator = {
    target: 'form:last',
    errors: [],
    validated: false,
    input_tags: 'select, input:not(.input-text-range-child), .form-enable-slider, .input-text-range',
    validate: function() {
        this.validated = true;
        var form = $(this.target);
        var form_groups = form.find('.form-group');
        var input_tags = this.input_tags;
        // remove previous errors
        form.find('.is-invalid, .is-valid').removeClass('is-invalid is-valid');
        form.find('.invalid-feedback').remove();
        form.find('.invalid-feedback-alert').remove();
        // add errors
        $(this.errors).each(function(key, val) {
            var target = $(val.target);
            var group = target.closest('.form-group');
            if (group.length) {
                console.debug('error', val, 'target', target, 'group', group, 'input-tag', group.find(input_tags));
                group.find(input_tags).addClass('is-invalid');
                group.append('<div class="invalid-feedback">' + val.error + '</>');
            } else {
                if (!val.name) {
                    val.name = 'Field "' + val.target.replace(/[\.#]/g, '').replace(/_([0-9]+)/g, ' #\$1').replace(/[_-]/g, ' ') + '"';
                }
                console.debug('error', val, 'target', target, 'group or target not found');
                var alert = form.find('.invalid-feedback-alert');
                if (!alert.length) {
                    form.prepend('<div class="invalid-feedback-alert mt-3 mb-0 p-2 alert alert-danger"><h5 class="text-center">Additional errors</h5><hr></div>');
                    alert = form.find('.invalid-feedback-alert');
                }
                alert.append('<div class="m-2">' + val.name + ': <strong>' + val.error + '</strong></div>');
            }
        });
        // display feedback and open collapsed groups
        form.find('.invalid-feedback').show().closest('.collapse').collapse('show');
        // add is-valid to all inputs that are not marked as invalid
        form_groups.find(input_tags).not('.is-invalid').addClass('is-valid');
    },
    addErrors: function(errors) {
        this.errors = errors;
        this.validate();
    }
};


// ---------------------------------------------------------------------------------
// appends the html from the class .form-help-block to the designated target labels
// it uses a small font size that can be increase/decreased by clicking on the text
// the function is executed when the javascript has loaded to speed up the process
// it is also executed on window.onload() with a small delay to add the help
// to labels that did not exist at the time of loading the script
//
// <div class="form-help-block">
// <div data-target="#target,#target2,[id^=starting_with_]">The ADC of the ESP8266 has some limitations. If it is read too...</div>
// </div>
// ---------------------------------------------------------------------------------
$.addFormHelp = function(force) {
    $('.form-help-block div[data-target]').each(function() {
        var targets = $($(this).data('target'));
        // after "tag"" everything is replaced with the help. if it does not exist, the help is appended
        var tag = '<br><div class="form-help-addon">';
        var help = tag + '<div class="form-help-label-text">' + $(this).html() + '</div></div>';
        targets.each(function() {
            var label = $('label[for="' + $(this).attr('id') + '"]');
            if (force || label.find('.form-help-addon').length == 0) {
                console.debug('adding help #'+ $(this).attr('id'));
                var regex = RegExp(tag.replace(/[.*+?^${}()|[\]\\]/g, '\\$&') + '.*');
                label.html(label.html().replace(regex, '') + help);
                label.find('.form-help-label-text').on('click', function(e) {
                    e.preventDefault();
                    $(this).toggleClass('form-help-label-text-zoom-in');
                });
            }
        });
    });
};

$.addFormHelp(true);

$(function() {
    // run again in update mode once the page has finished loading
    $.addFormHelp(false);
    window.setTimeout(function() {
        $.addFormHelp(false);
    }, 100);
});

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

// init entire body or the forms inside the element passed
$.formInitFunc = function(element) {

    if (!element) {
        // execute first
        if ($('body')[0].onload) {
            $('body')[0].onload();
            $('body')[0].onload = null;
        }
        element = $('body');

        try {
            var message;
            if (message = $.urlParam('_message', true)) {
                pop_error($.urlParam('_type', true) || 'danger', $.urlParam('_title', true) || 'ERROR!', message, null, true);
            }
        } catch(e) {
            console.error(e);
        }

    }

    // ---------------------------------------------------------------------------------
    // range slider
    // ---------------------------------------------------------------------------------
    element.find('.form-enable-slider input[type="range"]').rangeslider({
        polyfill : false
    });

    // ---------------------------------------------------------------------------------
    // allows to hide a group on input fields when the value of target changes
    // ---------------------------------------------------------------------------------
    element.find('.form-dependency-group').each(function() {
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
                console.error(e, $(this).data('action'), 'raw', actionStr, 'replaced', str, 'json', dep);
                return;
            }
        } catch(e) {
            console.error(e, $(this).data('action'));
            return;
        }
        var states = dep.s;
        var miss = dep.m ? dep.m : '';
        var always_execute = dep.e ? dep.e : '';
        var $I = $(dep.i);
        var $T = dep.t ? $(dep.t) : $(this);
        if ($I.length == 0) {
            console.error('$I not found', dep.i);
        }
        if ($T.length == 0) {
            console.error('$T not found', dep.t);
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
                // console.debug('onChange', 'V', $V, $Vint, 'I', $I, 'T', $T, 'key', vKey, 'code', code_x);
                eval(code_x);
            } catch(e) {
                console.error(e, {'value': $V, 'target': $T, 'input': $I, 'key': vKey, 'states': states, 'code': code_x});
            }
        }).trigger('change');
    });

    // ---------------------------------------------------------------------------------
    // stores the state of collapsible accordion type cards in a cookie
    // the child needs to have the attribute data-cookie="cookie-name" set instead
    // of data-parent="..."
    // ---------------------------------------------------------------------------------
    (function() {
        var cookies = {}; // cookie cache
        element.find('.card .collapse').each(function() {
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
                        // console.debug('set_cookie', cookie_name, JSON.stringify(cookies[cookie_name]));
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

    function getIntFloatValue(value) {
        if (value === undefined) {
            return undefined;
        }
        if (value == parseInt(value)) {
            value = parseInt(value);
        }
        else if (value == parseFloat(value)) {
            value = parseFloat(value);
        }
        return value;
    }

    // ---------------------------------------------------------------------------------
    // Adds a placeholder Disabled to the field when it matches the value of the
    // attribute. for example disabled-value="0". if the value is an integer or float,
    // "0" will match "" and "0", as well as "0.0"
    // The placeholder can be set with disabled-value-placeholder and defauls to
    // "Disabled"
    // disabled-value-targets can be a selector of additional input fields to disable
    // Works with .input-text-range
    // ---------------------------------------------------------------------------------
    element.find('[disabled-value]').each(function() {
        var self = $(this);
        var value = getIntFloatValue(self.attr('disabled-value'));
        var place_holder = self.attr('disabled-value-placeholder') || 'Disabled';
        var targets = self.attr('disabled-value-targets');
        self.on('change', function() {
            var val = self.val();
            if (value == val) {
                self.attr('placeholder', place_holder).val('');
                if (targets) {
                    form_set_disabled($(targets), true);
                }
            }
            else if (self.attr('placeholder')) {
                self.removeAttr('placeholder');
                if (targets) {
                    form_set_disabled($(targets), false);
                }
            }
        }).trigger('change');
    });

    // ---------------------------------------------------------------------------------
    // adds an input-group-append button to the field to test resolving zeroconf
    // ---------------------------------------------------------------------------------
    element.find('.resolve-zerconf-button').on('click', function() {
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
            console.log(data);
            show_dlg(data);
        }).fail(function(jqXHR, textStatus, error) {
            console.error(error);
            show_dlg('An error occured loading results:<br>' + error);
        });
    });


    // ---------------------------------------------------------------------------------
    // adds a warning to the top of the page if this value gets changed
    // ---------------------------------------------------------------------------------
    element.find('.setting-requires-restart').change(function(e) {
        var $this = $(this);
        var id = "#alert-for-" + $this.attr('id');
        if ($this.attr('type') != 'hidden' && $this.val() != this.defaultValue && $(id).length === 0) {
            var label = $('label[for="' + $this.attr('id') + '"]').html();
            if (!label) {
                label = $this.data('label');
            }
            var $html = $(window.__global_templates.dismissible_alert);
            $html.find('div').addClass('alert-warning').attr('id', id.substr(1));
            $html.find('h4').html("WARNING!");
            $html.find('span:first').html("Changing <strong>" + label.replace(":", "") + "</strong> requires a restart...");
            $(this).closest("form").prepend('<div class="form-group">' + $html.html() + '</div>');
            $html.on('closed.bs.alert', function () {
                $(this).closest('div').remove();
            });
            $html.alert();
        }
        else if ($(id).length !== 0) {
            $(id).alert('close');
        }
    });

    // ---------------------------------------------------------------------------------
    // password fields with visible toggle button
    // ---------------------------------------------------------------------------------
    // adds a button to password fields to make the password visible
    // options are permanently visile, visible while editing and hidden (***)
    // the option can be fixed or adjustable by the user

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

    element.find('.visible-password').each(function() {
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

    // ---------------------------------------------------------------------------------
    // links a checkbox button to a hidden field
    // the id of the checkbox must match the id of the hidden field with a suffix '_' or
    // use data-target=... to point to the hidden field
    //
    // <input type="hidden" id="myfield">
    // <button class=".button-checkbox btn ..." id="_myfield">
    // <button class=".button-checkbox btn ..." data-target="#myfield">
    //
    // <button data-color="primary" data-on-icon="oi oi-task" data-off-icon="oi oi-ban">
    // data-color="primary" adds the class "btn-primary" to the button if checked
    // ---------------------------------------------------------------------------------
    element.find('.button-checkbox').each(function () {
        var widget = $(this);
        var button = widget.find('button');
        if (button.length == 0) {
            button = widget;
        }
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
            hiddenInput = $(button.data('target'));
            if (hiddenInput.length == 0) {
                console.error('cannot find hidden input field', id);
                return;
            }
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

        // disable button if input is disabled
        if (hiddenInput.is(':disabled')) {
            button.off('click').addClass('disabled');
        }

    });

    // ---------------------------------------------------------------------------------
    // FormUI:InlineSuffix()
    // adds input-group-text inside the input field
    // ---------------------------------------------------------------------------------
    {
        var input_group_text_resize_handler = false;
        element.find('.input-group-text.inline.hidden').each(function() {
            var text = $(this);
            var input = text.closest('div.input-group').find('input:first')
            input.addClass('input-text-inline-group');
            var update_inline_element = function() {
                var pos = text.css('position');
                // reset
                input.removeAttr('style').css('padding-right', '');
                text.removeAttr('style').css('left', '').css('top', '');
                // adjust to new size and postion
                if (pos === 'relative') {
                    text.removeClass('hidden');
                }
                else {
                    text.css('height', input.outerHeight() + 2);
                    window.setTimeout(function() {
                        var padding = (input.innerWidth() - input.width()) / 2;
                        var text_width = text.outerWidth();
                        input.css('padding-right', text_width + padding);
                        text.css('left', input.outerWidth() - text_width + 3).css('top', 0).removeClass('hidden');
                    }, 1);
                }
            };
            update_inline_element();
            if (!input_group_text_resize_handler) {
                $(window).on('resize', function() {
                    update_inline_element();
                });
                input_group_text_resize_handler = true;
            }
        });

    }

    // ---------------------------------------------------------------------------------
    // transfer hidden inputs to other input fields
    // ---------------------------------------------------------------------------------
    // transfers hidden input field "target" to another input field and removes the target
    // supports select, input type=[text,number,range,password,checkbox]
    // attributes copied: ['id', 'name', 'min', 'max', 'step', 'placeholder', 'readonly', 'spellcheck', 'autocomplete']
    // attributes added: ['classex' => 'class']
    //
    // example of a hidden select input. the select options will be copied and the entire form-group removed
    // <select class="input-group-text form-select" id="shmp" name="shmp"><option value="0">Enable for Channel 0</option><option value="1" selected="">Enable for Channel 1</option></select>
    //
    // output
    // <div class="input-group-append"><select id="shmp" class="input-group-text form-select" name="shmp"><option value="0">Enable for Channel 0</option><option value="1" selected>Enable for Channel 1</option></select></div>
    //
    element.find('[data-action="transfer-hidden-field"]').each(function() {
        var target = $(this).data('target');
        var src = $(target);
        var dst = $(this);
        if (src.length) {
            var attributes = ['id', 'name', 'min', 'max', 'step', 'placeholder', 'readonly', 'spellcheck', 'autocomplete', 'classex']
            for(key in attributes) {
                var name = attributes[key];
                var val = src.attr(name);
                if (val !== undefined) {
                    if (name === 'classex') {
                        dst.addClass(val);
                    }
                    else {
                        dst.attr(name, val);
                    }
                }
            }
            if (src.prop('tagName').toLowerCase() == 'select') {
                // copy all options
                dst.html(src.html());
            }
            else  if (dst.attr('type') == 'checkbox') {
                // set checked if non zero
                dst.prop('checked', parseInt(src.val()) != 0);
            }
            else {
                // copy value
                dst.val(src.val());
            }
            dst.removeAttr('data-target').removeAttr('data-action');
            var form_group = src.parent();
            if (form_group.hasClass('form-group') && form_group.find('label').length == 0) {
                // remove entire form-group if it does not have a label
                form_group.remove();
            }
            else {
                src.remove();
            }
        }
        else {
            console.error('transfer-hidden-field target ' + target + ' not found');
        }
    });

    // ---------------------------------------------------------------------------------
    // alternative to <input type="number" class="form-control">
    // the range input is hidden until javascript initializes the pair by adding the
    // class show
    // the class custom-range for range input is optional
    //
    // events:
    // oninput and onchange of the text field are triggered when the slider is moved
    //
    // the input range can be hidden by removing the show class
    // $('div.input-text-range').removeClass('show');
    // $('div.input-text-range').addClass('show');
    //
    // <div class="form-control input-text-range">
    // <input type="text" name="name" value="750" max="2000" min="100" step="5" placeholder="750">
    // <input type="range" class="custom-range">
    // </div>
    // ---------------------------------------------------------------------------------
    element.find('.input-text-range').each(function() {
        var parent = $(this);
        var text = parent.find('input[type="text"]');
        var range = parent.find('input[type="range"]');
        var minVal = text.attr('min');
        var maxVal = text.attr('max');
        if (minVal === undefined || maxVal === undefined) {
            return;
        }
        // assign id to div and mark inputs as children
        $(this).attr('for', '#' + text.attr('id'));
        text.addClass('input-text-range-child');
        range.addClass('input-text-range-child');
        // copy attributes and value
        range.attr('min', minVal);
        range.attr('max', maxVal);
        range.attr('step', text.attr('step'));
        range.val(text.val());

        minVal = parseFloat(minVal);
        maxVal = parseFloat(maxVal);

        function validate_range(value, child) {
            if (value < minVal || value > maxVal) {
                console.debug(value, child, parent, minVal, maxVal);
                var dval = getIntFloatValue(text.attr('disabled-value'));
                if (dval !== undefined) {
                    if (dval == value) {
                        parent.removeClass('is-invalid');
                        return;
                    }
                }
                parent.addClass('is-invalid').removeClass('is-valid');
            }
            else {
                parent.removeClass('is-invalid');
            }
        }

        parent.addClass('show');
        text.on('input', function() {
            var value = text.val();
            range.val(value);
            validate_range(value, $(this));
        });
        range.on('input', function() {
            text.val(range.val());
            text.trigger('input');
        }).on('change', function() {
            text.val(range.val());
            text.trigger('change');
        });
    });


};



$(function() {
    $.formInitFunc();
});
