/**
 * Author: sascha_lammers@gmx.de
 */

// callbacks events

// callback({type: 'auth', socket: ws_console, success: true});
// callback({type: 'auth', socket: ws_console, success: false});
// callback({type: 'close', socket: ws_console, event: e, connect_counter: int, was_authenticated: bool });
// callback({type: 'error', socket: ws_console, event: e, connect_counter: int, was_authenticated: bool });
// callback({type: 'data', socket: ws_console, data: "message received from socket"});
// callback({type: 'binary', socket: ws_console, data: ArrayBuffer/blob});


window.ws_console_is_debug = false;


function WS_Console(url, sid, auto_reconnect, callback, consoleId) {
    this.url = url;
    this.sid = sid;
    this.consoleId = consoleId;
    this.socket = null;
    this.authenticated = false;
    this.callback = callback;
    this.auto_reconnect = auto_reconnect;
    this.reconnect_timeout = null;
    this.connect_counter = 0;
}

WS_Console.prototype.setConsoleId = function(id) {
    this.consoleId = id;
}

WS_Console.prototype.get_sid = function() {
    return this.sid;
}

WS_Console.prototype.console_log = function(message, prefix) {
    if (window.ws_console_is_debug) console.log("console_log", message);
    if (this.consoleId) {
        var consolePanel = document.getElementById(this.consoleId);

        var pos;
        while (pos = message.indexOf('\033[2J') != -1) {
            message = message.substr(pos, 4);
            consolePanel.value = "";
        }
        message.replace(/\\033\[\d(;\d)?H/g, '');

        if (prefix) {
            if (consolePanel.value.endsWith(message + "\n") && (pos = consolePanel.value.lastIndexOf("\n", consolePanel.value.length - message.length)) != -1) {
                consolePanel.value = consolePanel.value.substring(0, pos + 1) + prefix + message + "\n";
            } else {
                consolePanel.value += prefix + message + "\n";
            }

        } else {
            consolePanel.value += message + "\n";
        }
        consolePanel.scrollTop = consolePanel.scrollHeight;
    }
}

WS_Console.prototype.console_clear = function() {
    if (this.consoleId) {
        var consolePanel = document.getElementById(this.consoleId);
        consolePanel.value = "";
    }
}

WS_Console.prototype.is_connected = function() {
    return this.socket != null;
}

WS_Console.prototype.is_authenticated = function() {
    return this.is_connected() && this.authenticated;
}

WS_Console.prototype.send = function(data) {
    if (window.ws_console_is_debug) console.log("send", data);
    this.socket.send(data);
}

WS_Console.prototype.connect = function(authenticated_callback) {
    if (window.ws_console_is_debug) console.log("connect", this.url);
    if (this.reconnect_timeout != null) {
        window.clearTimeout();
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
    }
    this.authenticated = false;
    if (window.ws_console_is_debug) console.log("new WebSocket", this.url);
    this.socket = new WebSocket(this.url);
    this.socket.binaryType = 'arraybuffer';
    if (window.ws_console_is_debug) console.log(this.url);

    var ws_console = this;
    this.socket.onmessage = function(e) {
        if (window.ws_console_is_debug) console.log("onMessage", e);
        // if (e.data.substring(0, 16) == "+REQ_ENCRYPTION ") {
        //     data = e.data.split(" ");
        //     if (window.ws_console_is_debug) console.log("Authenticated and " + data[1] + " encryption requested");
        //     ws_console.authenticated = false;
        //     var key = convertHex(sha512(ws_console.get_sid() + data[2]));
        //     var aes = new aesjs.AES(key);
        //     ws_console.send("+SID " + window.btoa(aes.encrypt(ws_console.sid)));
        // } else
        if (e.data == "+REQ_AUTH") {
            ws_console.authenticated = false;
            ws_console.send("+SID " + ws_console.get_sid());
            // request encryption for the socket
            //ws_console.send("+REQ_ENCRYPTION AES256");
        } else if (e.data == "+AUTH_OK") {
            ws_console.console_log("Authentication successful");
            ws_console.callback({type: 'auth', success: true, socket: ws_console});
            ws_console.authenticated = true;
            if (authenticated_callback != undefined) {
                authenticated_callback();
            }
        } else if (e.data == "+AUTH_ERROR") {
            ws_console.console_log("Authentication failed");
            ws_console.callback({type: 'auth', success: false, socket: ws_console});
            ws_console.disconnect();
        } else {
            ws_console.callback({type: 'data', data: e.data, socket: ws_console});
        }
    }

    this.socket.onopen = function(e) {
        if (window.ws_console_is_debug) console.log("onOpen", e);
        ws_console.console_log("Connection has been established...");
        ws_console.connect_counter = 0;
        ws_console.callback({type: 'open', event: e});
    }

    this.socket.onclose = function(e) {
        if (window.ws_console_is_debug) console.log("onClose", e);
        ws_console.callback({type: 'close', socket: ws_console, event: e, connect_counter: ws_console.connect_counter, was_authenticated: ws_console.authenticated });
        if (ws_console.connect_counter == 0 && ws_console.authenticated) {
            ws_console.authenticated = false;
            ws_console.console_log("Connection closed...");
        }
        ws_console.disconnect(!e.wasClean);
    }

    this.socket.onerror = function(e) {
        if (window.ws_console_is_debug) console.log("onError", e);
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
    this.authenticated = false;
    if (this.socket != null) {
        if (this.socket != null) {
            this.socket.close();
            this.socket = null;
        }
    }
    if (reconnect) {
        if (this.reconnect_timeout == null && this.auto_reconnect) {
            var ws_console = this;
            ws_console.reconnect_timeout = window.setTimeout(function() {
                ws_console.connect();
            }, this.auto_reconnect * 1000);
        }
    }
}
