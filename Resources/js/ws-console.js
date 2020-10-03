/**
 * Author: sascha_lammers@gmx.de
 */

// callbacks events

// callback({type: 'auth', socket: ws_console, success: true});
// callback({type: 'auth', socket: ws_console, success: false});
// callback({type: 'close', socket: ws_console, event: e, connect_counter: int, was_authenticated: bool });
// callback({type: 'error', socket: ws_console, event: e, connect_counter: int, was_authenticated: bool });
// callback({type: 'data', socket: ws_console, data: "message received from socket"}); typeof event.data === 'string'
// callback({type: 'binary', socket: ws_console, data: ArrayBuffer}); -> event.data instanceof ArrayBuffer
// callback({type: 'object', socket: ws_console, data: {} }); -> typeof event.data === 'object'
// auto_reconnect: time in seconds or 0 to disable
function WS_Console(url, sid, auto_reconnect, callback, consoleId) {
    dbg_console.called('new WS_Console', arguments);
    this.url = url;
    this.sid = sid;
    this.consoleId = consoleId;
    this.socket = null;
    this.authenticated = false;
    this.callback = callback;
    this.auto_reconnect = auto_reconnect;
    this.reconnect_timeout = null;
    this.connect_counter = 0;
    this.connection_counter = 0;
    this.ping_interval = null;
}

WS_Console.prototype.setConsoleId = function(id) {
    this.consoleId = id;
}

WS_Console.prototype.get_sid = function() {
    return this.sid;
}

WS_Console.prototype.keep_alive = function() {
    dbg_console.debug('iPing', this.is_connected(), this.is_authenticated());
    if (this.is_connected()) {
        var d = new Date();
        this.socket.send('+iPING ' + (d.getTime() + d.getMilliseconds() / 1000.0));
    }
}

WS_Console.prototype.reset_ping_interval = function(set_new_interval) {
    if (this.ping_interval) {
        window.clearInterval(this.ping_interval);
        this.ping_interval = null;
    }
    if (set_new_interval) {
        var self = this;
        this.ping_interval = window.setInterval(function() {
            self.keep_alive();
        }, 5000);
    }
}

WS_Console.prototype.console_log = function(message, prefix) {
    dbg_console.called('console_log', arguments);
    if (this.consoleId) {
        var consolePanel = document.getElementById(this.consoleId);
        var scrollPos = consolePanel.scrollHeight - consolePanel.scrollTop - $(consolePanel).innerHeight();

        // dbg_console.debug('msg escpape', message.indexOf('\033'), "msg", message, "replace esc", message.replace(/\033\[[\d;]*[mHJ]/g, ''));

        // if (message.indexOf('\033') != -1) {
        //     var pos;
        //     if (pos = message.lastIndexOf('\033[2J') != -1) {
        //         message = message.substr(pos + 4);
        //         consolePanel.value = '';
        //         dbg_console.debug("clear screen", message);
        //     }
        //     // discard other vt100 sequences
        //     // https://github.com/xtermjs/xterm.js
        //     message = message.replace(/\033\[[\d;]*[mHJ]/g, '');
        //     dbg_console.debug("replaced", message);
        // }

        if (prefix) {
            if (consolePanel.value.endsWith(message + "\n") && (pos = consolePanel.value.lastIndexOf("\n", consolePanel.value.length - message.length)) != -1) {
                consolePanel.value = consolePanel.value.substring(0, pos + 1) + prefix + message + "\n";
            } else {
                consolePanel.value += prefix + message + "\n";
            }
        } else {
            consolePanel.value += message + "\n";
        }
        // disable auto scroll if the user scrolled 50 pixel up or more
        if (scrollPos <= 50) {
            consolePanel.scrollTop = consolePanel.scrollHeight;
        }
    }
}

WS_Console.prototype.console_clear = function() {
    if (this.consoleId) {
        var consolePanel = document.getElementById(this.consoleId);
        consolePanel.value = '';
    }
}

WS_Console.prototype.is_connected = function() {
    return this.socket != null;
}

WS_Console.prototype.is_authenticated = function() {
    return this.is_connected() && this.authenticated;
}

WS_Console.prototype.send = function(data) {
    dbg_console.called('send', arguments);
    this.socket.send(data);
}

