/**
 * Author: sascha_lammers@gmx.de
 */

var http2serialPlugin = {

    console: $("#rxConsole"),
    input: $("#command-input"),
    sendButton: $('#sendbutton'),

    filterModal: $('#console-filter'),
    filterInput: $('#console-filter .filter-input'),
    filterDefault: { value: "DEBUG[0-9]{8} .*", enabled: false },
    filter: null,

    autoReconnect: true,
    socket: null,
    packetCount: 0,

    addCommands: function(commands) {
        console.log(commands);
        var list = this.input[0].selectize;
        try {
            list.clearOptions();
            $(commands).each(function() {
                console.log(this);
                list.addOption({text: this, value: this});
            });
            list.refreshOptions();
        } catch(e) {
        }
    },

    write: function(message) {
        console.log("write", message);
        if (this.filter) {
            console.log("filtering", this.filter);
            var filterRegEx = new RegExp('^' + this.filter + '\n', 'gm');
            this.console[0].value = (this.console[0].value + message).replace(filterRegEx, '');
        }
        else {
            this.console[0].value += message;
        }
        this.console[0].scrollTop = this.console[0].scrollHeight;
    },

    dataHandler: function(event) {
        console.log("dataHandler", event);
        if (event.type == 'auth') {
            this.packetCount = 0;
        }
        else if (event.type == 'data') {
            console.log("data", this.packetCount, event.data);
            if (this.packetCount++ == 0 && event.data.substring(0, 6) == "+CMDS=") {
                this.addCommands(event.data.substring(6).split('\t'));
            } else {
                this.write(event.data);
                this.runFilter();
            }
        }
    },

    runFilter: function() {
        if (this.filter) {
            var filterRegEx = new RegExp('^' + this.filter + '\n', 'gm');
            this.console[0].value = this.console[0].value.replace(filterRegEx, '');
        }
    },

    setFilter: function(filter) {
        console.log('set filter', filter);
        this.filter = filter;
        Cookies.set('serial2http_filter', { value: filter, enabled: true });
        // this.filterModal.find('.filter-set').removeClass('btn-primary').addClass('btn-secondary');
        this.filterModal.find('.filter-remove').removeClass('btn-secondary').addClass('btn-primary').removeAttr('disabled');
    },

    removeFilter: function(filter) {
        console.log('remove filter');
        this.filter = null;
        Cookies.set('serial2http_filter', { value: filter, enabled: false });
        this.filterModal.find('.filter-remove').removeClass('btn-primary').addClass('btn-secondary').attr('disabled', 'disabled');
        // this.filterModal.find('.filter-set').removeClass('btn-secondary').addClass('btn-primary');
    },

    sendCommand: function(command) {
        var command = this.input.val();
        console.log("send " + command);
        if (command.trim() != "") {
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
                this.console.val('');
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
        this.packetCount = 0;

        var url = $.getWebSocketLocation('/serial_console');
        var SID = $.getSessionId();
        var self = this;
        this.socket = new WS_Console(url, SID, 1, function(event) { self.dataHandler(event); }, this.console.attr('id'));
        this.socket.connect();
    },

    init: function() {
        var self = this;
        this.input.on('keypress', function(event) {
            if (event.keyCode == 13) {
                event.preventDefault();
                self.sendCommand();
            }
        });
        this.input.focus();

        this.sendButton.on('click', function(event) {
            event.preventDefault();
            self.sendCommand();
        });

        var filter = null;
        try {
            filter = JSON.parse(Cookies.get('serial2http_filter'));
        } catch(e) {
            filter = null;
        }
        if (!filter || !filter.value) {
            filter = this.filterDefault;
        }
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
