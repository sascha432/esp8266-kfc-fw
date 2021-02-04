1/**
 * Author: sascha_lammers@gmx.de
 */

 var http2serialPlugin = {

    output: $("#serial_console"),
    input: $("#command-input"),
    sendButton: $('#sendbutton'),

    commands: [],
    history: {
        'data': [],
        'position': {}
    },

    filterModal: $('#console-filter'),
    filterInput: $('#console-filter .filter-input'),
    filterDefault: { value: "DEBUG[0-9]{8} .*", enabled: false },
    filter: null,

    autoReconnect: true,
    socket: null,

    addCommands: function(commands) {
        $.http2serialPlugin.console.log(commands);
        this.commands = commands;

        // var list = this.input[0].selectize;
        // try {
        //     list.clearOptions();
        //     $(commands).each(function() {
        //         $.http2serialPlugin.console.log(this);
        //         list.addOption({text: this, value: this});
        //     });
        //     list.refreshOptions();
        // } catch(e) {
        // }
    },

    historySave: function() {
        // limit to 100 entries
        if (this.history.data.length > 100) {
            if (this.history.position > 100) {
                this.history.position = 100;
            }
            this.history.data = this.history.data.slice(0, 100);
        }
        $.http2serialPlugin.console.debug('history save', this.history);
        Cookies.set('http2serial_history', this.history);
    },

    write: function(message) {
        var con = this.output[0];
        var scrollPos = con.scrollHeight - con.scrollTop - $(con).innerHeight();

        // prefilter vt100 escape codes
        if (message.indexOf('\033') != -1) {
            var pos;
            if (pos = message.lastIndexOf('\033[2J') != -1) {
                message = message.substr(pos + 4);
                consolePanel.value = '';
                $.http2serialPlugin.console.debug("clear screen", message);
            }
            // discard other vt100 sequences
            // https://github.com/xtermjs/xterm.js
            message = message.replace(/\033\[[\d;]*[mHJ]/g, '');
            $.http2serialPlugin.console.debug("replaced", message);
        }

        $.http2serialPlugin.console.debug("message", message)
        if (this.filter) {
            console.debug("filtering", this.filter)
            var filterRegEx = new RegExp('^' + this.filter + '\n', 'gm');
            con.value = (con.value + message).replace(filterRegEx, '');
        }
        else {
            con.value += message;
        }

        // stop auto scroll if user moved the scrollbar up
        if (scrollPos <= 50) {
            con.scrollTop = con.scrollHeight;
        }
    },

    dataHandler: null,

    runFilter: function() {
        if (this.filter) {
            var filterRegEx = new RegExp('^' + this.filter + '\n', 'gm');
            this.output[0].value = this.output[0].value.replace(filterRegEx, '');
        }
    },

    setFilter: function(filter) {
        $.http2serialPlugin.console.log('set filter', filter);
        this.filter = filter;
        Cookies.set('http2serial_filter', { value: filter, enabled: true });
        // this.filterModal.find('.filter-set').removeClass('btn-primary').addClass('btn-secondary');
        this.filterModal.find('.filter-remove').removeClass('btn-secondary').addClass('btn-primary').removeAttr('disabled');
    },

    removeFilter: function(filter) {
        $.http2serialPlugin.console.log('remove filter');
        this.filter = null;
        Cookies.set('http2serial_filter', { value: filter, enabled: false });
        this.filterModal.find('.filter-remove').removeClass('btn-primary').addClass('btn-secondary').attr('disabled', 'disabled');
        // this.filterModal.find('.filter-set').removeClass('btn-secondary').addClass('btn-primary');
    },

    sendCommand: function(command) {
        var command = this.input.val();
        $.http2serialPlugin.console.log("send " + command);
        if (command.trim() != "") {
            if (this.history.data.length == 0 || this.history.data[this.history.data.length - 1] != command) {
                this.history.data.push(command);
                this.history.position = this.history.data.length;
                this.historySave();
            }
            this.history.position = this.history.data.length;
            if (command.toLowerCase() == "/disconnect") {
                this.autoReconnect = false;
                this.socket.disconnect();
                this.input.val('');
                this.write("Disconnected...\n");
                return;
            } else if (command.toLowerCase() == "/reconnect") {
                this.autoReconnect = true;
                this.socket.disconnect();
                this.connect();
                this.input.val('');
                return;
            } else if (command.toLowerCase() == "/clear") {
                this.output.val('');
                this.input.val('');
                return;
            }
            try {
                this.socket.send(command + "\r\n");
                this.write(command + "\n");
                this.input.val('');
            } catch(e) {
                this.write(e + "\nConnection lost. Reconnecting...\n");
                this.connect();
            }
        } else {
            this.input.val('');
        }
    },

    connect: function() {
        if (!this.autoReconnect) {
            return;
        }
        //send_action(true);
        var url = $.getWebSocketLocation('/serial-console');
        var SID = $.getSessionId();
        var self = this;
        this.socket = new WS_Console(url, SID, 1, function(event) { self.dataHandler(event); }, this.output.attr('id'));
        $.http2serialPlugin.console.log(this.socket);
        this.socket.connect();
    },

    init: function() {
        var self = this;
        this.input.on('keyup', function(event) {
            $.http2serialPlugin.console.log('keyup', event.key, event);
            if (event.key === 'Enter') {
                event.preventDefault();
                self.sendCommand();
            }
            else if (event.key === 'ArrowUp') {
                event.preventDefault();
                if (self.history.position > 0) {
                    if (self.history.data.length == self.history.pposition) {
                        var value = self.input.val();
                        if (value.trim() !== '') {
                            self.history.data.push(value);
                        }
                    }
                    self.history.position--;
                }
                try {
                    self.input.val(self.history.data[self.history.position]);
                    self.historySave();
                } catch(e) {
                }
            }
            else if (event.key === 'ArrowDown') {
                event.preventDefault();
                if (self.history.position < self.history.data.length - 1) {
                    self.history.position++;
                    self.input.val(self.history.data[self.history.position]);
                    self.historySave();
                }
            }
            else {
                var tmp = self.history.position
                self.history.position = self.history.data.length;
                if (tmp != self.history.position) {
                    self.historySave();
                }
            }
        });
        this.input.focus();

        this.sendButton.on('click', function(event) {
            event.preventDefault();
            self.sendCommand();
        });

        this.history = Cookies.getJSON('http2serial_history', this.history);
        var filter = Cookies.getJSON('http2serial_filter', this.filterDefault);
        $.http2serialPlugin.console.debug('filter cookie', filter, 'history cookie', this.history)

        this.filterInput.val(filter.value);
        if (filter.enabled) {
            this.setFilter(filter.value);
        }
        else {
            this.removeFilter(filter.value);
        }

        var self = this;
        this.filterModal.find('.filter-remove').on('click', function() {
            self.removeFilter(self.filterInput.val());
            self.filterModal.modal('hide');
        });
        this.filterModal.find('.filter-set').on('click', function() {
            self.setFilter(self.filterInput.val());
            self.runFilter();
            self.filterModal.modal('hide');
        });

        this.connect();
    },

    ledMatrix: {
        'pixel_size': 0.6,
        'alpha1': 0.9,
        'alpha2': 0,
        'inner_size': 0,
        'outer_size': 0,
        'args': [0, 0],
        'fps_time': 0,
        'fps_value': 0,
    },

    zoomLedMatrix: function(dir) {
        this.ledMatrix.pixel_size += dir * 0.15;
        this.appendLedMatrix.apply(this, this.ledMatrix.args)
    }

};

http2serialPlugin.appendLedMatrix = function(rows, cols, reverse_rows, reverse_cols, rotate, interleaved) {
    this.ledMatrix.args = arguments
    var _pixel_size = this.ledMatrix.pixel_size;
    var unit = 'rem';
    var pixel_size = _pixel_size + unit;
    var inner_size = (_pixel_size * 1) + unit;
    var outer_size = (_pixel_size * 7) + unit;
    var border_radius = _pixel_size + unit;

    $('#led-matrix').remove();

    this.ledMatrix.inner_size = inner_size;
    this.ledMatrix.outer_size = outer_size;
    $('body').append('<div id="led-matrix"><div id="pixel-container"></div></div><style type="text/css"> \
#pixel-container { \
    position: absolute; \
    right: 0; \
    bottom: 0; \
    padding: 7rem; \
    background: rgba(0, 0, 0); \
    z-index: 9998; \
    padding: 7rem; \
    background: rgba(0, 0, 0); \
    min-height: 1rem; \
    min-width: 1rem; \
    overflow: hidden; \
} \
#pixel-container:hover .ipx { \
    font-size: 0.75rem; \
    mix-blend-mode: difference; \
} \
#pixel-container .row { \
    display: block; \
} \
#pixel-container .px { \
    mix-blend-mode: lighten; \
    margin: ' + inner_size + '; \
    width: ' + pixel_size + '; \
    height: ' + pixel_size + '; \
    display: inline-block; \
    border-radius: ' + border_radius +  '; \
} \
#pixel-container .ipx { \
    mix-blend-mode: lighten; \
    margin: 0px; \
    width: 0px; \
    height: 0px; \
    display: inline; \
    border-radius: 0px; \
    color: white; \
    font-size: 0; \
} \
#pixel-container .fps-container { \
    position: relative; \
    left: -90px; \
    bottom: -90px; \
    z-index: 9999; \
    color: #ccc; \
    font-size: 1rem; \
} \
#fps-number { \
    color: #aaa; \
    font-size: 1.25rem; \
} \
#pixel-container .zoom .oi { \
    padding: 0.25rem; \
    font-size: 1.25rem; \
    color: #bbb; \
} \
#pixel-container .oi-zoom .oi:hover { \
    color: #fff; \
} \
</style></div>');
    var container = $('#pixel-container');
    var contents = '';

    function to_address(row, col) {
        if (rotate) {
            var tmp = row;
            row = col;
            col = tmp;
        }
        if (reverse_rows) {
            row = (rows - 1) - row;
        }
        if (reverse_cols) {
            col = (cols - 1) - col;
        }
        if (interleaved) {
            if (col % 2 == 1) {
                row = (rows - 1) - row;
            }
        }
        return col * rows + row;
    }

    for (var i = 0; i < rows; i++) {
        contents += '<div class="row">';
        for (var j = 0; j < cols; j++) {
            var px_id = to_address(i, j);
            contents += '<div id="px' + px_id + '" class="px"><div class="ipx">' + px_id + '</div></div>';
        }
        contents += '</div>';
    }
    container.html(contents + '<div class="fps-container"><span id="fps-number">-</span> fps <div class="zoom"><span class="oi oi-zoom-in"></span><span class="oi oi-zoom-out"></span></div></div>');
    var n = 0;
    for (var i = 0; i <rows; i++) {
        for (var j = 0; j < cols; j++) {
            var name = 'px' + n;
            n++;
            this.ledMatrixSetColor('#' + name, 0x30, 0x30, 0x30, 0.4, 0.1);
        }
    }

    var self = this;
    container.find('.oi.oi-zoom-in').on('click', function() { self.zoomLedMatrix(1); });
    container.find('.oi.oi-zoom-out').on('click', function() { self.zoomLedMatrix(-1); });

    this.ledMatrix.fps_time = 0;

    return container;
};

http2serialPlugin.ledMatrixSetColor = function(selector, r, g, b, a1, a2) {
    if (a1 === undefined) {
        a1 = this.ledMatrix.alpha1;
        a2 = this.ledMatrix.alpha2;
       }
    $(selector).
        css('box-shadow', '0px 0px ' + this.ledMatrix.inner_size + ' ' + this.ledMatrix.inner_size + ' rgba(' + r + ', ' + g + ', ' + b + ', ' + a1 + ')').
        css('background', 'rgba(' + r + ', ' + g + ', ' + b + ', ' + a1 + ')').
        css('border-color', 'rgba(' + r + ', ' + g + ', ' + b + ', ' + a1 + ')');
    $(selector).find('.ipx').
        css('box-shadow', '0px 0px ' + this.ledMatrix.outer_size + ' ' + this.ledMatrix.outer_size + ' rgba(' + r + ', ' + g + ', ' + b + ', ' + a2 + ')').
        css('background', 'rgba(' + r + ', ' + g + ', ' + b + ', ' + a2 + ')').
        css('border-color', 'rgba(' + r + ', ' + g + ', ' + b + ' ' + a2 + ')');
};

http2serialPlugin.countFps = function() {
    var t = new Date().getTime();
    if (this.ledMatrix.fps_time == 0) {
        this.ledMatrix.fps_value = 0;
    }
    else if (this.ledMatrix.fps_value == 0) {
        this.ledMatrix.fps_value = t - this.ledMatrix.fps_time;
    }
    else {
        var diff = (t - this.ledMatrix.fps_time) + 0.001;
        var itg = 3000.0 / diff;
        this.ledMatrix.fps_value = ((this.ledMatrix.fps_value * itg) + diff) / (itg + 1);
        $('#fps-number').html(Math.round(1000 / this.ledMatrix.fps_value * 100) / 100.0);
    }
    this.ledMatrix.fps_time = t;
}

http2serialPlugin.dataHandler = function(event) {
    $.http2serialPlugin.console.log("dataHandler", event);
    if (event.data instanceof ArrayBuffer) {
        var packetId = new Uint16Array(event.data, 0, 1);
        if (packetId == 0x0004) {// WsClient::BinaryPacketType::LED_MATRIX_DATA
            var container = $('#pixel-container');
            if (container.length) {
                this.countFps();
                var num_pixels = this.ledMatrix.args[0] * this.ledMatrix.args[1];
                var pixels = new Uint16Array(event.data, 2, num_pixels);
                for (var i = 0; i < num_pixels; i++)  {
                    var pixel = $._____rgb565_to_888(pixels[i]);
                    this.ledMatrixSetColor('#px' + i, pixel[0], pixel[1], pixel[2]);
                }
            }
        }
    }
    else if (event.type == 'data') {
        if (event.data.startsWith('+LED_MATRIX=')) {
            $('#led-matrix').remove();
            args = event.data.substring(12).split(',');
            if (args.length >= 2) {
                while(args.lenth < 6) {
                    args.push(0);
                }
                this.appendLedMatrix(parseInt(args[0]), parseInt(args[1]), parseInt(args[2]), parseInt(args[3]), parseInt(args[4]), parseInt(args[5]));
            }
        }
        else if (event.data.startsWith("+ATMODE_CMDS_HTTP2SERIAL=")) {
            this.addCommands(event.data.substring(25).split('\t'));
        }
        else {
            this.write(event.data);
            this.runFilter();
        }
    }
};

$.http2serialPlugin = http2serialPlugin;
dbg_console.register('$.http2serialPlugin', $.http2serialPlugin);
