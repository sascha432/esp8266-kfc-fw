/**
 * Author: sascha_lammers@gmx.de
 */

 if ($.matchAll===undefined) {
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
}

$.webUIComponent = {
    uri: '/webui_ws',
    container: $('#webui'),

    prototypes: {
        webui_group_title_content: '<div class="{{column-type}}-12"><div class="webuicomponent title text-white bg-primary"><div class="row group"><div class="col-auto mr-auto"><h1>{{title}}</h1></div>{{webui-disconnected-icon}}<div class="col-auto">{{content}}</div></div></div></div>',
        webui_group_title: '<div class="{{column-type}}-12"><div class="webuicomponent title text-white bg-primary"><div class="row group"><div class="col-auto mr-auto mt-auto mb-auto"><h1>{{title}}</h1></div>{{webui-disconnected-icon}}</div></div></div>',
        webui_disconnected_icon: '<div class="col-auto lost-connection hidden" id="lost-connection"><span class="oi oi-bolt webui-disconnected-icon"></span></div>',
        webui_row: '<div class="row">{{content}}</div>',
        webui_col_12: '<div class="{{column-type}}-12"><div class="webuicomponent">{{content}}</div></div>',
        webui_col_2: '<div class="{{column-type}}-2"><div class="webuicomponent">{{content}}</div></div>',
        webui_col_3: '<div class="{{column-type}}-3"><div class="webuicomponent">{{content}}</div></div>',
        webui_sensor: '<div class="sensor"><h3>{{name}}</h3><h1><span id="{{id}}"></span><span class="unit">{{unit}}</span></h1></div>',
        webui_switch: '<div class="switch"><input type="range" class="switch-attribute-target"></div>',
        webui_sensor_badge: '<div class="badge-sensor"><div class="row"><div class="col"><div class="outer-badge"><div class="inner-badge"><{{head|h4}}><div id="{{id}}" class="value">{{value}}</div><span class="unit">{{unit}}</span></{{head|h4}}></div></div></div></div><div class="row"><div class="col text-center">{{name}}</div></div></div>',
        webui_sensor: '<div class="sensor"><h3>{{name}}</h3><{{head|h1}}><span id="{{id}}" class="value">{{value}}</span><span class="unit">{{unit}}</span></{{head|h1}}></div>',
    },


    defaults: {
        row: {
            columns_per_row: 0
        },
        column: {
            group: { columns: 12 },
            switch: { min: 0, max: 1, columns: 2, zero_off: true, display_name: false, attributes: [ 'min', 'max', 'value', 'zero-off', 'display-name' ] },
            slider: { min: 0, max: 255, columns: 12, attributes: [ 'min', 'max', 'zero-off', 'value' ] },
            color_slider: { min: 15300, max: 50000 },
            sensor: { columns: 3, head: null, height: 0, attributes: [ 'head', 'height' ] },
            screen: { columns: 3, width: 128, height: 32, attributes: [ 'width', 'height' ] },
            binary_sensor: { columns: 2 },
            buttons: { columns: 3, buttons: [], height: 0, attributes: [ 'height', 'buttons' ] },
        }
    },
    groups: [],
    pending_requests: [],
    lock_publish: false,
    retry_time: 500,

    // variables can use - or _
    // if undefined or null, the default value will be used
    // {{var_name}} <- no default, requires a variable to be set
    // {{var_name|default_value}}
    get_prototype: function(name, vars) {
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

        var prototype_tmp = prototype;

        $.each(replace, function(key, val) {
            regex = new RegExp('{{' + key.replace(/[_-]/g, '[_-]') + '(\|[^}]*)?}}', 'g');
            prototype = prototype.replace(regex, val);
            // dbg_console.log("replace ", regex, key, val);
        });
        // replace missing variables with default values
        prototype = prototype.replace(/{{[^\|]+\|([^}]*)}}/g, '\$1');

        var vars = $.matchAll(prototype, '{{(?<var>[^\|}]+)(?:\|[^}]*)?}}');
        if (vars.length) {
            throw 'variables not found: ' + vars.join(', ');
        }

        // dbg_console.log('prototype', name, prototype_tmp, prototype, 'replaced', replace);
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
        $.each(element.attr('class').split(/\s+/), function(key, val) {
            if (val.search(/attribute-target$/) != -1) {
                element.removeClass(val);
            }
        });
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
        if (options.attributes.length) {
            $(options.attributes).each(function() {
                var idx = this.replace('-', '_');
                if (options[idx] !== undefined) {
                    // console.log("copy_attributes", this, options[idx]);
                    element.attr(this, options[idx]);
                }
            });
        }
    },

    publish_state: function(id, value, type) {
        // type = state, value 0 or 1
        // type = value

        dbg_console.log("publish state", id, value, type, this.lock_publish);//, this.pending_requests[id]);
        if (this.lock_publish) {
            return;
        }

        var pendingRequest = this.pending_requests[id];
        if (pendingRequest && pendingRequest.pending) {

            pendingRequest.value = value;
            pendingRequest.type = type;

        } else {
            this.pending_requests[id] = {
                pending: true,
                value: value,
                type: type,
            };
            this.socket.send('+' + type + ' ' + id + ' ' + value);

            var self = this;
            window.setTimeout(function() { // max. update rate 100ms, last published state will be sent after the timeout
                self.pending_requests[id].pending = false;
                if (self.pending_requests[id].value != value || self.pending_requests[id].type != type) {
                    self.publish_state(id, self.pending_requests[id].value, self.pending_requests[id].type)
                }
            }, 100);
        }

    },

    toggle_group: function(groupId, value) {
        dbg_console.log("toggle_group", groupId, value);

        if (this.groups[groupId].switch.value == value) {
            return;
        }

        var self = this;
        $(this.groups[groupId].components).each(function() {
            if (this.type == 'switch' || this.type == 'slider') {
                if (this.zero_off) {
                    self.publish_state(this.id, value, 'set_state');
                }
            }
        });
    },

    move_slider: function(slider, id, value) {
        this.publish_state(id, value, 'set');
        this.update_group(id, value);
        this.update_group_switch();
    },

    slider_callback: function (slider, position, value, onSlideEnd) {

        var element = $(slider.$element);
        var dataId = element.attr('id');

        if (dataId.substring(0, 12) == 'group-switch') {
            var groupId = parseInt(dataId.substring(13)) - 1;
            this.toggle_group(groupId, value);
        }
        else {
            this.move_slider(slider, dataId, value);
        }
        this.sliderUpdateCSS(element, slider.$handle, slider.$fill);
    },

    sliderUpdateCSS: function(element, handle, fill) {
        if (element.attr('zero-off')) {
            var value = element.val();

            // change brightness of slider
            var a = (value / parseInt(element.attr('max')) * 0.7);
            fill.css('background-image', '-webkit-gradient(linear, 50% 0%, 50% 100%, color-stop(0%, rgba(255, 255, 255, '+a+')), color-stop(100%, rgba(255, 255, 255, '+a+')))');
            fill.css('background-image', '-moz-linear-gradient(rgba(255, 255, 255, '+a+'), rgba(255, 255, 255, 0, '+a+'))');
            fill.css('background-image', '-webkit-linear-gradient(rgba(255, 255, 255, '+a+'), rgba(255, 255, 255, '+a+'))');
            fill.css('background-image', 'linear-gradient(rgba(255, 255, 255, '+a+'), rgba(255, 255, 255, '+a+'))');

            // change color of handle
            if (value == 0) {
                handle.removeClass('on').addClass('off');
            } else if (value) {
                handle.removeClass('off').addClass('on');
            }
        }

    },

    add_to_group: function(options) {
        if (this.groups.length) {
            var group = this.groups[this.groups.length - 1];
            if (options.group_switch) {
                group.switch = options;
            }
            else if (group.switch) {
                group.components.push(options);
                if (!group.switch.state) {
                    if (options.zero_off) {
                        if (options.state) {
                            group.switch.state = true;
                            group.switch.value = 1;
                            $('#' + group.switch.id).val(1);
                        }
                    }
                }
            }
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

    add_element: function (options) {
        var prototype;
        if (options.type === 'group') {
            this.groups.push({components: []});

            if (options.has_switch) {
                prototype = this.get_prototype('webui-group-title-content', {
                    title: options.name,
                    content: this.get_prototype('webui-switch', {})
                });
            }
            else {
                prototype = this.get_prototype('webui-group-title', { title: options.name })
            }

            var row = $(this.get_prototype('webui-row', { content: prototype }));
            // if (this.row.extra_classes) {
            //     row.addClass(this.row.extra_classes);
            // }
            this.add_attributes($(row).find('.switch-attribute-target'), $.extend({ 'id': 'group-switch-' + this.groups.length }, this.defaults.column.switch));

            return row;

        } else if (options.type === 'sensor' || options.type === '"binary_sensor') {
            var element;
            if (options.value === undefined || options.state === false) {
                options.value = 'N/A';
            }

            var prototype_name = 'webui-sensor';
            if (options.render_type == 'badge') {
                prototype_name += '-badge';
            }
            prototype = $(this.get_prototype(prototype_name, options));

            if (options.height) {
                prototype.find('.badge-sensor').height(options.height + 'px');
            }

            return prototype;


/*
            switch: { min: 0, max: 1, columns: 2, zero_off: true, display_name: false, attributes: [ 'min', 'max', 'value', 'zero-off', 'display-name' ] },

            console.log('GROUP',options);

            var template = '<div class="row group"><div class="col"><h1>' + options.name + '</h1></div>';
            if (options.has_switch) {
                var o = {
                    type: 'switch',
                    value: 0,
                    id: 'group_switch_' + this.groups.length,
                    group_switch: true
                };
                o = $.extend(o, );
                template += '<div class="col">' + this.add_element(o).html() + '</div>';
            }
            template += '</div>';
            var element = this.create_column(options, $(template));
            return element;*/
        }
        else if (options.type === "switch") {
            if (options.display_name) {
                var element = $('<div class="named-switch"><div class="row"><div class="col"><input type="range" id=' + options.id + '></div></div><div class="row"><div class="col switch-name">' + options.name + '</div></div></div>');
            } else {
                var element = $('<div class="switch"><input type="range" id=' + options.id + '></div>');
            }
            var input = element.find("input");
            this.copy_attributes(input, options)
            var element = this.create_column(options, element);
            this.add_to_group(options);
            return element;
        }
        else if (options.type === "slider") {
            var slider = $('<div class="' + ((options.color === "temperature") ? 'color-slider' : 'slider') + '"><input type="range" id="' + options.id + '"></div>');
            var input = slider.find("input");
            this.copy_attributes(input, options)
            var element = this.create_column(options, slider);
            this.add_to_group(options);
            return element;
        }
        else if (options.type === "screen") {
            var canvas = '<canvas width="' + options.width + '" height="' + options.height + '" border="0" style="border:2px solid grey" id="' + options.id + '"></canvas>';
            var element = this.create_column(options, $('<div class="screen"><div class="row"><div class="col">' + canvas + '</div></div></div>'));
            return element;
        }
        else if (options.type === "buttons") {
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
        else if (options.type === "sensor" || options.type === "binary_sensor") {
            var element;
            if (options.value === undefined || options.state === false) {
                options.value = 'N/A';
            }
            if (options.render_type == 'badge') {
                if (options.head === false) {
                    options.head = 'h4';
                }
                var element = this.create_column(options, $('<div class="badge-sensor"><div class="row"><div class="col"><div class="outer-badge"><div class="inner-badge"><' + options.head + ' id="' + options.id + '">' + options.value + '</' + options.head + '><div class="unit">' + options.unit + '</div></div></div></div></div><div class="row"><div class="col text-center">' + options.name + '</div></div></div>'));
                if (options.height) {
                    $(element).find('.badge-sensor').height(options.height + 'px');
                }
            }
            else if (options.render_type == 'wide') {
            }
            else if (options.render_type == 'medium') {
            }
            else {
                if (options.head === false) {
                    options.head = 'h1';
                }
                var element = this.create_column(options, $('<div class="sensor"><h3>' + options.name + '</h3><' + options.head + '><span id="' + options.id + '">' + options.value + '</span><span class="unit">' + options.unit + '</span></' + options.head + '>'));
                if (options.height) {
                    $(element).find('.sensor').height(options.height + 'px');
                }
            }
            // this.add_to_group(options);
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
            dbg_console.error('unknown type', options);
        }
    },

    update_ui: function(json) {
        dbg_console.called('update_ui', arguments);

        this.lock_publish = true;

        this.groups = [];
        this.pending_requests = [];
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
                    self.slider_callback(this, position, value, true);
                }
            };
            if ($this.attr('min') != 0 || $this.attr('max') != 1) {
                options.onSlide = function(position, value) {
                    self.slider_callback(this, position, value, false);
                }
            }
            self.update_slider_css($this.rangeslider(options));
        });

        if (this.container.find('#system_time').length) {
            system_time_attach_handler();
        }

        this.lock_publish = false;
    },

    update_slider_css: function(input) {
        var next = input.next();
        var handle = next.find('.rangeslider__handle');
        var fill = next.find('.rangeslider__fill');
        this.sliderUpdateCSS(input, handle, fill);
    },

    // update state of components
    update_group: function(id, value) {
        $(this.groups).each(function() {
            $(this.components).each(function() {
                if (this.id == id && this.zero_off) {
                    this.state = value != 0;
                }
            });
        });
    },

    // check if any component is on and update group switch
    update_group_switch: function() {
        var self = this;
        $(this.groups).each(function() {
            var value = 0;
            $(this.components).each(function() {
                if (this.state && this.zero_off) {
                    value = 1;
                }
            });
            if (this.switch && this.switch.value != value) {
                var element = $('#' + this.switch.id);
                if (element.length) {
                    this.switch.value = value;
                    element.val(value).change();
                    self.update_slider_css(element);
                }
            }
        });
    },

    update_events: function(events) {
        dbg_console.log("update_events", events);
        this.lock_publish = true;
        var self = this;
        $(events).each(function() {
            var element = $('#' + this.id);
            if (element.length) {
                if (this.value !== undefined) {
                    self.update_group(this.id, this.value);
                    if (element[0].nodeName == 'DIV' && element.attr('class') && element.attr('class').indexOf('btn-group') != -1) {
                        var buttons = $(element).find('button');
                        if (this.state) {
                            buttons.removeClass('disabled');
                        }
                        else {
                            buttons.addClass('disabled');
                        }
                        buttons.removeClass('active');
                        var this2 = this;
                        $(buttons).each(function(key, val) {
                            if (key == this2.value) {
                                $(val).addClass('active');
                            }
                        });
                    }
                    else if (element[0].nodeName != 'INPUT') {
                        // console.log("update_event", "innerHtml", this);
                        element.html(this.value);
                        if (element.find('#system_time').length) {
                            system_time_attach_handler();
                        }
                    }
                    else {
                        // console.log("update_event", "input", this);
                        element.val(this.value).change();
                        self.update_slider_css(element);
                    }
                }
                if (this.state !== undefined) {
                    self.update_group(this.id, this.state ? 1 : 0);
                }
            }
        });
        this.update_group_switch();
        this.lock_publish = false;
    },

    request_ui: function() {
        dbg_console.called('request_ui', arguments);
        var url = $.getHttpLocation('/webui_get');
        var SID = $.getSessionId();
        this.retry_time = 500;
        var self = this;
        $.get(url + '?SID=' + SID, function(data) {
            // dbg_console.called('get_callback_request_ui', arguments);
            dbg_console.debug('GET /webui_get', data);
            self.update_ui(data.data);
            self.update_events(data.values);
            self.show_disconnected_icon('connected');
        }, 'json').fail(function(error) {
            dbg_console.error('request_ui get error', error, 'retry_time', self.retry_time);
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
                dbg_console.error('cannot find canvas for', canvasIdStr);
                return;
            }
            ctx = canvas[0].getContext('2d');
            if (!ctx) {
                dbg_console.error('cannot get 2d context for', canvas);
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
            dbg_console.error('failed to decode header', e);
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
            dbg_console.error('failed to decode image data', e);
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
                dbg_console.log('unknown packet id: ' + packetId)
            }
        }
        else if (event.type == 'data') {
            try {
                var json = JSON.parse(event.data);
                if (json.type === 'ue') {
                    this.update_events(json.events);
                }
            } catch(e) {
                dbg_console.error('failed to parse json string', e, 'event.data', event)
            }
        }
        else if (event.type == 'object') {
            if (event.data.type === 'ue') {
                this.update_events(event.data.events);
            }
        }
    },

    init: function() {
        dbg_console.called('init', arguments);
        var url = $.getWebSocketLocation(this.uri);
        var SID = $.getSessionId();
        var self = this;
        this.socket = new WS_Console(url, SID, 1, function(event) {
            event.self = self;
            self.socket_handler(event);
        } );

        if (dbg_console.vars.enabled) {
            $('body').append('<textarea id="console" style="width:350px;height:150px;font-size:10px;font-family:consolas;z-index:999;position:fixed;right:5px;bottom:5px"></textarea>');
            this.socket.setConsoleId("console");
        }
        this.socket.connect();
    }
};

if ($('#webui').length) {
    // enable alert icons for webui
    if ($.WebUIAlerts !== undefined) {
        $.WebUIAlerts.icon = true;
    }
    $(function() {
        $.webUIComponent.init();
    });
}