WS_Console.prototype.connect = function(authenticated_callback) {
    dbg_console.called('connect', arguments);
    dbg_console.assert(this.connection_counter++ == 0)

    dbg_console.debug('connect_counter', this.connect_counter, 'connection_counter', this.connection_counter, 'reconnect_timeout', this.reconnect_timeout);
    dbg_console.debug(this);
    if (this.reconnect_timeout) {
        window.clearTimeout(this.reconnect_timeout);
        this.reconnect_timeout = null;
    }
    if (this.connect_counter++ == 0) {
        if (this.authenticated) {
            this.console_log("Reconnecting...")
        } else {
            this.console_log("Connecting...")
        }
    }
    if (this.socket != null) {
        this.socket.close();
        this.socket = null;
    }
    this.authenticated = false;

    // var sid = this.get_sid();
    // var tmpUrl = this.url.replace(/^ws{1,2}:\/\//, '$&' + sid.substring(0, 16) + ':' + sid.substring(16) + '@');
    // this.socket = new WebSocket(tmpUrl);

    this.socket = new WebSocket(this.url);

    dbg_console.debug('WebSocket:', this.url);
    dbg_console.debug(this);
    this.socket.binaryType = 'arraybuffer';

    var ws_console = this;
    this.socket.onmessage = function(e) {
        dbg_console.called('WS_Console.socket.onmessage', arguments);
        dbg_console.debug(ws_console);
        ws_console.reset_ping_interval(true);
        if (e.data instanceof ArrayBuffer) {
            ws_console.callback({type: 'binary', data: e.data, socket: ws_console});
        }
        else if (typeof e.data === 'string') {
            if (e.data.match(/^\+iPONG /)) {
                // ignore
            }
            else if (e.data == "+REQ_AUTH") {
                ws_console.authenticated = false;
                ws_console.send("+SID " + ws_console.get_sid());
            }
            else if (e.data == "+AUTH_OK") {
                ws_console.console_log("Authentication successful");
                ws_console.callback({type: 'auth', success: true, socket: ws_console});
                ws_console.authenticated = true;
                if (authenticated_callback != undefined) {
                    authenticated_callback();
                }
            }
            else if (e.data == "+AUTH_ERROR") {
                ws_console.console_log("Authentication failed");
                ws_console.callback({type: 'auth', success: false, socket: ws_console});
                ws_console.disconnect();
            }
            else {
                ws_console.callback({type: 'data', data: e.data, socket: ws_console});
            }
        }
        else if (typeof e.data === 'object') {
            ws_console.callback({type: 'object', data: e.data, socket: ws_console});
        }
        else {
            dbg_console.error('unexpected type of data = ', typeof e.data, 'event', e);
        }
    }

    this.socket.onopen = function(e) {
        dbg_console.called('WS_Console.socket.onopen', arguments);
        dbg_console.debug(ws_console);
        ws_console.console_log("Connection has been established...");
        ws_console.connect_counter = 0;
        ws_console.callback({type: 'open', event: e});
        ws_console.reset_ping_interval(true);
    }

    this.socket.onclose = function(e) {
        dbg_console.called('WS_Console.socket.online', arguments);
        dbg_console.debug(ws_console);
        ws_console.callback({type: 'close', socket: ws_console, event: e, connect_counter: ws_console.connect_counter, was_authenticated: ws_console.authenticated });
        if (ws_console.connect_counter == 0 && ws_console.authenticated) {
            ws_console.authenticated = false;
            ws_console.console_log("Connection closed... (code=" + e.code + " reason=" + e.reason + " clean=" + e.wasClean + ")");
        }
        ws_console.disconnect(!e.wasClean);
    }

    this.socket.onerror = function(e) {
        dbg_console.called('WS_Console.socket.onerror', arguments);
        dbg_console.debug(ws_console);
        ws_console.callback({type: 'error', socket: ws_console, event: e, connect_counter: ws_console.connect_counter, was_authenticated: ws_console.authenticated });
        if (ws_console.authenticated) {
            ws_console.console_log("Connection lost...");
        } else {
            ws_console.console_log("Connection could not be established, retrying...", "[" + ws_console.connect_counter + "] ");
        }
        ws_console.disconnect(true);
    }
}

WS_Console.prototype.disconnect = function(reconnect) {
    dbg_console.called('WS_Console.disconnect', arguments);
    dbg_console.debug(this);

    this.reset_ping_interval(false);

    this.authenticated = false;
    this.connection_counter--;
    if (this.socket != null) {
        this.socket.close();
        this.socket = null;
    }
    if (reconnect) {
        if (this.reconnect_timeout == null && this.auto_reconnect) {
            dbg_console.debug('reconnecting in ' + this.auto_reconnect + ' seconds')
            var ws_console = this;
            ws_console.reconnect_timeout = window.setTimeout(function() {
                dbg_console.called('WS_Console.reconnect_timeout', arguments);
                dbg_console.debug(ws_console);
                ws_console.connect();
            }, this.auto_reconnect * 1000);
        }
    }
}
