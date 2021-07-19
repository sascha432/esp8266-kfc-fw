1/**
 * Author: sascha_lammers@gmx.de
 */

 $(function() {
    var self = http2serialPlugin;
    if ($('#open_led_matrix').length == 0) {
        $('#sendbutton').parent().append('<button class="btn btn-outline-secondary" type="button" id="open_led_matrix">LED Matrix</button>');
        $('#open_led_matrix').on('click', function() {
            cmd = '+LMVIEW=33,udp:192.168.0.3:5001:5002';
            self.write(cmd + '\n');
            self.socket.send(cmd + '\r\n');
        })
    }
});

http2serialPlugin.ledMatrix = {
    pixel_size: 0.6,
    alpha1: 1.0,
    alpha2: 0.1,
    inner_size: 0,
    outer_size: 0,
    args: [0, 0],
    fps_time: 0,
    fps_value: 0,
    contents: '<h1>No visualization</h1>',
    data_handler: http2serialPlugin.dataHandler // store original handler
};

http2serialPlugin.zoomLedMatrix = function(dir) {
    this.ledMatrix.pixel_size += dir * 0.15;
    this.appendLedMatrix.apply(this, this.ledMatrix.args)
}

http2serialPlugin.appendLedMatrix = function(rows, cols, reverse_rows, reverse_cols, rotate, interleaved) {
    this.ledMatrix.args = arguments
    var _pixel_size = this.ledMatrix.pixel_size;
    var unit = 'rem';
    var pixel_size = _pixel_size + unit;
    var inner_size = (_pixel_size * 1) + unit;
    var outer_size = (_pixel_size * 7) + unit;
    var border_radius = _pixel_size + unit;
    var bg = '0, 0, 0';
    var mode = 'lighten';
    var mode2 = 'lighten';
    // var bg = '255, 255, 255';
    // var mode = 'normal';
    // var mode2 = 'lighten';

    $('#led-matrix').remove();

    // #pixel-container background: black
    // mix-blend-mode: lighten

    // #pixel-container background: white
    // mix-blend-mode: multiply

    this.ledMatrix.inner_size = inner_size;
    this.ledMatrix.outer_size = outer_size;
    $('body').append('<div id="led-matrix"><div id="pixel-container"></div></div><style type="text/css"> \
#pixel-container { \
    background: rgba(' + bg + '); \
} \
#pixel-container:hover .ipx { \
    font-size: 0.75rem; \
} \
#pixel-container .row { \
    display: block; \
} \
#pixel-container .px { \
    mix-blend-mode: ' + mode + '; \
    margin: ' + inner_size + '; \
    width: ' + pixel_size + '; \
    height: ' + pixel_size + '; \
    display: inline-block; \
    border-radius: ' + border_radius +  '; \
} \
#pixel-container .ipx { \
    mix-blend-mode: ' + mode2 + '; \
    margin: 0px; \
    width: 0px; \
    height: 0px; \
    display: inline; \
    border-radius: 0px; \
    color: white; \
    font-size: 0; \
} \
#pixel-container .toolbar { \
    width: 100%; \
    position: relative; \
    padding: 1rem; \
    left: -7rem; \
    bottom: 0; \
    z-index: 9999; \
    color: #ccc; \
    font-size: 1rem; \
} \
#pixel-container .toolbar .row { \
    display: flex; \
    align-items: flex-end; \
} \
#fps-number { \
    color: #aaa; \
    font-size: 1.25rem; \
} \
#pixel-container .zoom .oi { \
    padding: 0.25rem; \
    font-size: 1.25rem; \
    color: #bbb; \
} \
#pixel-container .oi-zoom .oi:hover { \
    color: #fff; \
} \
#pixel-container .col.brightness { \
    font-size: 1rem; \
} \
#close_led_matrix { \
    padding: 0.25rem; \
} \
</style></div>');

    var container = $('#pixel-container');
    if ($('#digits').length) {
        this.ledMatrix.contents = $('#digits').html();
        $('#digits').remove();
    }
    var contents = this.ledMatrix.contents;

    // else {
    //     function to_address(row, col) {
    //         if (rotate) {
    //             var tmp = row;
    //             row = col;
    //             col = tmp;
    //         }
    //         if (reverse_rows) {
    //             row = (rows - 1) - row;
    //         }
    //         if (reverse_cols) {
    //             col = (cols - 1) - col;
    //         }
    //         if (interleaved) {
    //             if (col % 2 == 1) {
    //                 row = (rows - 1) - row;
    //             }
    //         }
    //         return col * rows + row;
    //     }

    //     for (var i = 0; i < rows; i++) {
    //         contents += '<div class="row">';
    //         for (var j = 0; j < cols; j++) {
    //             var px_id = to_address(i, j);
    //             contents += '<div id="px' + px_id + '" class="px"><div class="ipx">' + px_id + '</div></div>';
    //         }
    //         contents += '</div>';
    //     }

    // }

    container.html(contents + '<div class="toolbar"> \
        <div class="row"> \
            <div class="col"> \
                <div class="fps-container"><span id="fps-number">-</span> fps <span id="dqueue"></span></div> \
                <div class="zoom"><span class="oi oi-zoom-in"></span><span class="oi oi-zoom-out"></span></div> \
            </div> \
            <div class="col"> \
                <div>Brightness</div> \
                <div><input type="range" id="led_brightness" min="0" max="255" value="0"></div> \
            </div> \
            <div class="col"> \
                <div>Center alpha</div> \
                <input class="form-control" type="text" value="1.0" id="input-center-alpha"> \
            </div> \
            <div class="col"> \
                <div>Diffusion alpha</div> \
                <input class="form-control" type="text" value="1.0" id="input-diff-alpha"> \
            </div> \
            <div class="col"> \
                <button class="btn btn-primary"><span class="oi oi-circle-x" id="close_led_matrix"></button> \
            </div> \
        </div> \
    </div>');


    if (window.populate_clock_digits) {
        window.populate_clock_digits();
    }

    var n = 0;
    for (var i = 0; i < rows; i++) {
        for (var j = 0; j < cols; j++) {
            var name = 'px' + n;
            n++;
            this.ledMatrixSetColor('#' + name, 89, 123, 234, 1.0, 0);
        }
    }

    var self = this;
    container.find('.oi.oi-zoom-in').on('click', function() { self.zoomLedMatrix(1); });
    container.find('.oi.oi-zoom-out').on('click', function() { self.zoomLedMatrix(-1); });
    $('#close_led_matrix').on('click', function() {
        cmd = '+LMVIEW=0,0';
        self.write(cmd + '\n');
        self.socket.send(cmd + '\r\n');
        $('#led-matrix').remove();
    });

    self = http2serialPlugin;
    $('#input-center-alpha').off('change');
    $('#input-diff-alpha').off('change');
    $('#input-center-alpha').val(self.ledMatrix.alpha1).on('change', function() {
        self.ledMatrix.alpha1 = parseFloat($(this).val());
        $('.px').css('box-shadow', '');
        $('.ipx').css('box-shadow', '');
    });
    $('#input-diff-alpha').val(self.ledMatrix.alpha2).on('change', function() {
        self.ledMatrix.alpha2 = parseFloat($(this).val());
        $('.px').css('box-shadow', '');
        $('.ipx').css('box-shadow', '');
    });

    this.ledMatrix.fps_time = 0;

    return container;
};

