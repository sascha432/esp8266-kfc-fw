1/**
 * Author: sascha_lammers@gmx.de
 */

 var http2serialPlugin = {

    output: $("#serial_console"),
    input: $("#command-input"),
    sendButton: $('#sendbutton'),

    commands: [],
    history_limit: 100,
    history_input: null,
    history: {
        'data': [],
        'position': {},
        'selected': true,
    },

    filterModal: $('#console-filter'),
    filterInput: $('#console-filter .filter-input'),
    filterDefault: { value: "DEBUG[0-9]{8} .*", enabled: false },
    filter: null,

    autoReconnect: true,
    isConnected: false,
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

    historyClear: function() {
        this.history_input = null;
        this.history = {
            'data': [],
            'position': {},
            'selected': true,
        };
        this.historySave();
    },

    historySave: function() {
        if (this.history.data.length > this.history_limit) {
            this.history.data = this.history.data.slice(this.history.data.length - this.history_limit);
            if (this.history.position > this.history_limit) {
                this.history.position = this.history.data.length;
            }

        }
        $.http2serialPlugin.console.debug('history save', this.history);
        Cookies.set('http2serial_history', this.history);
    },

    historyAdd: function(command) {
        this.history_input = null;
        if ((this.history.data.length == 0 || this.history.data[this.history.data.length - 1] != command)) {
            this.history.data.push(command);
            this.history.position = this.history.data.length;
            this.historySave();
        }
        this.history.position = this.history.data.length;
    },

    historyKeyupEvent: function(event) {
        if (event.key === 'ArrowUp') {
            event.preventDefault();
            if (this.history.position > 0) {
                if (this.history.data.length == this.history.position) {
                    this.history_input = this.input.val(); // store current input if not from history
                }
                this.history.position--;
            }
            try {
                this.input.val(this.history.data[this.history.position]);
                this.historySave();
            } catch(e) {
                this.historyClear(); // clear invalid data
            }
        }
        else if (event.key === 'ArrowDown') {
            event.preventDefault();
            if (this.history.position < this.history.data.length - 1) {
                this.history.position++;
                this.input.val(this.history.data[this.history.position]);
                this.historySave();
            }
            else if (this.history_input) {
                this.input.val(this.history_input); // restore saved input
                this.history_input = null;
                this.history.position = this.history.data.length; // marker for input is not from history
            }
        }
    },

    write: function(message) {
        var con = this.output[0];
        var scrollPos = con.scrollHeight - con.scrollTop - $(con).innerHeight();
        // store selection
        var selStart = this.output.prop('selectionStart');
        var selEnd = this.output.prop('selectionEnd');

        // prefilter vt100 escape codes
        if (message.indexOf('\x1b') != -1) {
            var pos;
            if (pos = message.lastIndexOf('\x1b[2J') != -1) {
                message = message.substr(pos + 4);
                consolePanel.value = '';
                $.http2serialPlugin.console.debug("clear screen", message);
            }
            // discard other vt100 sequences
            // https://github.com/xtermjs/xterm.js
            message = message.replace(/\x1b\[[\d;]*[mHJ]/g, '');
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

        // restore selection
        if (selEnd - selStart > 0) {
            this.output.prop('selectionStart', selStart);
            this.output.prop('selectionEnd', selEnd);
        }

    },

    dataHandler: function(event) {
        $.http2serialPlugin.console.log("dataHandler", event);
        if (event.type == 'data') {
            if (event.data.startsWith("+ATMODE_CMDS_HTTP2SERIAL=")) {
                this.addCommands(event.data.substring(25).split('\t'));
            }
            else {
                this.write(event.data);
                this.runFilter();
            }
        }
    },

    runFilter: function() {
        if (this.filter) {
            var filterRegEx = new RegExp('^' + this.filter + '\n', 'gm');
            var newText = this.output[0].value.replace(filterRegEx, '');
            if (this.output[0].value != newText) {
                // selection is lost if the filter modifies the data
                this.output[0].value = newText;
            }
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

    sendCommand: function() {
        var command = this.input.val();
        $.http2serialPlugin.console.log('send ' + command);
        if (command.trim() != '') {
            this.historyAdd(command);
            cmdlo = command.toLowerCase();
            if (cmdlo.startsWith('/dis')) {
                this.autoReconnect = false;
                this.socket.disconnect();
                this.input.val('');
                this.write('Disconnected...\n');
                return;
            } else if (cmdlo.startsWith('/re') || cmdlo.startsWith('/con')) {
                this.autoReconnect = true;
                this.socket.disconnect();
                this.connect();
                this.input.val('');
                return;
            } else if (cmdlo == '/ch' || cmdlo == '/clear-history') {
                this.historyClear();
                this.input.val('');
                return;
            } else if (cmdlo.startsWith('/cl')) {
                this.output.val('');
                this.input.val('');
                return;
            }
            try {
                this.socket.send(command + '\r\n');
                this.write(command + '\n');
                this.input.val('');
            } catch(e) {
                this.write('Not connected. Reconnecting...\n');
                this.autoReconnect = true;
                this.socket.disconnect();
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
        this.socket.__connect = this.socket.connect;
        this.socket.__disconnect = this.socket.disconnect;
        $('#connectbutton').on('click', function() {
            if (self.isConnected) {
                self.autoReconnect = false;
                self.socket.disconnect();
                self.write('Disconnected...\n');
            }
            else {
                self.autoReconnect = true;
                self.socket.disconnect();
                self.connect();
            }
        });
        self.socket.connect = function(authenticated_callback) {
            self.socket.__connect(authenticated_callback);
            self.isConnected = true;
            $('#connectbutton').html('Disconnect');
        };
        self.socket.disconnect = function(reconnect) {
            self.socket.__disconnect(reconnect);
            self.isConnected = false;
            $('#connectbutton').html('Connect');
        };


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
            else {
                self.historyKeyupEvent(event);
            }
        });
        this.input.focus();

        this.sendButton.on('click', function(event) {
            event.preventDefault();
            self.sendCommand();
        });

        // open URLs on double click or copy src filename and line number to clipboard
        this.output.on('dblclick', function(e) {
            var pos = $(this).prop('selectionStart');
            if (pos) { // find beginning of the text that has been clicked on
                var start = -1;
                var text = $(this).val();
                for(var i = pos; i >= 0; i--) {
                    var ch = text[i];
                    switch(ch) {
                        case ' ':
                        case '(':
                        case '[':
                        case '\t':
                        case '\r':
                        case '\n':
                            start = i;
                            i = 0;
                            break;
                    }
                }
                try {
                    var g = text.substr(start + 1).match(/((?<url>http(s)?:\/\/[^\s\)\]]+)|(?<src>\S+\.(c(pp)?):[0-9]+))/).groups; // catch url or src code with line number
                    if (g['url'] !== undefined) {
                        window.open(g['url'], '_blank').focus(); // open in a new tab
                    }
                    else if (g['src'] !== undefined) {
                        $.clipboard(null, g['src']);  // copy to clipboard
                    }
                    else {
                        return;
                    }
                    $(this).prop('selectionStart', -1); // remove selection from double click
                    e.preventDefault();
                } catch(e) {
                }
            }
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
