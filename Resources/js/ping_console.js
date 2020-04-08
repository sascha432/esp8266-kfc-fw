/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // ping.html
    if ($('.ping-console').length) {

        var wsc;

        function ping_data_handler(e) {
            if (e.data) {
                wsc.console_log(e.data);
            }
        }

        function stop_ping() {
            wsc.disconnect();
        }

        function ping(host, count, timeout) {
            wsc.console_log("Pinging " + host + " " + count + " time(s) with a timeout of " + timeout + "ms");
            wsc.socket.send("+PING " + count + " " + timeout + " " + host);
        }

        function start_ping(host) {
            var host = $('#host').val().trim();
            var count = $('#count').val() * 1;
            var timeout = $('#timeout').val() * 1;
            if (count < 1) {
                count = 4;
            }
            if (timeout < 1) {
                timeout = 5000;
            }
            if (host == "") {
                wsc.console_log("Enter a hostname or IP address");
            } else {
                if (wsc.is_authenticated()) {
                    ping(host, count, timeout);
                } else {
                    wsc.connect(function() {
                        ping(host, count, timeout);
                    });
                }
            }
        }

        $('#host').focus().on('keypress', function(e) {
            if (e.charCode == 13) {
                e.preventDefault();
                start_ping();
            }
        });

        $('#start_button').on('click', function(e) {
            e.preventDefault();
            start_ping();
        });

        $('#stop_button').on('click', function(e) {
            e.preventDefault();
            stop_ping();
        });

        $('#clear_button').on('click', function(e) {
            e.preventDefault();
            wsc.console_clear();
        });

        wsc = new WS_Console($.getWebSocketLocation('/ping'), $.getSessionId(), 1, ping_data_handler, "rxConsole");
    }
});
