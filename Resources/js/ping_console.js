/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // ping.html
    if ($('#ping_console').length) {

        var wsc;
        var closeTimer = null;
        var start_button = $('#start_button');
        var stop_button = $('#stop_button');
        var clear_button = $('#clear_button');
        var ping_console = $('#ping_console');

        stop_button.prop('disabled', true);

        function clear_close_timer() {
            if (closeTimer) {
                window.clearTimeout(closeTimer);
            }
            closeTimer = null;
        }

        function ping_data_handler(e) {
            if (e.type == 'open') {
                stop_button.prop('disabled', false);
            }
            else if (e.type == 'close') {
                clear_close_timer();
                stop_button.prop('disabled', true);
            }
            else if (e.type == 'data') {
                if (e.data.match(/^\+CLOSE/)) {
                    if (!closeTimer) {
                        closeTimer = window.setTimeout(function() {
                            closeTimer = null;
                            stop_ping();
                        }, 5000);
                    }
                }
                else {
                    wsc.console_log(e.data);
                }
            }
        }

        function stop_ping() {
            clear_close_timer();
            wsc.disconnect();
        }

        function ping(host, count, timeout) {
            clear_close_timer();
            wsc.console_log('');
            wsc.console_log('Pinging ' + host + ' ' + count + ' time(s) with a timeout of ' + timeout + 'ms');
            wsc.socket.send('+PING ' + count + ' ' + timeout + ' ' + host);
        }

        function start_ping(host) {
            clear_close_timer();
            var host = $('#host').val().trim();
            var count = $('#count').val() * 1;
            var timeout = $('#timeout').val() * 1;
            if (count < 1) {
                count = 4;
            }
            if (timeout < 100) {
                timeout = 5000;
            }
            if (host == '') {
                wsc.console_log('Enter a hostname or IP address');
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
        start_button.on('click', function() {
            start_ping();
        });
        stop_button.on('click', function() {
            stop_ping();
        });
        clear_button.on('click', function() {
            wsc.console_clear();
        });

        wsc = new WS_Console($.getWebSocketLocation('/ping'), $.getSessionId(), 1, ping_data_handler, 'ping_console');
        var console_log_func = wsc.console_log;
        wsc.console_log = function(message) {
            if (closeTimer) {
                if (message.match(/^Connection closed/)) {
                    return;
                }
            }
            else {
                if (message.match(/^(Authentication successful|Connection has been est)/)) {
                    return;
                }
            }
            if (ping_console.val().indexOf('Connecting...') != -1) {
                ping_console.val(ping_console.val().replace(/(^|\s)Connecting...\n$/, '$1'));
            }
            console_log_func.apply(wsc, arguments);
        }
    }
});
