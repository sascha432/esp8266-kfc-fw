/**
 * Author: sascha_lammers@gmx.de
 */

$.webUIComponent = {
    websocket_uri: '/webui-ws',
    http_uri: '/webui-handler',
    container: $('#webui'),
    slider_fill_intensity: { left: 0.0, right: 0.7 }, // slider fill value. RGB alpha depending on the slider value (left:0.0 = 0%, right: 0.7 = 70%)

    prototypes: {
        webui_group_title_content: '<div class="{{column-type}}-12"><div class="webuicomponent title text-white bg-primary"><div class="row group"><div class="col-auto mr-auto"><h1>{{title}}</h1></div>{{webui-disconnected-icon}}<div class="col-auto mb-auto mt-auto">{{content}}</div></div></div></div>',
        webui_group_title: '<div class="{{column-type}}-12"><div class="webuicomponent title text-white bg-primary"><div class="row group"><div class="col-auto mr-auto mt-auto mb-auto"><h1>{{title}}</h1></div>{{webui-disconnected-icon}}</div></div></div>',
        webui_disconnected_icon: '<div class="col-auto lost-connection hidden" id="lost-connection"><span class="oi oi-bolt webui-disconnected-icon"></span></div>',
        webui_row: '<div class="row">{{content}}</div>',
        webui_col_12: '<div class="{{column-type}}-12"><div class="webuicomponent">{{content}}</div></div>',
        webui_col_2: '<div class="{{column-type}}-2"><div class="webuicomponent">{{content}}</div></div>',
        webui_col_3: '<div class="{{column-type}}-3"><div class="webuicomponent">{{content}}</div></div>',
        webui_sensor_badge: '<div class="webuicomponent"><div class="badge-sensor"><div class="row"><div class="col"><div class="outer-badge"><div class="inner-badge"><{{hb|h4}} class="data"><span id="{{id}}" class="value">{{value}}</span> <span class="unit">{{unit}}</span></{{hb|h4}}></div></div></div></div><div class="row"><div class="col title">{{title}}</div></div></div></div>',
        webui_sensor: '<div class="webuicomponent"><div class="sensor"><{{ht|h4}} class="title">{{title}}</{{hb|h1}}><{{hb|h1}} class="data"><span id="{{id}}" class="value">{{value}}</span> <span class="unit">{{unit}}</span></{{ht|h4}}></div></div>',
        webui_switch: '<div class="switch"><input type="range" class="attribute-target"></div>',
        webui_slider: '<div class="webuicomponent"><div class="slider"><input type="range" class="attribute-target"></div></div>',
    },


    defaults: {
        row: {
            columns_per_row: 0
        },
        column: {
            group: { columns: 12 },
            switch: { min: 0, max: 1, columns: 2, zero_off: true, display_name: false, attributes: [ 'min', 'max', 'value', 'display-name' ] },
            slider: { min: 0, max: 255, columns: 12, attributes: [ 'min', 'max', 'value' ] },
            color_slider: { min: 15300, max: 50000 },
            sensor: { columns: 3, height: 0, no_value: '-',  attributes: [ 'height' ] },
            screen: { columns: 3, width: 128, height: 32, attributes: [ 'width', 'height' ] },
            binary_sensor: { columns: 2 },
            buttons: { columns: 3, buttons: [], height: 0, attributes: [ 'height', 'buttons' ] },
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

    // variables can use - or _
    // if undefined or null, the default value will be used
    // {{var_name}} <- no default, requires a variable to be set
    // {{var_name|default_value}}
    get_prototype: function(name, vars) {
        var self = this;
        var prototype = null;
        var replace = {
            'column-type': 'col-md'
        };
        name = name.replace(/-/g, '_');

        $.each(this.prototypes, function(key, val) {
            replace[key] = val;
            if (key == name) {
                prototype = val;
            }
        });
        if (prototype === null) {
            throw 'prototype ' + name + ' not found';
        }

        $.each(vars, function(key, val) {
            if (val !== null) {
                replace[key] = val;
            }
        });

        $.each(replace, function(key, val) {
            regex = new RegExp('{{' + key.replace(/[_-]/g, '[_-]') + '(\|[^}]*)?}}', 'g');
            prototype = prototype.replace(regex, val);
            // self.console.log("replace ", regex, key, val);
        });
        // replace missing variables with default values
        prototype = prototype.replace(/{{[^\|]+\|([^}]*)}}/g, '\$1');

        var vars = $.matchAll(prototype, '{{(?<var>[^\|}]+)(?:\|[^}]*)?}}');
        if (vars.length) {
            throw 'variables not found: ' + vars.join(', ');
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

    prepare_options: function(options) {
        // apply default settings to all columns and calculate number of columns

        options = $.extend({}, this.defaults.row, options);

        var self = this;
        $(options.columns).each(function() {
            var defaults = self.defaults.column[this.type];
            if (this.type === "slider" && this.color) {
                $.extend(defaults, self.defaults.column.color_slider);
            }
            var tmp = $.extend({}, defaults, this);
            $.extend(this, tmp);
            options.columns_per_row += this.columns;
        });

        self.row = options;
        return options;
    },

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
            element.removeAttr('class');
        }
        if (this.group) {
            element.data('group-id', this.group);
            element.attr('data-group-id', this.group);
        }
        if (options.zero_off) {
            element.closest('.webuicomponent').addClass('zero-off');
            if (!options.id.match(/^group-switch-/)) {
                element.addClass('slider-change-fill-intensity');
            }
        }
        if (options.attributes.length) {
            $.each(options.attributes, function() {
                var idx = this.replace(/-/g, '_');
                if (options[idx] !== undefined) {
                    element.attr(this, options[idx]);
                }
            });
        }
    },

    copy_attributes: function(element, options) {
        var self = this;
        if (options.attributes.length) {
            $(options.attributes).each(function() {
                var idx = this.replace('-', '_');
                if (options[idx] !== undefined) {
                    // self.console.log("copy_attributes", this, options[idx]);
                    element.attr(this, options[idx]);
                }
            });
        }
    },

    // publish_state: function(id, value, type) {
    //     // type = state, value 0 or 1
    //     // type = value
    //     if (this.lock_publish) {
    //         return;
    //     }
    //     var pendingRequest = this.pending_requests[id];
    //     if (pendingRequest) {
    //         // store last value and request to publish if there was any change
    //         if (pendingRequest.value != value || pendingRequest.type != type) {
    //             pendingRequest.value = value;
    //             pendingRequest.type = type;
    //             pendingRequest.publish = true;
    //         }
    //         window.clearTimeout(pendingRequest.timeout);
    //     }
    //     else {
    //         console.log("publish state", id, value, type, this.lock_publish, this.pending_requests[id]);

    //         this.pending_requests[id] = {
    //             sent: {
    //                 value: value,
    //                 type: type
    //             },
    //             value: value,
    //             type: type,
    //             publish: false
    //         };
    //         this.socket.send('+' + type + ' ' + id + ' ' + value);
    //     }
    //     // max. update rate 150ms
    //     // last published state will be sent after the timeout if it is different from what was sent last
    //     var self = this;
    //     this.pending_requests[id].timeout = window.setTimeout(function() {
    //         var request = { ...self.pending_requests[id] };
    //         delete self.pending_requests[id];
    //         if (request.publish && (request.value != request.sent.value || request.type != request.sent.type)) {
    //             self.publish_state(id, request.value, request.type)
    //         }
    //     }, 150);

    // },

    // slider_unlock_timeout: function(id, element) {
    //     if (this.components[id].locked_timeout) {
    //         window.clearTimeout(this.components[id].locked_timeout);
    //     }
    //     var self = this;
    //     this.components[id].locked_timeout = window.setTimeout(function() {
    //         delete self.components[id].locked_timeout;
    //         element.removeClass('locked');
    //         if (self.components[id].set_value !== undefined) {
    //             console.log('slider_callback set_value_delayed', self.components[id].set_value);
    //             self.lock_publish = true;
    //             element.val(self.components[id].set_value).trigger('change');
    //             delete self.components[id].set_value;
    //             window.setTimeout(function() {
    //                 self.lock_publish = false;
    //                 console.log('slider_callback set_value_delayed removing lock publish');
    //             }, 50);
    //         }
    //     }, 500);
    // },

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
                        self.console.log('SOCKET_SEND +' + key + ' ' + id + ' ' + value);
                        queue.published[key] = value;
                    }
                }
            }
            queue.values = {};
            if (queue.remove_update_lock) {
                queue.remove_update_lock = false;
                queue.update_lock = false;
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
    queue_update_state: function(id, element, value, state, update_element) {
        if (!this.lock_publish) {
            var queue = this.queue_get_entry(id);
            this.console.log('queue_update_state', id, value, state, update_element, this.lock_publish, queue);
            if (queue.update_lock) {
                queue.update['value'] = value;
                queue.update['state'] = state;
                this.add_queue_timeout(id, queue, this.queue_default_timeout);
                return;
            }
        }
        if (update_element) {
            this.queue_update_element(id, element, value, state);
        }
    },
    //
    // update element and trigger onChange
    // restore update_lock status
    //
    queue_update_element: function(id, element, value, state) {
        // console.log('queue_update_element', id, element, value, state);
        var queue = this.queue_get_entry(id);
        if (state !== undefined) {
            queue.published.state = state;
            element.prop('disabled', state != true);
        }
        if (value !== undefined) {
            queue.published.value = value;
            var update_lock = queue.update_lock;
            // console.log('save lock ',queue.update_lock, update_lock);
            element.val(value).trigger('change');
            // console.log('restore lock ',queue.update_lock, update_lock);
            queue.update_lock = update_lock;
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
        queue.values[type] = value;
        this.add_queue_timeout(id, queue, timeout);
    },

    slider_callback: function (slider, value, onSlideEnd) {
        var element = $(slider.$element);
        this.slider_update_css(element, slider.$handle, slider.$fill);
        var id = element.attr('id');
        //console.log('slider_callback ',id, this.components[id].type,element);
        if (this.components[id].type == 'group-switch') {
            this.console.log('slider_callback toggle group ', $('input[data-group-id="' + id + '"]'));
            //this.queue_publish_state(id, value, 'set', update_lock, timeout);
        }
        // else {
        //     // if (onSlideEnd) {
        //     //     this.slider_unlock_timeout(id, element);
        //     // }
        //     // else if (!element.hasClass('locked')) {
        //     //     element.addClass('locked');
        //     // }
        //     // this.publish_state(id, value, 'set');
        //     this.queue_publish_state(id, value, 'set', update_lock, timeout);
        // }
        var queue = this.queue_get_entry(id);
        if (onSlideEnd == false) {
            queue.update_lock = true;
        }
        else {
            queue.remove_update_lock = true;
        }
        this.queue_publish_state(id, value, 'set', onSlideEnd ? (this.queue_default_timeout + 400) : this.queue_default_timeout);
    },

    slider_update_css: function(element, handle, fill) {
        var value = element.val();
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
        if (value == 0 && !handle.hasClass('off')) {
            handle.removeClass('on').addClass('off');
        } else if (value && !handle.hasClass('on')) {
            handle.removeClass('off').addClass('on');
        }
    },

    create_column: function (options, innerElement) {
        var containerClass = 'webuicomponent';
        if (this.row.extra_classes) {
            containerClass += ' ' + this.row.extra_classes;
        }
        var html = '';
        for(i = 0; i < innerElement.length; i++) {
            html += innerElement[i].outerHTML;
        }
        return $('<div class="' + containerClass + '">' + html + '</div>');
    },

    has_value: function(options) {
        return !(options.value === null || options.value === undefined || options.state === false);
    },

    add_element: function (options) {
        var prototype;
        if (options.type === 'group') {
            var row_prototype = $(this.get_prototype('webui-row', { content:
                options.has_switch ?
                    (prototype = this.get_prototype('webui-group-title-content', { title: options.title, content: this.get_prototype('webui-switch', {}) })) :
                    (prototype = this.get_prototype('webui-group-title', options))
            }));
            if (this.row.extra_classes) {
                row_prototype.addClass(this.row.extra_classes);
            }
            var group_switch_id = 'group-switch-' + this.container.find('>.row').length;
            this.add_attributes($(row_prototype).find('.attribute-target'), $.extend({ 'id': group_switch_id }, this.defaults.column.switch));

            row_prototype.find('.webuicomponent').addClass(options.has_switch ? 'group-switch' : 'group')


            this.components[group_switch_id] = {type: 'group-switch'};
            this.group = group_switch_id;
            return row_prototype;

        } else if (options.type === 'sensor' || options.type === '"binary_sensor') {
            this.components[options.id] = {type: 'sensor'};
            var prototype_name = 'webui-sensor';
            if (options.render_type == 'badge') {
                prototype_name += '-badge';
            }
            var has_no_value = !this.has_value(options);
            if (has_no_value) {
                options.value = this.defaults.column.sensor.no_value;
            }
            prototype = $(this.get_prototype(prototype_name, $.extend({}, this.defaults.column.sensor, options)));
            if (has_no_value) {
                prototype.find('.data').addClass('no-value');
            }
            // if (options.height) {
            //     var sensor = prototype.find('.sensor,.badge-sensor');
            //     if (Number.isInteger(options.height)) {
            //         sensor.height(options.height + 'px');
            //     }
            //     else {
            //         sensor.height(options.height);
            //     }
            // }
            return prototype;
        }
        else if (options.type === 'switch') {
            this.components[options.id] = {type: 'switch'};
            if (options.display_name) {
                prototype = $(this.get_prototype('webui-named-switch', options));
            } else {
                prototype = $(this.get_prototype('webui-switch', options));
            }
            this.add_attributes(prototype.find('.attribute-target'), $.extend({}, this.defaults.column.switch, options));
            return prototype;
        }
        else if (options.type === 'slider') {
            this.components[options.id] = {type: 'slider'};
            options.class = (options.color === 'temperature' ? 'slider color-slider' : 'slider');
            prototype = $(this.get_prototype('webui-slider', options));
            this.add_attributes(prototype.find('.attribute-target'), $.extend({}, this.defaults.column.slider, options));
            return prototype;
        }
        else if (options.type === 'screen') {
            this.components[options.id] = {type: 'screen'};
            prototype = $(this.get_prototype('webui-screen', options));
            this.add_attributes(prototype.find('.attribute-target'), $.extend({}, this.defaults.column.screen, options));
            return prototype;
        }
        else if (options.type === "buttons") {
            this.components[options.id] = {type: 'buttons'};
            options.buttons = options.buttons.split(',');
            var buttons = '';
            $(options.buttons).each(function(key, val) {
                buttons += '<button class="btn btn-outline-primary" data-idx="' + key + '">' + val + '</button>';
            });
            var name = '';
            if (options.name) {
                name = '<h2>' + options.name + '</h2>';
            }
            var element = this.create_column(options, $('<div class="button-group"><div class="row"><div class="col">' + name + '<div class="btn-group-vertical btn-group-lg" id="' + options.id + '">' + buttons + '</div></div></div></div>'));
            if (options.height) {
                $(element).find('.button-group').height(options.height + 'px');
            }
            return element;
        }
        else if (options.type === "row") {

            options = this.prepare_options(options);

            var rowClass = 'row';
            if (options.align === "center") {
                rowClass += ' justify-content-center';
            }
            var html = '<div class="' + rowClass + '">';
            var self = this;
            $(options.columns).each(function(idx) {
                var colClass = 'col-md';
                if (this.columns) {
                    colClass += '-' + this.columns;
                }
                // add extra column to align other columns on the right
                if (idx == 0 && options.align === "right" & options.columns_per_row < 12) {
                    colClass += ' offset-md-' + (12 - options.columns_per_row);
                }
                html += '<div class="' + colClass +'">';
                html += self.add_element(this)[0].outerHTML;
                html += '</div>';
            });
            // add extra column to fill the row
            if (options.columns_per_row < 12 && options.align === "left") {
                html += '<div class="offset-md-' + (12 - options.columns_per_row) + '">';
                html += '</div>';
            }
            html += '</div>';
            this.container.append(html);

        }
        else {
            this.console.error('unknown type', options);
        }
    },

    update_ui: function(json) {
        this.console.log('update_ui', arguments);

        this.lock_publish = true;
        this.components = {};
        this.group = null;
        // this.pending_requests = [];
        this.container.html('');

        var self = this;
        $(json).each(function() {
            self.add_element(this);
        });
        this.container.append('<div class="p-2">&nbsp;<div>');

        this.container.find('.button-group').find('button').on('click', function() {
            var $this = $(this);
            var _parent = $this.parent();
            self.publish_state(_parent.attr('id'), $this.attr('data-idx'), 'set');
            _parent.find('button').removeClass('active');
            $this.addClass('active');
        });

        this.container.find('input[type="range"]').each(function() {
            var $this = $(this);
            var options = {
                polyfill : false,
                onSlideEnd: function(position, value) {
                    self.slider_callback(this, value, true);
                }
            };
            if ($this.attr('min') != 0 || $this.attr('max') != 1) {
                options.onSlide = function(position, value) {
                    self.slider_callback(this, value, false);
                }
            }
            self.update_slider_css($this.rangeslider(options));
        });

        if (this.container.find('#system_time').length) {
            system_time_attach_handler();
        }

        this.queue_delete();
        this.lock_publish = false;
    },

    update_slider_css: function(input) {
        var next = input.next();
        var handle = next.find('.rangeslider__handle');
        var fill = next.find('.rangeslider__fill');
        this.slider_update_css(input, handle, fill);
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
                else if (component_type == 'buttons') {
                    var buttons = $(element).find('button');
                    if (this.state) {
                        buttons.removeClass('disabled');
                    }
                    else {
                        buttons.addClass('disabled');
                    }
                    buttons.removeClass('active');
                    var self2 = this;
                    $(buttons).each(function(key, val) {
                        if (key == self2.value) {
                            $(val).addClass('active');
                        }
                    });
                    self.queue_update_state(this.id, element, this.value, this.state, false)
                }
                else {
                    self.queue_update_state(this.id, element, this.value, this.state, true)
                }
                // if (self.has_value(this)) {
                //     if (component_type == 'sensor') {
                //         webui_component.find('.data').removeClass('no-value');
                //     }
                //     if (component_type == 'buttons') {
                //     }
                //     else if (component_type == 'switch' || component_type == 'slider') {
                //         // // console.log("update_event", "input", this);
                //         // if (element.hasClass('locked')) {
                //         //     self.components[this.id].set_value = this.value;
                //         // }
                //         // else {
                //         queue_update_state(id, element, this.value);
                //         //     element.val(this.value).trigger('change');
                //         //     // self.update_slider_css(element);
                //         // }
                //     }
                //     else {
                //     }
                // }
                // else {
                //     if (self.components[this.id]['type'] == 'sensor') {
                //         var data = webui_component.find('.data');
                //         if (!data.hasClass('no-value')) {
                //             data.addClass('no-value');
                //             data.find('.value').html(self.defaults.column.sensor.no_value);
                //         }
                //     }
                // }
                // if (this.state !== undefined) {
                //     self.update_group(this.id, this.state ? 1 : 0);
                // }
            }
        });
        // this.update_group_switch();
        this.lock_publish = false;
    },

    request_ui: function() {
        this.console.log('request_ui', arguments);
        var url = $.getHttpLocation(this.http_uri);
        var SID = $.getSessionId();
        this.retry_time = 500;
        var self = this;
        $.get(url + '?SID=' + SID, function(data) {
            // console.log('get_callback_request_ui', arguments);
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
              }
            self.retry_time += 1000;
        });
    },

    _____rgb565_to_888: function (color) {
        var b5 = (color & 0x1f);
        var g6 = (color >> 5) & 0x3f;
        var r5 = (color >> 11);
        return [(r5 * 527 + 23) >> 6, (g6 * 259 + 33) >> 6, (b5 * 527 + 23) >> 6];
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
                    palette.push(this._____rgb565_to_888(palettergb565[i]));
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
                    copy_rle_color(this._____rgb565_to_888((tmp[1] << 8) | tmp[0]));
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
                if (json.type === 'ue') {
                    this.update_events(json.events);
                }
            } catch(e) {
                this.console.error('failed to parse json string', e, 'event.data', event)
            }
        }
        else if (event.type == 'object') {
            if (event.data.type === 'ue') {
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
    // enable alert icons for webui
    if ($.WebUIAlerts !== undefined) {
        $.WebUIAlerts.icon = true;
    }
    $(function() {
        $.webUIComponent.init();
    });
}