http2serialPlugin.ledMatrixSetColor = function(selector, r, g, b, a1, a2) {
    var is = this.ledMatrix.inner_size;
    var os = this.ledMatrix.outer_size;
    var tmp = $(selector);
    tmp.css('background', 'rgba(' + r + ',' + g + ',' + b + ',' + a1 + ')');
    tmp.css('border-color', 'rgba(' + r + ',' + g + ',' + b + ',' + a1 + ')');
    if (a1) {
        tmp.css('box-shadow', '0 0 ' + is + ' ' + is + ' rgba(' + r + ',' + g + ',' + b + ',' + a1 + ')');
    }
    if (a2) {
        tmp.find('.ipx').css('box-shadow', '0 0 ' + os + ' ' + os + ' rgba(' + r + ',' + g + ',' + b + ',' + a2 + ')');
    }
};

http2serialPlugin.countFps = function() {
    var t = new Date().getTime();
    if (this.ledMatrix.fps_time == 0) {
        this.ledMatrix.fps_value = 0;
    }
    else if (this.ledMatrix.fps_value == 0) {
        this.ledMatrix.fps_value = t - this.ledMatrix.fps_time;
    }
    else {
        var diff = (t - this.ledMatrix.fps_time) + 0.001;
        var itg = 3000.0 / diff;
        this.ledMatrix.fps_value = ((this.ledMatrix.fps_value * itg) + diff) / (itg + 1);
        $('#fps-number').html(Math.round(1000 / this.ledMatrix.fps_value * 100) / 100.0);
    }
    this.ledMatrix.fps_time = t;
}

