/**
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

    dataHandler: function(event) {
        $.http2serialPlugin.console.log("dataHandler", event);
        if (event.type == 'data') {
            if (event.data.substring(0, 25) == "+ATMODE_CMDS_HTTP2SERIAL=") {
                this.addCommands(event.data.substring(25).split('\t'));
            } else {
                this.write(event.data);
                this.runFilter();
            }
        }
    },

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
};

$.http2serialPlugin = http2serialPlugin;
dbg_console.register('$.http2serialPlugin', $.http2serialPlugin);
