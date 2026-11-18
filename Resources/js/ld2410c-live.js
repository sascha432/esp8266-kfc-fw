/**
 * Author: sascha_lammers@gmx.de
 */

// Live view of the HLK-LD2410C gates (/ld2410c.html)
//
// The firmware pushes the data frames over the WebSocket /ld2410c-live, the module reports about 10 frames per second.
//   {"cfg":{"mm":8,"ms":8,"dur":5}}                     max. moving gate, max. still gate, nobody duration
//   {"f":[state,md,me,sd,se,dd],"m":[...],"s":[...],"l":light,"o":out}
// f: target state (0 none, 1 moving, 2 still, 3 both), moving distance (cm) and energy, still distance (cm) and energy,
// detection distance (cm). m/s: the moving and still energy of the 9 gates (0-100), the highest since the last
// message. They are missing while the module is not in engineering mode, the firmware reads it again to switch it on.
// The bars show the energy of each gate next to its values: a gate reports a target when its energy is above the value.

$(function() {

    var live = $('#ld2410c-live');
    if (!live.length) {
        return;
    }

    var kGateCount = 9;
    var kMaxEnergy = 100;
    var kStaleTimeout = 1500;       // ms without a message before the bars are cleared
    var kPeakMargin = 10;           // added to the peak by "raise values"

    var tools = $('#ld2410c-tools');
    var state = $('#ld2410c-live-state');
    var presence = $('#ld2410c-presence');
    var summary = $('#ld2410c-short');
    var target = $('#ld2410c-target');
    var toggle = $('#ld2410c-live-toggle');
    var config = null;
    var peaks = { m: [], s: [] };
    var bars = [];
    var lastMessage = 0;
    var hasData = false;
    var stopped = false;
    var socket = null;

    // below the title of the form: the line with the presence stays visible while the gates are scrolled, the buttons
    // and the details scroll with the page (a phone has no room for more)
    var header = $('#ld2410c > .card').first();
    if (header.length) {
        header.after(live);
        live.after(tools);
    }
    live.removeClass('hidden');
    tools.removeClass('hidden');

    function resetPeaks() {
        for (var i = 0; i < kGateCount; i++) {
            peaks.m[i] = -1;
            peaks.s[i] = -1;
        }
    }

    // the elements of a bar are kept: the bars are updated 10 times per second, without a style computation
    function createBar(name) {
        var row = $('<div class="ld2410c-row"><span class="ld2410c-name"></span><div class="ld2410c-bar"><div class="ld2410c-fill"></div>' +
            '<div class="ld2410c-peak"></div><div class="ld2410c-value"></div></div><span class="ld2410c-num">-</span></div>');
        row.find('.ld2410c-name').text(name);
        return {
            row: row,
            fill: row.find('.ld2410c-fill')[0],
            peak: row.find('.ld2410c-peak')[0],
            value: row.find('.ld2410c-value')[0],
            num: row.find('.ld2410c-num')[0]
        };
    }

    // two bars below the inputs of every gate
    for (var i = 0; i < kGateCount; i++) {
        var group = $('#lcm_' + i).closest('.form-group');
        var gate = $('<div class="ld2410c-gate"></div>');
        var move = createBar('move');
        var still = createBar('still');
        gate.append(move.row).append(still.row);
        group.append(gate);
        bars.push({ m: move, s: still });
    }
    resetPeaks();

    // the value of the input, NaN while it is not a number
    function getValue(type, gate) {
        return parseInt($('#lc' + type + '_' + gate).val(), 10);
    }

    function isGateUsed(type, gate) {
        if (!config) {
            return true;
        }
        return gate <= (type === 'm' ? config.mm : config.ms);
    }

    function percent(energy) {
        return (Math.max(0, Math.min(energy, kMaxEnergy)) * 100 / kMaxEnergy) + '%';
    }

    function updateBar(bar, type, gate, energy) {
        var value = getValue(type, gate);
        var used = isGateUsed(type, gate);
        var peak = peaks[type][gate];
        var over = used && energy !== null && !isNaN(value) && energy > value;
        bar.row[0].className = 'ld2410c-row' + (over ? ' ld2410c-over' : '') + (used ? '' : ' ld2410c-unused');
        bar.fill.style.width = (energy === null) ? '0' : percent(energy);
        bar.peak.style.left = percent(peak);
        bar.peak.style.display = (peak < 0) ? 'none' : '';
        bar.value.style.left = isNaN(value) ? '0' : percent(value);
        bar.value.style.display = isNaN(value) ? 'none' : '';
        var text = (energy === null) ? '-' : String(energy);
        if (peak >= 0) {
            text += ' (peak ' + peak + ')';
        }
        bar.num.textContent = text;
        bar.row[0].title = used ? '' : 'beyond the max. gate, the radar ignores it';
    }

    function updateBars(data) {
        for (var i = 0; i < kGateCount; i++) {
            var move = (data && data.m) ? data.m[i] : null;
            var still = (data && data.s) ? data.s[i] : null;
            if (move !== null && move > peaks.m[i]) {
                peaks.m[i] = move;
            }
            if (still !== null && still > peaks.s[i]) {
                peaks.s[i] = still;
            }
            updateBar(bars[i].m, 'm', i, move);
            updateBar(bars[i].s, 's', i, still);
        }
    }

    function meters(cm) {
        return (cm / 100).toFixed(2) + ' m';
    }

    function updateTarget(data) {
        var f = data.f;
        var text;
        var parts = [];
        if (f[0] === 0) {
            presence.text('Clear').attr('class', 'badge badge-success');
            parts.push('no target');
            text = 'No target';
        }
        else {
            presence.text('Detected').attr('class', 'badge badge-danger');
            // bit 0 moving, bit 1 still
            if (f[0] & 1) {
                parts.push('moving ' + meters(f[1]) + ' (' + f[2] + ')');
            }
            if (f[0] & 2) {
                parts.push('still ' + meters(f[3]) + ' (' + f[4] + ')');
            }
            text = 'moving ' + meters(f[1]) + ' energy ' + f[2] + ', still ' + meters(f[3]) + ' energy ' + f[4] + ', detection ' + meters(f[5]);
        }
        if (data.m) {
            text += ', light ' + data.l + ', OUT ' + (data.o ? 'high' : 'low');
        }
        summary.text(parts.join(', '));
        target.text(text);
    }

    function clearView(message) {
        hasData = false;
        presence.text('-').attr('class', 'badge badge-secondary');
        summary.text('-');
        target.text('-');
        updateBars(null);
        state.text(message);
    }

    function handleMessage(message) {
        var data;
        try {
            data = JSON.parse(message);
        }
        catch (e) {
            return;
        }
        if (data === null || typeof data !== 'object') {
            return;
        }
        if (data.cfg) {
            config = data.cfg;
            updateBars(null);
        }
        else if (data.f) {
            lastMessage = new Date().getTime();
            hasData = true;
            updateTarget(data);
            updateBars(data.m ? data : null);
            state.text(data.m ? 'Live view' :
                'The radar reports no gate energies (not in engineering mode), reading it again...');
        }
    }

    function socketHandler(event) {
        if (event.type === 'auth') {
            if (!event.success) {
                state.text('Authentication failed, reload the page');
            }
        }
        else if (event.type === 'open') {
            state.text('Connecting...');
        }
        else if (event.type === 'close') {
            clearView(stopped ? 'Live view stopped' : 'Connection closed, reconnecting...');
        }
        else if (event.type === 'error') {
            state.text('WebSocket error');
        }
        else if (event.type === 'data') {
            handleMessage(event.data);
        }
    }

    // a command holds the serial port, or the module does not report
    window.setInterval(function() {
        if (hasData && new Date().getTime() - lastMessage > kStaleTimeout) {
            clearView('No data from the radar, waiting for the serial port or the module');
        }
    }, 500);

    // the bars follow the values while they are edited
    $('#ld2410c').on('input change', 'input', function() {
        if (!hasData) {
            updateBars(null);
        }
    });

    $('#ld2410c-reset-peaks').on('click', function() {
        resetPeaks();
        updateBars(null);
    });

    // raises the gates whose peak reached their value, the others keep their sensitivity. A gate without energy (still
    // energy of gates 0 and 1) is left alone
    $('#ld2410c-raise').on('click', function() {
        var changed = 0;
        for (var i = 0; i < kGateCount; i++) {
            $.each(['m', 's'], function(index, type) {
                var peak = peaks[type][i];
                var value = getValue(type, i);
                if (peak <= 0 || isNaN(value) || peak < value) {
                    return;
                }
                var raised = Math.min(kMaxEnergy, peak + kPeakMargin);
                if (raised > value) {
                    $('#lc' + type + '_' + i).val(raised).trigger('change');
                    changed++;
                }
            });
        }
        state.text(changed ? changed + ' values raised, Save Changes writes them to the radar' : 'No gate reached its value');
    });

    toggle.on('click', function() {
        stopped = !stopped;
        toggle.text(stopped ? 'Start' : 'Stop');
        // the close event of the socket must not reconnect it
        socket.auto_reconnect = stopped ? 0 : 1;
        if (stopped) {
            socket.disconnect(false);
            clearView('Live view stopped');
        }
        else {
            socket.connect();
        }
    });

    socket = new WS_Console($.getWebSocketLocation('/ld2410c-live'), $.getSessionId(), 1, socketHandler, null);
    socket.connect();
});
