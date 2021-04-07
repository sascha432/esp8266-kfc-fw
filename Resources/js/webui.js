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
        webui_switch_top: '<div class="webuicomponent"><div class="switch named-switch"><div class="row"><{{ht|h4}} class="col title">{{title}}</{{ht|h4}}></div><div class="row"><div class="col"><input type="range" class="attribute-target"></div></div></div></div>',        webui_slider: '<div class="webuicomponent"><div class="{{slider-type}}"><input type="range" class="attribute-target"></div></div>',
        webui_screen: '<div class="webuicomponent"><div class="screen"><div class="row"><div class="col"><canvas class="attribute-target"></canvas></div></div></div></div>',
        webui_listbox: '<div class="webuicomponent"><div class="listbox"><div class="row"><div class="col title">{{title}}</div></div><div class="row"><div class="col"><select class="attribute-target">{{content}}</select></div></div></div></div>',
        webui_button_group: '<div class="webuicomponent"><div class="button-group" role="group"><div class="row"><div class="col"><{{th|h4}} class="title">{{title}}</{{th|h4}}><div id="{{id}}">{{content}}</div></div></div></div></div>',
        webui_button_group_button: '<button class="btn btn-outline-primary" data-value="{{index}}">{{value}}</button>',
        webui_button_group_col: '<div class="btn-group-vertical btn-group-lg">{{content}}</div>'
    },

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
            switch: { min: 0, max: 1, columns: 2, zero_off: true, name: 0, attributes: [ 'min', 'max', 'value', 'name' ] },
            slider: { slider_type: 'slider', min: 0, max: 255, columns: 12, rmin: false, rmax: false, attributes: [ 'min', 'max', 'rmin', 'rmax', 'value' ] },
            color_slider: { slider_type: 'slider coplor-slider', min: 15300, max: 50000 },
            rgb_slider: { slider_type: 'slider rgb-slider', min: 0, max: 0xffffff },
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

    // .webuicomponent > .slider.rgb-slider .rangeslider--horizontal
    rgb_slider_colors: [ [255, 0, 0], [255, 125, 0], [255, 255, 0], [125, 255, 0], [0, 255, 0], [0, 255, 125], [0, 255, 255], [0, 125, 255], [0, 0, 255] ],
    //
    // convert slider value to RGB
    // the slider must have min="0" and max="16777215"
    //
    get_rgb_color: function(value) {
        function mix(from, to, frac) {
            return parseInt((to - from) * frac + from);
        }
        var offset = (this.rgb_slider_colors.length - 1) * (value / 16777216);
        var index = Math.min(Math.floor(offset), this.rgb_slider_colors.length - 2);
        var next = this.rgb_slider_colors[index + 1];
        var current = this.rgb_slider_colors[index];
        offset -= index;
        var r = mix(current[0], next[0], offset);
        var g = mix(current[1], next[1], offset);
        var b = mix(current[2], next[2], offset);

        // var val2 = (r << 16) | (g << 8) | b;
        // console.log(value, val2, this.get_value_from_color(val2));
        // this.get_value_from_color(val2);

        return (r << 16) | (g << 8) | b;
    },
    //
    // convert RGB to slider value
    get_value_from_color: function(value) {
        var r = value >> 16;
        var g = (value >> 8) & 0xff;
        var b = value & 0xff;
        var start = null;
        var end = null;
        for(var i = 0; i < this.rgb_slider_colors.length - 1; i++) {
            var current = this.rgb_slider_colors[i];
            var next = this.rgb_slider_colors[i + 1];
            if (start === null && r >= current[0] && g >= current[1] && b >= current[2]) {
                start = i;
            }
            if (r <= next[0] && g <= next[1] && b <= next[2]) {
                if (start !== null && i > start) {
                    end = i;
                    break;
                }
            }
    }
        // console.log('#', r, g, b, 'index', start, end);
        return 0;
    },

    create_slider_gradient: function(min, max, rmin, rmax) {
        min = parseFloat(min);
        max = parseFloat(max);
        rmin = (rmin === false) ? rmin = min : parseFloat(rmin);
        rmax = (rmax === false) ? rmax = max : parseFloat(rmax);
        range = max - min;
        start = Math.round(rmin * 10000  / range) / 100;
        end = Math.round(rmax * 10000  / range) / 100;
        var grad = 'linear-gradient(to right';
        if (start > 1) {
            grad += ', rgba(0,0,0,0.2) 0% ' + start + '%,'
        }
        else {
            start = 0
        }
        grad += ' rgba(0,0,0,0) ' + start + '%';
        if (end < 100) {
            grad += ' ' + end + '%, rgba(0,0,0,0.2) ' + end + '%';
        }
        grad += ' 100%';
        return grad + ')';
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
                        defaults = $.extend(defaults, self.defaults.column.rgb_slider);
                        break;
                    case 'temp':
                        defaults = $.extend(defaults, self.defaults.column.color_slider);
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
            element.removeClass('class', 'attribute-target');
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
            if (!options.id.match(/^group-switch-/)) {
                element.addClass('slider-change-fill-intensity');
            }
        }
        if (options.attributes !== undefined && options.attributes.length) {
            $.each(options.attributes, function() {
                var index = this.replace(/-/g, '_');
                if (options[index] !== undefined) {
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
                    self.console.log('publish_if_changed ', key, 'value', value, 'has_changed', has_changed);
                    if (has_changed) {
                        self.socket.send('+' + key + ' ' + id + ' ' + value);
                        console.log('SOCKET_SEND +' + key + ' ' + id + ' ' + value);
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
            this.console.log('queue_update_state', id, value, state, this.lock_publish, queue);
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
    // update element and trigger onChange
    // restore update_lock status
    //
    queue_update_element: function(id, element, value, state) {
        // console.log('queue_update_element', id, element, value, state);
        var queue = this.queue_get_entry(id);
        var component = this.components[id];
        if (state !== undefined) {
            queue.published.state = state;
            if (component.type == 'button-group') {
                var buttons = $(element).find('button');
                (state != true) ? buttons.addClass('disabled') : buttons.removeClass('disabled');
            } else {
                element.prop('disabled', state != true);
            }
        }
        if (value !== undefined) {
            if (component.converter) {
                value = component.converter.from(value);
            }
            queue.published.value = value;
            if (component.type == 'button-group') {
                var buttons = $(element).find('button');
                buttons.removeClass('active');
                buttons.each(function() {
                    if ($(this).data('value') == value) {
                        $(this).addClass('active');
                    }
                });
            }
            else {
                var update_lock = queue.update_lock;
                element.val(value).trigger('change');
                this.slider_update_css(component.range_slider);
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

    slider_callback: function (slider, value, onSlideEnd) {
        this.slider_update_css(slider);
        var id = element.attr('id');
        var component = this.components[id];
        //console.log('slider_callback ',id, component.type, element);
        var queue = this.queue_get_entry(id);
        if (onSlideEnd == false) {
            queue.update_lock = true;
        }
        else {
            queue.remove_update_lock = true;
        }
        // having value set prevents queue_publish_state from publishing the state
        // remove component.value when the value has changed
        if (component.type == 'group-switch' && component.value !== undefined && value != component.value) {
            delete component.value;
        }
        this.queue_publish_state(id, value, 'set', onSlideEnd ? (this.queue_default_timeout + 400) : this.queue_default_timeout);
    },

    slider_update_css: function(slider) {
        if (slider === undefined || slider == null) {
            return;
        }
        if (slider.$element !== undefined) {
            element = slider.$element;
            handle = slider.$handle;
            fill = slider.$fill;
        }
        else {
            element = slider.element;
            handle = slider.handle;
            fill = slider.fill;
        }
        var value = parseInt(element.val());
        if (this.slider_fill_intensity && element.hasClass('slider-change-fill-intensity')) {
            // change brightness of slider
            var fill_range = this.slider_fill_intensity.right - this.slider_fill_intensity.left;
            var slider_range = parseInt(element.attr('max') - parseInt(element.attr('min')));
            var alpha = (value / slider_range * fill_range) + this.slider_fill_intensity.left;
            var rgba = 'rgba(255, 255, 255, ' + alpha + ')';
            var rgba2x = rgba + ',' + rgba;
            fill.css('background-image', '-webkit-gradient(linear,50% 0%, 50% 100%,color-stop(0%,' + rgba + '),color-stop(100%,' + rgba + '))');
            fill.css('background-image', '-moz-linear-gradient(' + rgba2x + ')');
            fill.css('background-image', '-webkit-linear-gradient(' + rgba2x + ')');
            fill.css('background-image', 'linear-gradient(' + rgba2x +')');
        }
        var hasOffClass = handle.hasClass('off');
        if (value == 0 && !hasOffClass) {
            handle.addClass('off');
        } else if (value != 0 && hasOffClass) {
            handle.removeClass('off');
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
        this.add_attributes($(row_prototype).find('.attribute-target'), $.extend({ 'id': group_switch_id }, this.defaults.column.switch));

        row_prototype.find('.webuicomponent').addClass(options.has_switch ? 'group-switch' : 'group')

        this.components[group_switch_id] = {type: 'group-switch'};
        this.group = group_switch_id;
        return row_prototype;
    },
    add_element_sensor: function(options) {
        this.components[options.id] = {type: 'sensor'};
        var prototype_name = 'webui-sensor';
        if (options.render_type == 'badge') {
            prototype_name += '-badge';
        }
        var has_no_value = !this.has_value(options);
        if (has_no_value) {
            options.value = this.defaults.column.sensor.no_value;
        }
        var prototype = $(this.get_prototype(prototype_name, options));
        if (has_no_value) {
            prototype.find('.data').addClass('no-value');
        }
        this.add_height(prototype.find('.sensor,.badge-sensor'), options);
        if (options.render_type == 'columns') {
            prototype.find('.value').parent().css('flex-direction', 'column');
        }
        return prototype;
    },
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
        this.add_attributes(prototype.find('.attribute-target'), options);
        return prototype;
    },
    add_element_slider: function(options) {
        this.components[options.id] = {type: 'slider'};
        if (options.color == 'rgb') {
            var self = this;
            this.components[options.id].converter = {
                from: function(value) {
                    console.log("converter from "+value);
                    return value;
                },
                to: function(value) {
                    value = self.get_rgb_color(value);
                    // r = value >> 16;
                    // g = (value >> 8) & 0xff;
                    // b = value & 0xff;
                    // $('#webui > div:nth-child(1) > div > div > div > div> div> div').css('background-color', 'rgb('+r+','+g+','+b+')');
                    return value;
                }
            };
        }
        var prototype = $(this.get_prototype('webui-slider', options));
        var range = prototype.find('.attribute-target');
        this.add_attributes(range, options);
        return prototype;
    },
    add_element_screen: function(options) {
        this.components[options.id] = {type: 'screen'};
        var prototype = $(this.get_prototype('webui-screen', options));
        this.add_attributes(prototype.find('.attribute-target'), $.extend({}, this.defaults.column.screen, options));
        return prototype;
    },
    add_element_button_group: function(options) {
        this.components[options.id] = {type: 'button-group'};

        options.content = '';
        var self = this;
        var count = 0;
        var items = this.parse_items(options.items);
        var content = '';
        $(items).each(function(key, val) {
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
        this.add_attributes(buttons, {height: options.height});

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
    add_element_listbox: function(options) {
        this.console.log(this.parse_items(options.items));
        return $('<div></div>');
    },
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

    add_element: function (options) {
        switch(options.type) {
            case 'group':
                return this.add_element_group(options);
            case 'binary_sensor':
            case 'sensor':
                return this.add_element_sensor(options);
            case 'switch':
                return this.add_element_switch(options);
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

        this.container.find('input[type="range"]').each(function() {
            var options = {
                polyfill : false,
                onSlideEnd: function(position, value) {
                    self.slider_callback(this, value, true);
                }
            };
            if ($(this).attr('min') != 0 || $(this).attr('max') != 1) {
                options.onSlide = function(position, value) {
                    self.slider_callback(this, value, false);
                }
            }
            var input = $(this).rangeslider(options);
            var next = input.next();
            var range_slider = { element: input, handle: next.find('.rangeslider__handle'), fill: next.find('.rangeslider__fill') };
            self.components[$(this).attr('id')].range_slider = range_slider;
            self.slider_update_css(range_slider);
            if (input.attr('rmin') || input.attr('rmax')) {
                next.css('background-image', self.create_slider_gradient(input.attr('min'), input.attr('max'), input.attr('rmin'), input.attr('rmax')));
            }
        });

        if (this.container.find('#system_time').length) {
            system_time_attach_handler();
        }

        this.queue_delete();
        this.lock_publish = false;
    },

    update_events: function(events) {
        this.console.log("update_events", events);
        this.lock_publish = true;
        var self = this;
        $(events).each(function() {
            var element = $('#' + this.id);
            if (element.length) {
                var component_type = self.components[this.id]['type'];
                if (component_type == 'sensor') {
                    var webui_component = $('#' + this.id).closest('.webuicomponent');
                    if (self.has_value(this)) {
                        webui_component.find('.data').removeClass('no-value');
                        element.html(this.value);
                        if ($('#system_time').length) {
                            system_time_attach_handler();
                        }
                    }
                    else {
                        var data = webui_component.find('.data');
                        if (!data.hasClass('no-value')) {
                            data.addClass('no-value');
                            data.find('.value').html(self.defaults.column.sensor.no_value);
                        }
                    }
                }
                else {
                    // this.group is the value for the group switch
                    if (this.group !== undefined) {
                        var component = self.components[element.data('group-id')];
                        var group_switch = $('#' + element.data('group-id'));
                        component.value = this.group;
                        group_switch.val(this.group).trigger('change');
                        if (component.range_slider) {
                            self.slider_update_css(component.range_slider);
                        }
                    }
                    self.queue_update_state(this.id, element, this.value, this.state)
                }
            }
        });

        this.lock_publish = false;
    },

    request_ui: function() {
        this.console.log('request_ui', arguments);
        var url = $.getHttpLocation(this.http_uri);
        var SID = $.getSessionId();
        this.retry_time = 500;
        var self = this;
        $.get(url + '?SID=' + SID, function(data) {
            self.console.debug('get ', this.http_uri, data);
            self.update_ui(data.data);
            self.update_events(data.values);
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

    handleRLECompressedBitmap: function(data, pos) {

        var x, y, width, height, ctx, image, palette = [], paletteCount, writePos = 0, maxWritePos;
        try {

            var len = new Uint8Array(data, pos++, 1)[0];
            var canvasId = new Uint8Array(data, pos, len);
            pos += canvasId.byteLength;
            var canvasIdStr = new TextDecoder("utf-8").decode(canvasId);
            var canvas = $('#' + canvasIdStr);
            if (canvas.length == 0) {
                this.console.error('cannot find canvas for', canvasIdStr);
                return;
            }
            ctx = canvas[0].getContext('2d');
            if (!ctx) {
                this.console.error('cannot get 2d context for', canvas);
                return;
            }
            if (pos % 2) { // word alignment
                pos++;
            }
            var tmp = new Uint16Array(data, pos, 5);
            pos += tmp.byteLength;
            x = tmp[0];
            y = tmp[1];
            width = tmp[2];
            height = tmp[3];
            paletteCount = tmp[4];
            if (paletteCount) {
                var palettergb565 = new Uint16Array(data, pos, paletteCount);
                pos += palettergb565.byteLength;
                for (var i = 0; i < palettergb565.length; i++) {
                    palette.push($._____rgb565_to_888(palettergb565[i]));
                }
            }
            image = ctx.createImageData(width, height);
            maxWritePos = width * height * 4;

        } catch(e) {
            this.console.error('failed to decode header', e);
            return;
        }

        try {
            function copy_rle_color(color) {
                do {
                    image.data[writePos++] = color[0];
                    image.data[writePos++] = color[1];
                    image.data[writePos++] = color[2];
                    image.data[writePos++] = 0xff;
                    if (writePos > maxWritePos) {
                        throw 'image size exceeded';
                    }
                } while(rle--);
            }

            if (paletteCount) {
                while(pos < data.byteLength) {
                    var tmp = new Uint8Array(data, pos++, 1)[0];
                    var index = (tmp >> 4); // palette index
					tmp &= 0xf;
                    if (tmp == 0xe) { // 8 bit data marker
                        rle = new Uint8Array(data, pos++, 1)[0] + 0xe;
                    } else if (tmp == 0xf) { // 15 bit data marker, low-hi-byte
                        var tmp = new Uint8Array(data, pos, 2);
                        pos += tmp.byteLength;
                        rle = tmp[0] | (tmp[1] << 8);
                    }
                    else { // 4 bit data
                        rle = tmp;
                    }
                    copy_rle_color(palette[index]);
                }
            }
            else {
                while(pos < data.byteLength) {
                    var rle = new Uint8Array(data, pos++, 1)[0];
                    if (rle & 0x80) { // marker for 15bit, hi-low-byte
                        rle = ((rle & 0x7f) << 8) | (new Uint8Array(data, pos++, 1)[0]);
                    }
                    var tmp = new Uint8Array(data, pos, 2); // 16bit color rgb565
                    pos += tmp.byteLength;
                    copy_rle_color($._____rgb565_to_888((tmp[1] << 8) | tmp[0]));
                }
            }

        } catch(e) {
            this.console.error('failed to decode image data', e);
        }

        ctx.putImageData(image, x, y);
    },

    socket_handler: function(event) {
        if (event.type == 'close' || event.type == 'error') {
            this.show_disconnected_icon('lost');
        }
        else if (event.type == 'auth') {
            //event.socket.send('+GET_VALUES');
            this.request_ui();
        }
        else if (event.data instanceof ArrayBuffer) {
            var packetId = new Uint16Array(event.data, 0, 1);
            if (packetId == 0x0001) {// RGB565_RLE_COMPRESSED_BITMAP
                this.handleRLECompressedBitmap(event.data, packetId.byteLength);
            } else {
                this.console.log('unknown packet id: ' + packetId)
            }
        }
        else if (event.type == 'data') {
            try {
                var json = JSON.parse(event.data);
                if (json.type === this.short.to.update_events) {
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

    init: function() {
        this.console.log('init', arguments);
        var url = $.getWebSocketLocation(this.websocket_uri);
        var SID = $.getSessionId();
        var self = this;
        this.socket = new WS_Console(url, SID, 1, function(event) {
            event.self = self;
            self.socket_handler(event);
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
