/**
 * Author: sascha_lammers@gmx.de
 */

// Live view of the HLK-LD2450 radar (/ld2450-radar.html)
//
// The firmware pushes the targets over the WebSocket /ld2450-live instead of one HTTP request per frame: the module
// reports about 10 frames per second.
//   {"cfg":{"multi":1,"bluetooth":0,"zone":2,"areas":[[x1,y1,x2,y2],[x1,y1,x2,y2],[x1,y1,x2,y2]]}}
//   {"t":[[x,y,speed,resolution],...]}             x/y in mm, speed in mm/s, resolution in mm
// A target slot without a target has x = y = 0. The configuration is sent when the page connects.

$(function() {

    if (!$('#ld2450-radar-canvas').length) {
        return;
    }

    var kMaxRange = 6000;           // mm, the detection range of the module
    var kHalfAngle = 60;            // degrees to either side of the sensor
    var kRingStep = 1000;           // mm between the range rings
    var kTrailLength = 12;          // positions the trail of a target keeps
    var kStaleTimeout = 1500;       // ms without a message before the targets are removed
    var kColors = ['#e74c3c', '#27ae60', '#2980b9'];

    var canvas = document.getElementById('ld2450-radar-canvas');
    var ctx = canvas.getContext('2d');
    var state = $('#ld2450-radar-state');
    var targets = [];
    var trails = [[], [], []];
    var config = null;
    var lastMessage = 0;
    var socket = null;

    function hasTarget(target) {
        return target && (target[0] !== 0 || target[1] !== 0);
    }

    function rad(degrees) {
        return degrees * Math.PI / 180;
    }

    // the sensor is drawn at the bottom center, the fan opens upwards
    function geometry() {
        var oy = canvas.height - 26;
        var scaleY = (oy - 40) / kMaxRange;
        var scaleX = (canvas.width / 2 - 26) / (Math.sin(rad(kHalfAngle)) * kMaxRange);
        var scale = Math.min(scaleX, scaleY);
        return { ox: canvas.width / 2, oy: oy, scale: scale };
    }

    function drawBackground(t, angleStart, angleEnd) {
        // the detection area of the module
        ctx.beginPath();
        ctx.moveTo(t.ox, t.oy);
        ctx.arc(t.ox, t.oy, kMaxRange * t.scale, angleStart, angleEnd);
        ctx.closePath();
        ctx.fillStyle = 'rgba(52, 152, 219, 0.08)';
        ctx.fill();

        ctx.font = '12px sans-serif';
        ctx.lineWidth = 1;

        // radial lines every 30 degrees
        ctx.strokeStyle = 'rgba(120, 140, 160, 0.28)';
        ctx.fillStyle = 'rgba(190, 200, 210, 0.85)';
        for (var angle = -kHalfAngle; angle <= kHalfAngle; angle += 30) {
            var a = rad(-90 + angle);
            var x = t.ox + Math.cos(a) * kMaxRange * t.scale;
            var y = t.oy + Math.sin(a) * kMaxRange * t.scale;
            ctx.beginPath();
            ctx.moveTo(t.ox, t.oy);
            ctx.lineTo(x, y);
            ctx.stroke();
            ctx.fillText(angle + '\u00b0', x + Math.cos(a) * 10 - 8, y + Math.sin(a) * 10 + 4);
        }

        // range rings with their distance
        ctx.strokeStyle = 'rgba(120, 140, 160, 0.45)';
        ctx.fillStyle = 'rgba(220, 225, 230, 0.9)';
        for (var r = kRingStep; r <= kMaxRange; r += kRingStep) {
            ctx.beginPath();
            ctx.arc(t.ox, t.oy, r * t.scale, angleStart, angleEnd);
            ctx.stroke();
            ctx.fillText((r / 1000).toFixed(0) + ' m', t.ox + 4, t.oy - r * t.scale - 3);
        }

        // the sensor
        ctx.fillStyle = '#3c4b5c';
        ctx.fillRect(t.ox - 22, t.oy - 4, 44, 14);
        ctx.fillStyle = 'rgba(220, 225, 230, 0.9)';
        ctx.fillText('LD2450', t.ox - 20, t.oy + 8);
    }

    function drawZones(t) {
        if (!config || !config.zone) {
            return;
        }
        var filter = (config.zone === 2);
        for (var i = 0; i < config.areas.length; i++) {
            var area = config.areas[i];
            if (!area[0] && !area[1] && !area[2] && !area[3]) {
                continue;
            }
            var x1 = Math.min(area[0], area[2]);
            var x2 = Math.max(area[0], area[2]);
            var y1 = Math.min(area[1], area[3]);
            var y2 = Math.max(area[1], area[3]);
            var left = t.ox + x1 * t.scale;
            var top = t.oy - y2 * t.scale;
            var width = (x2 - x1) * t.scale;
            var height = (y2 - y1) * t.scale;
            ctx.fillStyle = filter ? 'rgba(231, 76, 60, 0.10)' : 'rgba(46, 204, 113, 0.10)';
            ctx.strokeStyle = filter ? 'rgba(231, 76, 60, 0.55)' : 'rgba(46, 204, 113, 0.55)';
            ctx.fillRect(left, top, width, height);
            ctx.strokeRect(left, top, width, height);
        }
    }

    function drawTrails(t) {
        ctx.lineWidth = 2;
        for (var i = 0; i < trails.length; i++) {
            var trail = trails[i];
            if (trail.length < 2) {
                continue;
            }
            ctx.strokeStyle = kColors[i];
            for (var j = 1; j < trail.length; j++) {
                ctx.globalAlpha = 0.15 + 0.65 * j / trail.length;
                ctx.beginPath();
                ctx.moveTo(t.ox + trail[j - 1][0] * t.scale, t.oy - trail[j - 1][1] * t.scale);
                ctx.lineTo(t.ox + trail[j][0] * t.scale, t.oy - trail[j][1] * t.scale);
                ctx.stroke();
            }
        }
        ctx.globalAlpha = 1;
        ctx.lineWidth = 1;
    }

    function drawTargets(t) {
        for (var i = 0; i < targets.length; i++) {
            if (!hasTarget(targets[i])) {
                continue;
            }
            var x = t.ox + targets[i][0] * t.scale;
            var y = t.oy - targets[i][1] * t.scale;
            ctx.globalAlpha = 0.25;
            ctx.fillStyle = kColors[i];
            ctx.beginPath();
            ctx.arc(x, y, 14, 0, 2 * Math.PI);
            ctx.fill();
            ctx.globalAlpha = 1;
            ctx.beginPath();
            ctx.arc(x, y, 6, 0, 2 * Math.PI);
            ctx.fill();
            ctx.fillStyle = '#fff';
            ctx.font = 'bold 11px sans-serif';
            ctx.fillText(String(i + 1), x - 3, y + 4);
        }
    }

    function draw() {
        var t = geometry();
        var angleStart = rad(-90 - kHalfAngle);
        var angleEnd = rad(-90 + kHalfAngle);
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        drawBackground(t, angleStart, angleEnd);
        drawZones(t);
        drawTrails(t);
        drawTargets(t);
    }

    // the values of the target cards
    function updateTargets() {
        for (var i = 0; i < 3; i++) {
            var card = $('#ld2450-target-' + i);
            var target = targets[i];
            if (!hasTarget(target)) {
                card.removeClass('has-target');
                card.find('.ld2450-distance, .ld2450-angle, .ld2450-speed, .ld2450-position').text('-');
                continue;
            }
            var x = target[0];
            var y = target[1];
            var distance = Math.sqrt(x * x + y * y);
            var angle = Math.atan2(x, y) * 180 / Math.PI;
            card.addClass('has-target');
            card.find('.ld2450-distance').text((distance / 1000).toFixed(2) + ' m');
            card.find('.ld2450-angle').text((angle >= 0 ? '+' : '') + angle.toFixed(1) + '\u00b0');
            card.find('.ld2450-speed').text((target[2] / 1000).toFixed(2) + ' m/s');
            card.find('.ld2450-position').text('x ' + x + ' mm, y ' + y + ' mm, resolution ' + target[3] + ' mm');
        }
    }

    function addToTrails() {
        for (var i = 0; i < 3; i++) {
            var trail = trails[i];
            if (!hasTarget(targets[i])) {
                trail.length = 0;
                continue;
            }
            var last = trail[trail.length - 1];
            if (!last || last[0] !== targets[i][0] || last[1] !== targets[i][1]) {
                trail.push([targets[i][0], targets[i][1]]);
                if (trail.length > kTrailLength) {
                    trail.shift();
                }
            }
        }
    }

    function resetTargets() {
        targets = [];
        for (var i = 0; i < 3; i++) {
            trails[i].length = 0;
        }
        updateTargets();
        draw();
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
            draw();
        }
        else if (data.t) {
            targets = data.t;
            lastMessage = new Date().getTime();
            addToTrails();
            draw();
            updateTargets();
            state.text('Live view, ' + (config && config.multi ? 'multiple' : 'single') + ' target tracking');
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
            state.text('Connection closed, reconnecting...');
            resetTargets();
        }
        else if (event.type === 'error') {
            state.text('WebSocket error');
        }
        else if (event.type === 'data') {
            handleMessage(event.data);
        }
    }

    // the module stops sending when it is disconnected: remove the targets instead of showing them forever
    window.setInterval(function() {
        if (targets.length && new Date().getTime() - lastMessage > kStaleTimeout) {
            state.text('No data from the radar');
            resetTargets();
        }
    }, 500);

    draw();
    updateTargets();
    socket = new WS_Console($.getWebSocketLocation('/ld2450-live'), $.getSessionId(), 1, socketHandler, null);
    socket.connect();
});