http2serialPlugin.dataHandler = function(event) {
    $.http2serialPlugin.console.log("dataHandler", event);
    if (event.data instanceof ArrayBuffer) {
        var packetId = new Uint16Array(event.data, 0, 1);
        if (packetId == 0x0004) {   // WsClient::BinaryPacketType::LED_MATRIX_DATA
            var container = $('#pixel-container');
            if (container.length) {
                this.countFps();
                if (1) { //window.display_dqueue) {
                    if (event.type && event.type == 'udp') {
                        var tmp = new Uint16Array(event.data, 2, 1);
                        $('#dqueue').html('<br>dropped ' + (tmp >> 4));
                    }
                    else {
                        var tmp = new Uint16Array(event.data, 2, 1);
                        var queue_size = tmp & 0xf;
                        var queue_mem_size = (tmp & 0xfff0) << 1;
                        $('#dqueue').html('<br>queue ' + queue_size + ' ' + queue_mem_size);
                    }
                }

                var num_pixels = this.ledMatrix.args[0] * this.ledMatrix.args[1];
                var pixels = new Uint16Array(event.data, 4, num_pixels);
                for (var i = 0; i < num_pixels; i++)  {
                    var pixel = $._____rgb565_to_888(pixels[i]);
                    this.ledMatrixSetColor('#px' + i, pixel[0], pixel[1], pixel[2], this.ledMatrix.alpha1, this.ledMatrix.alpha2);
                }
            }
        }
        return;
    }
    else if (event.type == 'data') {
        var lines = event.data.split('\n');
        for(var i = 0; i < lines.length; i++) {
            var line = lines[i];
            if (line.startsWith('+LED_MATRIX_BRIGHTNESS=')) {
                var br = parseInt(line.substring(23));
                $('#led_brightness').val(br);
            }
            else if (line.startsWith('+LED_MATRIX=')) {
                $('#led-matrix').remove();
                args = line.substring(12).split(',');
                if (args.length >= 2) {
                    while(args.lenth < 6) {
                        args.push(0);
                    }
                    this.appendLedMatrix(parseInt(args[0]), parseInt(args[1]), parseInt(args[2]), parseInt(args[3]), parseInt(args[4]), parseInt(args[5]));
                }
            }
            else if (line.startsWith('+LED_MATRIX_SERVER=')) {
                $('#led-matrix').remove();
                self = this
                args = line.substring(19).split(',');
                console.log('lex-matrix-server', args);
                if (args.length >= 3) {
                    while(args.lenth < 7) {
                        args.push(0);
                    }
                    var ws = new WebSocket(args[0]);
                    ws.onopen = function(e) {
                        console.log('web socket connected', args);
                        self.appendLedMatrix(parseInt(args[1]), parseInt(args[2]), parseInt(args[3]), parseInt(args[4]), parseInt(args[5]), parseInt(args[6]));
                        self.write('web socket connected: ' + args[0] + '\n');
                    };
                    ws.onerror = ws.onclose = function(e) {
                        console.log('web socket error', e, args);
                        $('#led-matrix').remove();
                        self.write('web socket closed: ' + args[0] + '\n');
                        try {
                            ws.close();
                        } catch(e) {
                        }
                        ws = null;
                    };
                    ws.onmessage = function(e) {
                        if (e.data instanceof Blob) {
                            e.data.arrayBuffer().then(function(arr) {
                                self.dataHandler({'data': arr, 'type': 'udp'});
                            });
                        }
                    };
                }
            }
        }
        // call original handler
        this.ledMatrix.data_handler.call(http2serialPlugin, event);
    }
};
