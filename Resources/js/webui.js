/**
 * Author: sascha_lammers@gmx.de
 */
$.webUIComponent = {
    websocket_uri: '/webui-ws',
    http_uri: '/webui-handler',
    container: $('#webui'),
    slider_fill_intensity: { left: 0.0, right: 0.7 }, // slider fill value. RGB alpha depending on the slider value (left:0.0 = 0%, right: 0.7 = 70%)

    prototypes: {
        webui_group_title_content: '<div class="{{column-type|col-12}}"><div class="webuicomponent title text-white bg-primary"><div class="row group"><div class="col-auto mr-auto"><h1>{{title}}</h1></div>{{webui-disconnected-icon}}<div class="col-auto mb-auto mt-auto">{{content}}</div></div></div></div>',
        webui_group_title: '<div class="{{column-type|col-12}}"><div class="webuicomponent title text-white bg-primary"><div class="row group"><div class="col-auto mr-auto mt-auto mb-auto"><h1>{{title}}</h1></div>{{webui-disconnected-icon}}</div></div></div>',
        webui_disconnected_icon: '<div class="col-auto lost-connection hidden" id="lost-connection"><span class="oi oi-bolt webui-disconnected-icon"></span></div>',
        webui_row: '<div class="row">{{content}}</div>',
        webui_col_12: '<div class="{{column-type|col-12}}"><div class="webuicomponent">{{content}}</div></div>',
        webui_col_2: '<div class="{{column-type|col-2}}"><div class="webuicomponent">{{content}}</div></div>',
        webui_col_3: '<div class="{{column-type|col-3}}"><div class="webuicomponent">{{content}}</div></div>',
        webui_sensor_badge: '<div class="webuicomponent"><div class="badge-sensor"><div class="row"><div class="col"><div class="outer-badge"><div class="inner-badge"><{{hb|h4}} class="data"><span id="{{id}}" class="value">{{value}}</span> <span class="unit">{{unit}}</span></{{hb|h4}}></div></div></div></div><div class="row"><div class="col title">{{title}}</div></div></div></div>',
        webui_sensor: '<div class="webuicomponent"><div class="sensor"><{{ht|h4}} class="title">{{title}}</{{hb|h1}}><{{hb|h1}} class="data"><span id="{{id}}" class="value">{{value}}</span> <span class="unit">{{unit}}</span></{{ht|h4}}></div></div>',
        webui_switch: '<div class="webuicomponent"><div class="switch"><div class="row"><div class="col"><input type="range" class="attribute-target"></div></div></div></div>',
        webui_named_switch: '<div class="webuicomponent"><div class="switch named-switch"><div class="row"><div class="col"><input type="range" class="attribute-target"></div></div><div class="row"><div class="col title">{{title}}</div></div></div></div>',
        webui_switch_top: '<div class="webuicomponent"><div class="switch named-switch"><div class="row"><{{ht|h4}} class="col title">{{title}}</{{ht|h4}}></div><div class="row"><div class="col"><input type="range" class="attribute-target"></div></div></div></div>',
        webui_slider: '<div class="webuicomponent"><div class="{{slider-type}}"><input type="range" class="attribute-target"></div></div>',
        webui_slider_top: '<div class="webuicomponent"><div class="switch named-switch"><div class="row"><{{ht|h4}} class="col title">{{title}}</{{ht|h4}}></div><div class="row"><div class="col"><div class="{{slider-type}}"><input type="range" class="attribute-target"></div></div></div></div></div>',
        webui_color_picker: '<div class="webuicomponent"><div class="{{slider-type}}"><input type="hidden" class="attribute-target" data-color-format="hex"><div class="rgb-color-picker"></div></div></div>',
        webui_screen: '<div class="webuicomponent"><div class="screen"><div class="row"><div class="col"><canvas class="attribute-target"></canvas></div></div></div></div>',
        webui_listbox: '<div class="webuicomponent"><div class="listbox"><div class="row"><{{th|h4}} class="col title">{{title}}</{{th|h4}}></div><div class="row"><div class="col"><select class="attribute-target">{{content}}</select></div></div></div></div>',
        webui_listbox_option: '<option{{index}}>{{value}}</option>',
        webui_button_group: '<div class="webuicomponent"><div class="button-group" role="group"><div class="row"><div class="col"><{{th|h4}} class="title">{{title}}</{{th|h4}}><div id="{{id}}">{{content}}</div></div></div></div></div>',
        webui_button_group_button: '<button class="btn btn-outline-primary" data-value="{{index}}">{{value}}</button>',
        webui_button_group_col: '<div class="btn-group-vertical btn-group-lg">{{content}}</div>'
    },

    init_functions: [],
    event_handlers: [],

    short: function(from) {
        var to = {};
        for(var key in from) {
            to[from[key]] = key;
        }
        return {from: from, to: to};
    } ({
        bs: 'binary_sensor',
        bg: 'button_group',
        c: 'columns',
        hs: 'has_switch',
        // rmin: 'range_min',
        // rmax: 'range_max',
        rt: 'render_type',
        ue: 'update_events',
        zo: 'zero_off',
        // ht: 'heading_top',
        // hb: 'heading_bottom'
    }),

    defaults: {
        row: {
            columns_per_row: 0
        },
        column: {
            group: { columns: 12 },
            switch: { min: 0, max: 1, columns: 2, zero_off: true, name: 0, attributes: [ 'min', 'max', 'name' ] },
            slider: { slider_type: 'slider', min: 0, max: 255, columns: 12, rmin: false, rmax: false, attributes: [ 'min', 'max', 'rmin', 'rmax' ] },
            color_slider: { slider_type: 'slider color-slider', min: 153000, max: 500000, attributes: [ 'min', 'max' ] },
            color_picker: { },
            listbox: { },
            sensor: { columns: 3, no_value: '-'},
            screen: { columns: 3, width: 128, height: 32, attributes: [ 'width', 'height' ] },
            binary_sensor: { columns: 2 },
            button_group: { columns: 3, buttons: [], row: 0 /*items per row*/, attributes: [ 'buttons' ] },
        }
    },
    components: {},
    group: null,
    // pending_requests: [],
    lock_publish: false,
    publish_queue: {},
    queue_default_timeout: 100,
    queue_end_slider_default_timeout: 500,
    retry_time: 500,
    selected_slider: null,
    //
    // create gradient depending in min/max/rmin/rmax value
    //
    create_slider_gradient: function(min, max, rmin, rmax) {
        min = parseFloat(min);
        max = parseFloat(max);
        rmin = (rmin === false || rmin === 'false') ? rmin = min : parseFloat(rmin);
        rmax = (rmax === false || rmax === 'false') ? rmax = max : parseFloat(rmax);
        var range = max - min;
        var start = Math.round(rmin * 10000 / range) / 100;
        var end = Math.round(rmax * 10000 / range) / 100;
		if (start <= 0 && end >= 100) {
			return ''; // remove gradient
		}
        var grad = 'linear-gradient(to right';
        if (start > 0) {
            grad += ',rgba(0,0,0,0.2) 0% ' + start + '%,'
        }
        else {
            start = 0
        }
        grad += 'rgba(0,0,0,0) ' + start + '%';
        if (end < 100) {
            grad += ' ' + end + '%,rgba(0,0,0,0.2) ' + end + '%';
        }
        return grad + ' 100%)';
    },
    //
    // get prototype and replace variables
    //
    get_prototype: function(name, vars) {
        // var self = this;
        // var replace = {}
        name = name.replace(/-/g, '_');

        if (!this.prototypes.hasOwnProperty(name)) {
            throw 'prototype ' + name + ' not found';
        }
        var prototype = this.prototypes[name];
        var replace = filter_objects(this.prototypes, vars, function(val) { return val !== null; });

        for(var key in replace) {
            regex = new RegExp('{{' + key.replace(/[_-]/g, '[_-]') + '(\|[^}]*)?}}', 'g');
            prototype = prototype.replace(regex, replace[key]);
            // self.console.log("replace ", regex, key, val);
        }
        // replace missing variables with default values
        prototype = prototype.replace(/{{[^\|]+\|([^}]*)}}/g, '\$1');

        var vars = $.matchAll(prototype, '{{(?<var>[^\|}]+)(?:\|[^}]*)?}}');
        if (vars.length) {
            console.log(name, vars, replace, prototype);
            throw 'variable(s) not found\nvariables=' + vars.join(', ') + '\nname';
        }

        // self.console.log('prototype', name, prototype, 'replaced', replace);
        return prototype;
    },
    //
    // show/hide connection lost icon
    //
    show_disconnected_icon: function(action) {
        var icon = $('#lost-connection');
        if (action == 'lost') {
            // display red icon
            icon.clearQueue().stop().removeClass('connected').show();
        }
        else if (action == 'connected') {
            // display green icon for 10 seconds if the connection was lost before
            if (icon.is(':visible')) {
                icon.clearQueue().stop().addClass('connected').show().delay(10000).hide();
            }
        }
        else if (!action) {
            icon.clearQueue().stop().hide();
        }
    },
    //
    // rename shortcuts in keys and values
    //
    rename_shortcuts: function(options) {
        // rename keys
        for (var i in this.short.from) {
            if (options.hasOwnProperty(i)) {
                options[this.short.from[i]] = options[i];
                delete options[i];
            }
        }
        // replace values
        for (var i in options) {
            if (this.short.from.hasOwnProperty(options[i])) {
                options[i] = this.short.from[options[i]];
            }
        }
    },
    //
    // apply defaults to all columns and calculate number of columns
    //
    prepare_options: function(options) {
        this.rename_shortcuts(options);
        if (options.hasOwnProperty('columns') && options.columns.length) {
            for(var i = 0; i < options.columns.length; i++) {
                this.rename_shortcuts(options.columns[i]);
            }
        }
        // merge defaults
        options = $.extend({}, this.defaults.row, options);
        var self = this;
        $(options.columns).each(function() {
            var defaults = self.defaults.column[this.type];
            if (this.type == 'slider') {
                switch(this.color) {
                    case 'rgb':
                        defaults = $.extend({}, defaults, self.defaults.column.color_picker);
                        this.type = 'color_picker'
                        break;
                    case 'temp':
                        defaults = $.extend({}, defaults, self.defaults.column.color_slider);
                        break;
                }
            }
            $.extend(this, $.extend({}, defaults, this));
            options.columns_per_row += this.columns;
        });
        self.row = options;
        return options;
    },
    //
    // add attributes listed in options.attributes to the element
    //
    // additional attributes are:
    //
    // id
    // class
    // data-group-id (and data[id[])
    // zero_off (applied to .webuicomponent)
    // height (applied to the first div child of .webuicomponent)
    //
    add_attributes: function(element, options) {
        //var component = this.components[options.id];
        if (element.length == 0) {
            return;
        }
        if (options.id) {
            element.attr('id', options.id);
        }
        if (options.class) {
            element.attr('class', options.class);
        }
        else {
            element.removeClass('attribute-target');
            if (element.attr('class') == '') {
                element.removeAttr('class');
            }
        }
        if (this.group) {
            element.data('group-id', this.group);
            element.attr('data-group-id', this.group);
        }
        var webui_component = element.closest('.webuicomponent');
        if (options.zero_off) {
            webui_component.addClass('zero-off');
            if (element[0].type == 'range' && !options.id.match(/^group-switch-/)) {
                element.addClass('slider-change-fill-intensity');
            }
        }
        if (options.hasOwnProperty('attributes') && options.attributes.length) {
            $.each(options.attributes, function() {
                var index = this.replace(/-/g, '_');
                if (options.hasOwnProperty(index)) {
                    element.attr(this, options[index]);
                }
                delete options.index;
            });
        }
        this.add_height(webui_component.find('div:first'), options);
    },
    //
    // add height to element
    //
    add_height: function(element, options) {
        if (options.height) {
            var height = options.height;
            if (Number.isInteger(height)) {
                height += 'px';
            }
            element.css('min-height', height);
        }
    },
    //
    // remove timeout and delete entries
    //
    queue_delete: function() {
        var self = this;
        $.each(this.publish_queue, function(key, queue) {
            if (queue.timeout) {
                window.clearTimeout(queue.timeout);
                queue.timeout = null;
            }
            delete self.publish_queue[key];
        });
    },
    //
    // returns true if the queue is done
    //
    queue_is_done: function(queue) {
        return !queue.update_lock && queue.next_timeout == 0 && queue.update['value'] === undefined && queue.update['set'] === undefined && queue.update['state'] === undefined;
    },
    //
    // get or create entry in the queue for element id
    //
    queue_get_entry: function(id) {
        if (this.publish_queue[id] === undefined) {
            this.publish_queue[id] = { timeout: null, update_lock: false, remove_update_lock: false, next_timeout: 0, values: {}, published: {}, update: {} };
        }
        return this.publish_queue[id];
    },
    //
    // add timeout to process the queue
    //
    add_queue_timeout: function(id, queue, timeout) {
        if (queue.timeout) {
            queue.next_timeout = timeout;
            return;
        }
        var callback = null;
        var self = this;
        var _callback = function() {
            if (self.lock_publish) {
                delete self.publish_queue[id];
                return;
            }
            // update element if not locked
            if (!queue.update_lock && queue.update['value'] !== undefined) {
                self.queue_update_element(id, $('#' + id), queue.update['value'], queue.update['state']);
                queue.update = {};
            }
            // publish queue if no global lock is set
            for (var key in queue.values) {
                if (queue.values.hasOwnProperty(key)) {
                    var value = queue.values[key];
                    var has_changed = queue.published[key] !== value;
                    // console.log('publish_if_changed id='+id+' key'+key+' value='+value+' has_changed='+has_changed);
                    if (has_changed) {
                        self.socket.send('+' + key + ' ' + id + ' ' + value);
                        // console.log('SOCKET_SEND +' + key + ' ' + id + ' ' + value);
                        queue.published[key] = value;
                    }
                }
            }
            queue.values = {};
            if (queue.remove_update_lock) {
                queue.remove_update_lock = false;
                queue.update_lock = false;
                self.console.log('remove update lock', id, queue);
            }
            // keep calling this function until the queue is done
            if (self.queue_is_done(queue)) {
                self.console.log('QUEUE DONE unlocked ', id, queue);
                self.console.log('unlocked ', id, queue);
                queue.timeout = null;
            }
            else {
                var timeout = self.queue_default_timeout;
                if (queue.next_timeout) {
                    timeout = queue.next_timeout;
                    queue.next_timeout = 0;
                }
                queue.timeout = window.setTimeout(callback, timeout);
            }
        };
        callback = _callback;
        queue.timeout = window.setTimeout(callback, timeout);
    },
    //
    // update element and trigger onChange
    // if updates are locked, add a timer to retry
    //
    queue_update_state: function(id, element, value, state) {
        // console.log('queue_update_state', id, element, value, state);
        if (!this.lock_publish) {
            var queue = this.queue_get_entry(id);
            // this.console.log('queue_update_state', id, value, state, this.lock_publish, queue);
            if (queue.update_lock) {
                queue.update['value'] = value;
                queue.update['state'] = state;
                this.add_queue_timeout(id, queue, this.queue_default_timeout);
                return;
            }
        }
        this.queue_update_element(id, element, value, state);
    },
    //
    // update element and trigger on change event
    // restore update_lock status
    // use_queue = false will not lock or update the queue
    //
    queue_update_element: function(id, element, value, state, use_queue) {
        if (arguments.length < 5) {
            use_queue = true;
        }
        // use publish queue or create dummy object
        var component = this.components[id];
        var webui_component = element.closest('.webuicomponent');

        // special handling for sensor
        if (component.type == 'sensor') {
            var data = webui_component.find('.data');
            var unit = webui_component.find('.unit');
            var hasUnit = unit.html().length != 0;
            if (this.has_value({value: value, state: state})) {
                if (hasUnit) { // if a unit is available, check if the value is a valid float. if not, hide unit and set class no-value
                    if (isNaN(parseFloat(value))) {
                        data.addClass('no-value');
                        unit.hide();
                    } else {
                        data.removeClass('no-value');
                        unit.show();
                    }
                }
                element.html(value);
                if ($('#system_time').length) {
                    system_time_attach_handler();
                }
            }
            else {
                if (hasUnit) { // if a unit is available, show "<N/A> <unit>" and set class to no-value
                    if (!data.hasClass('no-value')) {
                        data.find('.value').html(this.defaults.column.sensor.no_value);
                        data.addClass('no-value');
                        unit.show();
                    }
                }
            }
            return;
        }

        var queue = use_queue ? this.queue_get_entry(id) : {update_lock: false, published: {}};
        // console.log('queue_update_element', id, value, state, component.type, component);
        if (state !== undefined) {
            queue.published.state = state;
            if (component.type == 'button-group') {
                var buttons = $(element).find('button');
                (state != true) ? buttons.addClass('disabled') : buttons.removeClass('disabled');
            }
            else {
                element.prop('disabled', state != true);
            }
        }
        if (value !== undefined) {
            // console.log('queue_update_element', id, element, component.converter ? value + '=>' + component.converter.from(value) : value)
            if (component.converter) {
                value = component.converter.from(value);
            }
            if (use_queue) {
                queue.published.value = value;
            }
            if (component.type == 'button-group') {
                var buttons = $(element).find('button');
                buttons.removeClass('active');
                buttons.each(function() {
                    var btn = $(this);
                    if (btn.data('value') == value) {
                        btn.trigger('blur');
                        btn.addClass('active');
                    }
                });
            }
            else {
                var update_lock = queue.update_lock;
                queue.update_lock = true;
                element.val(value);
                if (use_queue) {
                    element.trigger('change');
                }
                if (component.type == 'slider' && component.hasOwnProperty('range_slider')) {
                    component.range_slider.$element.rangeslider('update', true);
                    this.slider_update_css(component.range_slider);
                }
                queue.update_lock = update_lock;
            }
        }
    },
    //
    // lock update and add timer to publish state
    //
    queue_publish_state: function(id, value, type, timeout) {
        if (this.lock_publish) {
            return;
        }
        var queue = this.queue_get_entry(id);
        var component = this.components[id];
        if (component.value !== undefined) {
            // console.log('queue_publish_state VALUE defined', id, value, type, timeout, component);
            return;
        }
        if (component.converter) {
            value = component.converter.to(value);
        }
        // console.log('queue_publish_state', id, value, type, timeout);
        queue.values[type] = value;
        this.add_queue_timeout(id, queue, timeout);
    },
    //
    // range_slider onchange callback
    //
    slider_callback: function (slider, value, onSlideEnd) {
        // console.log('slider_callback',this,slider);
        var input = slider.$element;
        var id = input.attr('id');
        var component = this.components[id];
        var queue = this.queue_get_entry(id);
        if (onSlideEnd) {
            queue.remove_update_lock = true;
            // this.slider_update_css(slider);
        }
        else {
            queue.update_lock = true;
        }
        // having value set prevents queue_publish_state from publishing the state
        // remove component.value when the value has changed
        if (component.type == 'group-switch' && component.value !== undefined && value != component.value) {
            delete component.value;
        }
        this.queue_publish_state(id, value, 'set', onSlideEnd ? (this.queue_default_timeout + 400) : this.queue_default_timeout);
        this.slider_update_css(slider);
    },
    //
    // update css for range_slider
    //
    slider_update_css: function(slider) {
        var input = slider.$element;
        var value = parseInt(input.val());
        var min = parseInt(input.attr('min'));
        var max = parseInt(input.attr('max'));
        var slider_range = max - min;
        if (this.slider_fill_intensity && input.hasClass('slider-change-fill-intensity')) {
            // change brightness of slider
            var fill_range = this.slider_fill_intensity.right - this.slider_fill_intensity.left;
            var alpha = (value / slider_range * fill_range) + this.slider_fill_intensity.left;
            var rgba = 'rgba(255, 255, 255, ' + alpha + ')';
            var rgba2x = rgba + ',' + rgba;
            var fill = slider.$fill;
            fill.css('background-image', '-webkit-gradient(linear,50% 0%, 50% 100%,color-stop(0%,' + rgba + '),color-stop(100%,' + rgba + '))');
            fill.css('background-image', '-moz-linear-gradient(' + rgba2x + ')');
            fill.css('background-image', '-webkit-linear-gradient(' + rgba2x + ')');
            fill.css('background-image', 'linear-gradient(' + rgba2x +')');
        }
        var handle = slider.$handle;
        var hasOffClass = handle.hasClass('off');
        var is_off = (input.attr('min') === undefined && input.val() == 0) || (input.val() === input.attr('min'));
        if (is_off && !hasOffClass) {
            handle.addClass('off');
        }
        else if (!is_off && hasOffClass) {
            handle.removeClass('off');
        }
        if (slider_range) {
            if (input.parent().hasClass('color-slider')) {
                handle.attr('title', (1000000000 / value).toFixed(0) + 'K');
            }
            else {
                handle.attr('title', ((value - min) * 100 / slider_range).toFixed(1) + '%');
            }
        }
    },
    //
    // slider key handler activated by clicking on the handle
    //
    slider_onkeyup: function(event) {
        var input = this.selected_slider;
        if (!input) {
            return;
        }
        var value, change, min, max;
        function getInputData() {
            value = parseInt(input.val());
            min = parseInt(input.attr('min'));
            max = parseInt(input.attr('max'));
            var range = max - min;
            change = Math.round(range / (event.ctrlKey ? 10 : 100));
        }
        // console.log(getInputData(), event, value, min, max, change);
        if (event.key === 'ArrowRight') {
            getInputData();
            input.val(value + change).trigger('change');
        }
        else if (event.key === 'ArrowLeft') {
            getInputData();
            input.val(value - change).trigger('change');
        }
        else if (event.key === 'ArrowUp') {
            getInputData();
            input.val(min).trigger('change');
        }
        else if (event.key === 'ArrowDown') {
            getInputData();
            input.val(max).trigger('change');
        }
    },
    //
    // items can be a json object or array
    // if parsing fails, the lists is treated as comma separated
    // if items is undefined or an empty string, an empty array is returned
    //
    parse_items: function(items) {
        if (items === undefined || items == '') {
            return [];
        }
        var list = null;
        if (items.match(/^[{\[]/)) {
            try {
                list = JSON.parse(items);
            } catch(e) {
                this.console.error(e, items);
                list = null;
            }
        }
        return (list === null) ? items.split(',') : list;
    },
    //
    // check if options have a value and state is not false
    //
    has_value: function(options) {
        return !(options.value === null || options.value === undefined || options.state === false);
    },
    //
    // element add methods
    //
    add_element_group: function(options) {
        var row_prototype = $(this.get_prototype('webui-row', { content:
            options.has_switch ?
                (prototype = this.get_prototype('webui-group-title-content', { title: options.title, content: this.get_prototype('webui-switch', {}) })) :
                (prototype = this.get_prototype('webui-group-title', options))
        }));
        var group_switch_id = 'group-switch-' + $('.webuicomponent .group-switch input').length;
        this.components[group_switch_id] = {type: 'group-switch'};
        this.add_attributes($(row_prototype).find('.attribute-target'), $.extend({ 'id': group_switch_id }, this.defaults.column.switch));
        row_prototype.find('.webuicomponent').addClass(options.has_switch ? 'group-switch' : 'group')
        this.group = group_switch_id;
        return row_prototype;
    },
    //
    // add sensor
    //
    add_element_sensor: function(options) {
        this.components[options.id] = {type: 'sensor'};
        var prototype_name = 'webui-sensor';
        if (options.render_type == 'badge') {
            prototype_name += '-badge';
        }
        if (options.value === undefined) {
            options.value = this.defaults.column.sensor.no_value;
        }
        var prototype = $(this.get_prototype(prototype_name, options));
        this.add_height(prototype.find('.sensor,.badge-sensor'), options);
        if (options.render_type == 'columns') {
            prototype.find('.value').parent().css('flex-direction', 'column');
        }
        return prototype;
    },
    //
    // add switch
    //
    add_element_switch: function(options) {
        var prototype;
        this.components[options.id] = {type: 'switch'};
        if (options.name == 2) {
            prototype = $(this.get_prototype('webui-switch-top', options));
        }
        else if (options.name == 1) {
            prototype = $(this.get_prototype('webui-named-switch', options));
        }
        else {
            prototype = $(this.get_prototype('webui-switch', options));
        }
        var element = prototype.find('.attribute-target');
        this.add_attributes(element, options);
        return prototype;
    },
    //
    // add (color-)range_slider
    //
    add_element_slider: function(options) {
        this.components[options.id] = {type: 'slider'};
        var prototype;
        if (options.name == 2) {
            prototype = $(this.get_prototype('webui-slider-top', options));
        }
        else {
            prototype = $(this.get_prototype('webui-slider', options));
        }
        var range = prototype.find('.attribute-target');
        this.add_attributes(range, options);
        return prototype;
    },
    //
    // ad rgb color picker
    //
    add_element_color_picker: function(options) {
        this.components[options.id] = {type: 'color_picker'};
        var prototype = $(this.get_prototype('webui-color-picker', options));
        var range = prototype.find('.attribute-target');
        this.add_attributes(range, options);
        return prototype;
    },
    //
    // add virtual screen
    //
    add_element_screen: function(options) {
        this.components[options.id] = {type: 'screen'};
        var prototype = $(this.get_prototype('webui-screen', options));
        this.add_attributes(prototype.find('.attribute-target'), $.extend({}, this.defaults.column.screen, options));
        return prototype;
    },
    //
    // add button group
    //
    // options.row          number of rows per column, 0 single column
    //
    add_element_button_group: function(options) {
        this.components[options.id] = {type: 'button-group'};
        options.content = '';
        var self = this;
        var count = 0;
        var items = this.parse_items(options.items);
        var content = '';
        $.each(items, function(key, val) {
            content += $(self.get_prototype('webui-button-group-button', { value: val, index: key }))[0].outerHTML;
            if (options.row && ++count % options.row == 0) {
                options.content += $(self.get_prototype('webui-button-group-col', { content: content }))[0].outerHTML;
                content = '';
            }
        });
        if (content != '') {
            options.content += $(self.get_prototype('webui-button-group-col', { content: content }))[0].outerHTML;
        }

        var prototype = $(this.get_prototype('webui-button-group', $.extend({}, this.defaults.column.button_group, options)));
        var buttons = prototype.find('button');
        this.add_attributes(buttons, { height: options.height });

        var id = options.id;
        buttons.on('click', function(event) {
            if ($(this).hasClass('disabled')) {
                event.preventDefault();
                return;
            }
            buttons.removeClass('active');
            self.queue_publish_state(id, $(this).data('value'), 'set', this.queue_default_timeout);
        });

        if (options.title.length == 0) {
            prototype.find('.title').remove();
        }
        return prototype;
    },
    //
    // add listbox
    //
    // options.height       height of <select>
    // options.row          size attribute of <select>
    // options.multi        true for <select multiple>
    //
    add_element_listbox: function(options) {
        // this.console.log(this.parse_items(options.items));
        this.components[options.id] = {type: 'listbox'};
        options.content = '';
        var self = this;
        var items = this.parse_items(options.items);
        var content = '';
        $.each(items, function(key, val) {
            index = ' value="' + key + '"';
            content += $(self.get_prototype('webui-listbox-option', { value: val, index: index }))[0].outerHTML;
        });
        options.content = content;

        var prototype = $(this.get_prototype('webui-listbox', $.extend({}, this.defaults.column.listbox, options)));
        var listbox = prototype.find('.attribute-target');
        this.add_attributes(listbox, { height: options.height });
        var select = prototype.find('select');
        this.add_attributes(select, $.extend({ class: 'custom-select' }, options));
        if (options.row) {
            select.attr('size', options.row);
        }
        if (options.multi) {
            select.attr('multiple', 'multiple');
        }
        select.on('change', function(event) {
            event.preventDefault();
            if (select.hasClass('disabled') || select.prop('disabled')) {
                return;
            }
            self.queue_publish_state(options.id, $(this).val(), 'set', this.queue_default_timeout);
        });
        if (options.title.length == 0) {
            prototype.find('.title').remove();
        }
        return prototype;
    },
    //
    // add row
    //
    // options.align            center: use class justify-content-center
    //
    add_element_row: function(options) {
        options = this.prepare_options(options);
        var row = $('<div></div>');
        row.addClass('row');
        if (options.align === "center") {
            row.addClass('justify-content-center');
        }
        for(key in options.columns) {
            var column = options.columns[key];
            var col = this.add_element(column);
            if (col) {
                var div = $('<div></div>');
                switch(column.columns) {
                    case 1:
                        div.addClass('col-lg-1 col-md-2 col-sm-2 col-3');
                        break;
                    case 2:
                        div.addClass('col-lg-2 col-md-3 col-sm-4 col-6');
                        break;
                    case 3:
                        div.addClass('col-lg-3 col-md-4 col-sm-6 col-12');
                        break;
                    case 4:
                        div.addClass('col-lg-4 col-md-4 col-sm-6 col-12');
                        break;
                    case 6:
                        div.addClass('col-lg-6 col-md-6 col-sm-12 col-12');
                        break;
                    default:
                        if (column.columns) {
                            div.addClass('col-lg-' + (column.columns % 100));
                        } else {
                            div.addClass('col-auto');
                        }
                        break;
                }
                row.append(div.append(col));
            }
        }
        this.container.append(row);
        // var self = this;
        // $(options.columns).each(function(idx) {
        //     // add extra column to align other columns on the right
        //     if (idx == 0 && options.align === "right" & options.columns_per_row < 12) {
        //         colClass += ' offset-md-' + (12 - options.columns_per_row);
        //     }
        //     html += '<div class="' + colClass +'">';
        //     html += self.add_element(this)[0].outerHTML;
        //     html += '</div>';
        // });
        // // add extra column to fill the row
        // if (options.columns_per_row < 12 && options.align === "left") {
        //     html += '<div class="offset-md-' + (12 - options.columns_per_row) + '">';
        //     html += '</div>';
        // }
        // html += '</div>';
        // this.container.append(html);
    },
    //
    // add element by type
    //
    add_element: function (options) {
        switch(options.type) {
            case 'group':
                return this.add_element_group(options);
            case 'binary_sensor':
            case 'sensor':
                return this.add_element_sensor(options);
            case 'switch':
                return this.add_element_switch(options);
            case 'color_picker':
                return this.add_element_color_picker(options);
            case 'slider':
                return this.add_element_slider(options);
            case 'screen':
                return this.add_element_screen(options);
            case 'button_group':
                return this.add_element_button_group(options);
            case 'listbox':
                return this.add_element_listbox(options);
            case 'row':
                this.add_element_row(options);
                return
        }
        throw 'unknown element type ' + options.type;
    },
    //
    // update_ui gets called when receiving the ui from the web server
    //
    update_ui: function(json) {
        this.console.log('update_ui', arguments);

        this.lock_publish = true;
        this.components = {};
        this.group = null;
        this.container.html('');

        var self = this;
        $(json).each(function() {
            self.add_element(this);
        });
        this.container.append('<div class="p-2">&nbsp;<div>');

        if (this.container.find('#system_time').length) {
            system_time_attach_handler();
        }

        // console.log('components', this.components);
    },
    //
    // call finalize_ui after update_ui and update_events
    //
    finalize_ui: function() {
        var self = this;
        // process color-picker
        this.container.find('input[data-color-format="hex"]').each(function() {
            var id = $(this).attr('id');
            var input = $(this);
            var flat = true;
            var preview = null;
            var color_picker;
            if (flat == false) {
                color_picker = $(this);
                color_picker.parent().append('<span id=' + id + '"_preview" style="padding:1rem">&nbsp;</span>');
                preview = $('#' + id + '_preview');
                input.attr('type', 'text');
            }
            else {
                color_picker = $(this).parent().find('.rgb-color-picker');
            }
            self.components[id].color_picker = { input: input, color_picker: color_picker };
            color_picker.ColorPickerSliders({
                color: input.val(),
                customswatches: id + '_swatches',
                swatches: ['FFFFFF', '000000', 'FF0000', '800000', 'FFFF00', '808000', '00FF00', '008000', '00FFFF', '008080', '0000FF', '000080', 'FF00FF', '800080'],
                grouping: false,
                connectedinput: input,
                flat: flat,
                previewformat: 'hex',
                labels: {
                    hslhue: 'Hue',
                    hslsaturation: 'Saturation',
                    hsllightness: 'Lightness',
                },
                order: {
                    hsl: 1,
                    preview: 2
                },
                onchange: function(container, color) {
                    if (preview) {
                        preview.css('background-color', color.tiny.toRgbString())
                    }
                    self.queue_publish_state(id, parseInt(color.tiny.toHex(), 16), 'set', this.queue_default_timeout);
                }
            });
            // var icons = color_picker.parent().find('.glyphicon');
            // $(icons.get(0)).attr('class', 'oi oi-plus');
            // $(icons.get(1)).attr('class', 'oi oi-trash');
            // $(icons.get(2)).attr('class', 'oi oi-reload');
            // self.console.log('color_picker', input.val(), color_picker);
        });
        // process range-slider
        this.container.find('input[type="range"]').each(function() {
            var range_slider = null;
            var options = {
                polyfill : false,
                onSlideEnd: function(position, value) {
                    self.slider_callback(this, value, true);
                    // self.slider_update_css(range_slider);
                }
            };
            var input = $(this);
            if (input.attr('min') !== undefined || input.attr('max') !== undefined) {
                options.onSlide = function(position, value) {
                    self.slider_callback(this, value, false);
                }
            }
            input.rangeslider(options);
            var next = input.next();
            range_slider = { '$element': input, '$handle': next.find('.rangeslider__handle'), '$fill': next.find('.rangeslider__fill') };
            $(range_slider.$handle).on('click', function(event) {
                event.preventDefault();
                self.selected_slider = input;
            });
            self.components[input.attr('id')].range_slider = range_slider;
            self.slider_update_css(range_slider);
            if (input.attr('rmin') !== undefined && input.attr('rmax') !== undefined) {
                next.css('background-image', self.create_slider_gradient(input.attr('min'), input.attr('max'), input.attr('rmin'), input.attr('rmax')));
            }
        });
        for(var i = 0; i < this.init_functions.length; i++) {
            this.init_functions[i].call(this);
        }
        this.queue_delete();
        this.lock_publish = false;
    },
    //
    // process events to update displayed data
    //
    update_events: function(events, init) {
        // this.console.log("update_events", events);
        if (!init) {
            this.lock_publish = true;
        }
        var self = this;
        $(events).each(function() {
            if (this.hasOwnProperty('i')) {
                this.id = this.i;
            }
            if (this.hasOwnProperty('v')) {
                this.value = this.v;
            }
            if (this.hasOwnProperty('s')) {
                this.state = this.s;
            }
            var element = $('#' + this.id);
            if (element.length) {
                // this.group is the value for the group switch
                if (this.group !== undefined) {
                    // console.log("GROUP", component.type, this.id, this.value, component, this);
                    var group_component = self.components[element.data('group-id')];
                    var group_switch = $('#' + element.data('group-id'));
                    group_component.value = this.group;
                    group_switch.val(this.group).trigger('change');
                    // if (group_component.type == 'range_slider') {
                    //     self.slider_update_css(group_component.range_slider);
                    // }
                }
                // update rmin/rmax values for sliders
                if (self.components[this.id].type === 'slider') {
                    var update_css = false;
                    if (this.hasOwnProperty('rmin')) {
                        var rmin = element.attr('rmin');
                        if (this.rmin != rmin) {
                            element.attr('rmin', this.rmin);
                            update_css = true;
                        }
                    }
                    if (this.hasOwnProperty('rmax')) {
                        var rmax = element.attr('rmax');
                        if (this.rmax != rmax) {
                            element.attr('rmax', this.rmax);
                            update_css = true;
                        }
                    }
                    if (update_css) {
                        element.next().css('background-image', self.create_slider_gradient(element.attr('min'), element.attr('max'), element.attr('rmin'), element.attr('rmax')));
                    }
                }
                // queue update value and state
                self.queue_update_state(this.id, element, this.value, this.state, !init)
            }
        });
        if (!init) {
            this.lock_publish = false;
        }
    },
    //
    // request webui and initial data from web server
    //
    request_ui: function() {
        this.console.log('request_ui', arguments);
        var url = $.getHttpLocation(this.http_uri);
        var SID = $.getSessionId();
        this.retry_time = 500;
        var self = this;
        $.get(url + '?SID=' + SID, function(data) {
            self.console.debug('get ', this.http_uri, data);
            self.update_ui(data.data);
            self.update_events(data.values, true);
            self.finalize_ui();
            self.show_disconnected_icon('connected');
        }, 'json').fail(function(error) {
            self.console.error('request_ui get error', error, 'retry_time', self.retry_time);
            if (self.retry_time < 5000) {
                window.setTimeout(function() {
                    self.request_ui();
                }, self.retry_time);
                self.retry_time += 1000;
            }
        });
    },
    //
    // web socket handler
    //
    socket_handler: function(event) {
        if (event.type == 'close' || event.type == 'error') {
            this.show_disconnected_icon('lost');
        }
        else if (event.type == 'auth') {
            //event.socket.send('+GET_VALUES');
            this.request_ui();
        }
        else if (event.type == 'data') {
            try {
                var json = JSON.parse(event.data);
                if (json.type === this.short.to.update_events) {
                    for(var i = 0; i < this.event_handlers.length; i++) {
                        for(var j = 0; j < json.events.length; j++) {
                            this.event_handlers[i](json.events[j]);
                        }
                    }
                    this.update_events(json.events);
                }
            } catch(e) {
                this.console.error('failed to parse json string', e, 'event.data', event)
            }
        }
        else if (event.type == 'object') {
            if (event.data.type === this.short.to.update_events) {
                this.update_events(event.data.events);
            }
        }
    },
    //
    // init webui
    //
    init: function() {
        this.console.log('init', arguments);
        var url = $.getWebSocketLocation(this.websocket_uri);
        var SID = $.getSessionId();
        var self = this;
        this.socket = new WS_Console(url, SID, 1, function(event) {
            event.self = self;
            self.socket_handler(event);
        });
        $(window).on('keyup', function(event) {
            self.slider_onkeyup(event);
        }).on('click', function(event) {
            if (!event.isDefaultPrevented()) {
                self.selected_slider = null;
            }
        });


        if (this.console.is_debug_enabled()) {
            $('body').append('<textarea id="console" style="width:350px;height:150px;font-size:10px;font-family:consolas;z-index:999;position:fixed;right:5px;bottom:5px"></textarea>');
            this.socket.setConsoleId("console");
        }
        this.socket.connect();
    }
};
dbg_console.register('$.webUIComponent', $.webUIComponent);

if ($('#webui').length) {
    $(function() {
        $.webUIComponent.init();
    });
}
