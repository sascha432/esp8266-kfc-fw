/**
 * Author: sascha_lammers@gmx.de
 */

function WS_Console(url, sid, auto_reconnect, callback) {
    this.url = url;
    this.sid = sid;
    this.consoleId = "rxConsole";
    this.socket = null;
    this.authenticated = false;
    this.callback = callback;
    this.auto_reconnect = auto_reconnect;
    this.reconnect_timeout = null;
}

WS_Console.prototype.setConsoleId = function(id) {
    this.consoleId = id;
}

WS_Console.prototype.get_sid = function() {
    return this.sid;
}

WS_Console.prototype.console_log = function(message) {
    if (this.consoleId) {
        var console = document.getElementById(this.consoleId);

        var pos;
        while (pos = message.indexOf('\033[2J') != -1) {
            message = message.substr(pos, 4);
            console.value = "";
        }
        message.replace(/\\033\[\d(;\d)?H/g, '');
    
        console.value += message + "\n";
        console.scrollTop = console.scrollHeight;
    }
}

WS_Console.prototype.console_clear = function() {
    if (this.consoleId) {
        var console = document.getElementById(this.consoleId);
        console.value = "";
    }
}

WS_Console.prototype.is_connected = function() {
    return this.socket != null;
}

WS_Console.prototype.is_authenticated = function() {
    return this.is_connected() && this.authenticated;
}

WS_Console.prototype.send = function(data) {
    this.socket.send(data);
}

WS_Console.prototype.connect = function(authenticated_callback) {
    console.log("connect");
    if (this.reconnect_timeout != null) {
        this.reconnect_timeout.clearTimeout();
        this.reconnect_timeout = null;
    }
    this.console_log("Connecting...")
    if (this.socket != null) {
        this.socket.close();
    }
    this.authenticated = false;
    console.log("new websocket");
    this.socket = new WebSocket(this.url);
    console.log(this.url);

    var ws_console = this;
    this.socket.onmessage = function(e) {
        console.log(e);
        // if (e.data.substring(0, 16) == "+REQ_ENCRYPTION ") {
        //     data = e.data.split(" ");
        //     console.log("Authenticated and " + data[1] + " encryption requested");
        //     ws_console.authenticated = false;
        //     var key = convertHex(sha512(ws_console.get_sid() + data[2]));
        //     var aes = new aesjs.AES(key);
        //     ws_console.send("+SID " + window.btoa(aes.encrypt(ws_console.sid)));
        // } else
        if (e.data == "+REQ_AUTH") {
            console.log("Authentication requested");
            ws_console.authenticated = false;
            ws_console.send("+SID " + ws_console.get_sid());
            // request encryption for the socket
            //ws_console.send("+REQ_ENCRYPTION AES256");
        } else if (e.data == "+AUTH_OK") {
            ws_console.console_log("Authentication successful");
            ws_console.authenticated = true;
            if (authenticated_callback != undefined) {
                authenticated_callback();
            }
        } else if (e.data == "+AUTH_ERROR") {
            console.log("Authentication failed");
            ws_console.disconnect();
        } else {
            ws_console.callback(e);
        }
    }

    this.socket.onopen = function(e) {
        ws_console.console_log("Connection has been established...");
    }

    this.socket.onclose = function(e, no_message) {
        if (no_message == undefined) {
            ws_console.console_log("Connection closed...");
        }
        ws_console.disconnect();
    }

    this.socket.onerror = function(e) {
        ws_console.console_log("Connection lost...");
        ws_console.disconnect(e, true);
        if (ws_console.reconnect_timeout == null && ws_console.auto_reconnect) {
            ws_console.reconnect_timeout = window.setTimeout(function() {
                this.connect();
            }, ws_console.auto_reconnect * 1000);
        }
    }
}

WS_Console.prototype.disconnect = function(display_message) {
    if (this.socket != null) {
        if (display_message == undefined) {
            display_message = true;
        }
        if (this.socket != null) {
            if (display_message) {
                this.console_log("Disconnecting...");
            }
            this.socket.close();
            this.socket = null;
        }
    }
}
