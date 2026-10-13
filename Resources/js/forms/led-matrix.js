/**
 * Author: sascha_lammers@gmx.de
 */

 $.initFormLedMatrixFunc = function() {
    if ($('#collapse-rainbow').length) {
        $.initFormLedMatrixFunc();
    }
 };

$(function () {

    // led-matrix/irremote.html (and clock/irremote.html): capture the code of a remote control
    // button. The device disables all remote control actions while the dialog is open.
    if ($('.ir_remote_learn').length) {

        var IR_REMOTE_URL = '/ir-remote.json';
        var ir_remote_timer = null;
        var ir_remote_dialog = null;
        var ir_remote_input = null;
        var ir_remote_id = -1;
        var ir_remote_code = '------------';
        var ir_remote_active = false;

        function ir_remote_get(action, callback) {
            var url = IR_REMOTE_URL + '?action=' + action + '&SID=' + $.getSessionId() + '&rnd=' + random_str();
            $.get(url, function (data) {
                if (typeof data === 'string') {
                    try {
                        data = JSON.parse(data);
                    } catch (e) {
                        data = null;
                    }
                }
                if (typeof callback === 'function') {
                    callback(data);
                }
            }, 'json').fail(function () {
                if (typeof callback === 'function') {
                    callback(null);
                }
            });
        }

        function ir_remote_stop_polling() {
            if (ir_remote_timer !== null) {
                window.clearTimeout(ir_remote_timer);
                ir_remote_timer = null;
            }
        }

        function ir_remote_poll() {
            ir_remote_timer = null;
            if (!ir_remote_active) {
                return;
            }
            ir_remote_get('read', function (data) {
                if (data === null) {
                    ir_remote_dialog.find('#ir_remote_message').text('Communication error! The actions stay disabled until the dialog is closed or the device times out');
                    ir_remote_dialog.find('#ir_remote_spinner').addClass('hidden');
                }
                else if (typeof data.id === 'number' && data.id != ir_remote_id) {
                    ir_remote_id = data.id;
                    if (data.code && data.code != '00000000') {
                        ir_remote_code = data.code;
                        ir_remote_dialog.find('#ir_remote_code').text(ir_remote_code);
                        ir_remote_dialog.find('#ir_remote_message').text('Code received, click "Use Code" to assign it to the field or press another button');
                        ir_remote_dialog.find('#ir_remote_spinner').addClass('hidden');
                        ir_remote_dialog.find('#ir_remote_use').prop('disabled', false);
                    }
                }
                // the dialog can be closed while a request is in flight
                if (ir_remote_active) {
                    ir_remote_timer = window.setTimeout(ir_remote_poll, 250);
                }
            });
        }

        $('.ir_remote_learn').on('click', function (e) {
            e.preventDefault();

            ir_remote_input = $(this).closest('.form-group').find('input').first();
            ir_remote_id = -1;
            ir_remote_code = '------------';
            ir_remote_active = true;

            var body =
                '<div class="text-center">' +
                '<p id="ir_remote_message">Press a button on the remote control...</p>' +
                '<p><span class="h2 text-monospace font-weight-bold" id="ir_remote_code">------------</span></p>' +
                '<p><img src="/images/spinner.gif" width="48" height="48" border="0" id="ir_remote_spinner"></p>' +
                '<p class="text-muted small">All remote control actions are disabled while this dialog is open</p>' +
                '</div>';

            ir_remote_dialog = $.addModalDialog('ir_remote_dialog', 'Capture IR Remote Code', body, function () {
                // the dialog is gone, enable the actions again
                ir_remote_active = false;
                ir_remote_stop_polling();
                ir_remote_get('stop');
            });
            ir_remote_dialog.find('.modal-footer').prepend('<button type="button" class="btn btn-primary" id="ir_remote_use" disabled>Use Code</button>');
            ir_remote_dialog.find('#ir_remote_use').on('click', function () {
                if (ir_remote_input && ir_remote_input.length) {
                    ir_remote_input.val(ir_remote_code).trigger('change');
                }
                ir_remote_dialog.modal('hide');
            });

            // disable all actions and start polling
            ir_remote_get('learn', function (data) {
                if (data === null) {
                    ir_remote_dialog.find('#ir_remote_message').text('Failed to enable the capture mode');
                    ir_remote_dialog.find('#ir_remote_spinner').addClass('hidden');
                    return;
                }
                if (!ir_remote_active) {
                    return;
                }
                if (typeof data.id === 'number') {
                    ir_remote_id = data.id;
                }
                ir_remote_poll();
            });
        });

    }

    // led-matrix/matrix.html: every segment (output pin) is a panel placed in the matrix, see
    // matrix_validation.h. The server rejects a panel outside the matrix or larger than the pixel buffer
    // (hard errors), overlapping panels and uncovered matrix pixels are hints only. The read only field of
    // each segment shows its LED count and linear offset (where its chain starts in the pixel buffer: the
    // LEDs of the enabled segments before it) while editing
    if ($('#mx_pr0').length && $('#mx_px').length) {
        var matrix_max_pixels = parseInt($('#mx_px').val()) || 0;
        var matrix_form = $('#mx_pr0').closest('form');
        var matrix_fields = ['#dm', '#mx_rows', '#mx_cols'];
        for (var n = 0; n < 4; n++) {
            matrix_fields.push('#mx_en' + n, '#mx_pr' + n, '#mx_pc' + n, '#mx_prp' + n, '#mx_pcp' + n);
        }

        // the transport limits are rendered by the form (see matrix_validation.h). The checks below
        // are warning/info only, the server blocks only on errors
        var matrix_attr = function (name, fallback) {
            var value = parseInt($('#mx_px').attr('data-' + name));
            return isNaN(value) ? fallback : value;
        };
        var matrix_max_strips = matrix_attr('max-strips', 4);
        var matrix_i2s_strips = matrix_attr('i2s-strips', 0);
        var matrix_i2s_port = matrix_attr('i2s-port', 255);
        var matrix_method_rmt = matrix_attr('method-rmt', -1);
        var matrix_method_i2s = matrix_attr('method-i2s', -1);

        var matrix_field_warning = function (id, message) {
            var group = $('#' + id).closest('.form-group');
            group.find('.matrix-field-warning').remove();
            // the server marks the field with an error, do not repeat the message
            if (message && !group.find('.is-invalid').length) {
                $('<div class="matrix-field-warning text-warning small"></div>').text(message).appendTo(group);
            }
        };

        var matrix_summary = function (messages) {
            var box = matrix_form.find('#matrix-layout-warning');
            // the dismiss button removes the alert, drop the wrapper with it
            if (box.length && !box.find('.alert').length) {
                box.remove();
                box = $();
            }
            if (!messages.length) {
                box.remove();
                return;
            }
            if (!box.length) {
                // pt-3 keeps the alert below the navigation bar, the accordion below adds its own pt-3
                box = $('<div class="pt-3" id="matrix-layout-warning">' +
                    '<div class="alert alert-warning alert-dismissible fade show mb-0" role="alert">' +
                    '<h4 class="alert-heading">Warning</h4><div class="matrix-warning-list"></div>' +
                    '<button type="button" class="close" data-dismiss="alert" aria-label="Close"><span aria-hidden="true">&times;</span></button></div></div>');
                matrix_form.prepend(box);
            }
            var list = box.find('.matrix-warning-list').empty();
            $(messages).each(function (key, message) {
                $('<div></div>').text(message).appendTo(list);
            });
        };

        var matrix_int = function (selector) {
            var value = parseInt($(selector).val());
            return isNaN(value) ? 0 : value;
        };

        var matrix_update = function () {
            var rows = matrix_int('#mx_rows');
            var cols = matrix_int('#mx_cols');
            var matrix_pixels = rows * cols;
            var messages = [];
            var segments = [];

            for (var i = 0; i < 4; i++) {
                if (!$('#mx_pr' + i).length) {
                    continue;
                }
                var enabled = matrix_int('#mx_en' + i) != 0;
                var panel_rows = matrix_int('#mx_pr' + i);
                var panel_cols = matrix_int('#mx_pc' + i);
                segments.push({ index: i, enabled: enabled, rows: panel_rows, cols: panel_cols, pos_row: matrix_int('#mx_prp' + i), pos_col: matrix_int('#mx_pcp' + i),
                    pixels: enabled ? panel_rows * panel_cols : 0 });
            }

            $(matrix_fields).each(function (key, selector) {
                matrix_field_warning(selector.substr(1), '');
            });

            // LED count and linear offset of every segment, the offset is the number of LEDs of the
            // enabled segments before it (the order of the output pins)
            var linear_offset = 0;
            var active_segments = 0;
            $(segments).each(function (key, segment) {
                // the panel fields of a disabled segment are hidden, their values are still submitted
                var n = segment.index;
                $('#mx_pr' + n + ',#mx_pc' + n + ',#mx_prp' + n + ',#mx_pcp' + n + ',#mx_rot' + n).closest('.form-group').toggle(segment.enabled);
                var info = 'disabled';
                if (segment.enabled) {
                    info = segment.pixels + ' LEDs (' + segment.rows + ' x ' + segment.cols + '), linear offset ' + linear_offset +
                        ' (pixels ' + linear_offset + '-' + (linear_offset + segment.pixels - 1) + ')';
                    linear_offset += segment.pixels;
                    active_segments++;
                }
                $('#mx_seg' + segment.index).val(info);
                if (!segment.enabled) {
                    return;
                }
                if (!segment.rows || !segment.cols) {
                    var message = 'Segment ' + (segment.index + 1) + ': the panel height and width must be at least 1';
                    matrix_field_warning('mx_pr' + segment.index, message);
                    messages.push(message);
                }
                else if (segment.pixels > matrix_max_pixels) {
                    var message = 'Segment ' + (segment.index + 1) + ': panel ' + segment.rows + ' x ' + segment.cols + ' = ' + segment.pixels +
                        ' LEDs exceeds the maximum number of pixels (' + matrix_max_pixels + ')';
                    matrix_field_warning('mx_pc' + segment.index, message);
                    messages.push(message);
                }
                if (rows && cols && segment.rows && segment.cols && (segment.pos_row + segment.rows > rows || segment.pos_col + segment.cols > cols)) {
                    var message = 'Segment ' + (segment.index + 1) + ': the panel ' + segment.rows + ' x ' + segment.cols + ' at row ' + segment.pos_row +
                        ', column ' + segment.pos_col + ' reaches outside the matrix (' + rows + ' x ' + cols + ')';
                    matrix_field_warning('mx_prp' + segment.index, message);
                    messages.push(message);
                }
            });

            // overlapping panels, the first pair is reported
            var panels = $.grep(segments, function (segment) {
                return segment.pixels > 0;
            });
            var intersects = function (a, b) {
                return a.pos_row < b.pos_row + b.rows && b.pos_row < a.pos_row + a.rows && a.pos_col < b.pos_col + b.cols && b.pos_col < a.pos_col + a.cols;
            };
            var overlap = null;
            for (var i = 1; i < panels.length && overlap === null; i++) {
                for (var j = 0; j < i; j++) {
                    if (intersects(panels[i], panels[j])) {
                        overlap = [panels[j], panels[i]];
                        break;
                    }
                }
            }
            if (overlap !== null) {
                var message = 'Segment ' + (overlap[1].index + 1) + ' overlaps segment ' + (overlap[0].index + 1) + ', the same matrix pixels are shown on both pins';
                matrix_field_warning('mx_prp' + overlap[1].index, message);
                messages.push(message);
            }

            if (rows && cols) {
                if (matrix_pixels > matrix_max_pixels) {
                    messages.push('Rows x columns = ' + rows + ' x ' + cols + ' = ' + matrix_pixels + ' exceeds the maximum number of pixels (' + matrix_max_pixels + ')');
                }
                else {
                    // matrix pixels that no panel covers stay dark
                    var uncovered = 0;
                    for (var row = 0; row < rows; row++) {
                        for (var col = 0; col < cols; col++) {
                            var covered = false;
                            for (var k = 0; k < panels.length && !covered; k++) {
                                var panel = panels[k];
                                covered = row >= panel.pos_row && row < panel.pos_row + panel.rows && col >= panel.pos_col && col < panel.pos_col + panel.cols;
                            }
                            if (!covered) {
                                uncovered++;
                            }
                        }
                    }
                    if (uncovered) {
                        messages.push(uncovered + ' of the ' + matrix_pixels + ' matrix pixels (' + rows + ' x ' + cols + ') are not covered by a panel and stay dark');
                    }
                }
            }

            // transport checks, mirror of matrix_validation.h (warning/info, never blocking)
            var method = matrix_int('#dm');
            var has_neobus = (matrix_method_rmt >= 0 || matrix_method_i2s >= 0);
            var i2s_selected = (matrix_method_i2s >= 0 && method === matrix_method_i2s);
            if (matrix_max_strips && active_segments > matrix_max_strips) {
                messages.push(active_segments + ' segments are enabled but the transport can drive only ' + matrix_max_strips);
            }
            if (i2s_selected && !active_segments) {
                messages.push('NeoPixelBus I2S is selected but no segment is enabled: nothing will be transmitted');
            }
            if (i2s_selected && matrix_i2s_strips > 0 && active_segments > matrix_i2s_strips) {
                messages.push(active_segments + ' segments: ' + matrix_i2s_strips + ' on I2S, ' + (active_segments - matrix_i2s_strips) +
                    ' transmitted by RMT (mixed mode' + (matrix_i2s_port < 255 ? ', the microphone reserves I2S port ' + matrix_i2s_port : '') + ')');
            }
            if (!has_neobus && active_segments > 1) {
                messages.push(active_segments + ' segments are enabled but this transport drives only one chain');
            }
            if (i2s_selected) {
                $(panels).each(function (key, segment) {
                    if (segment.pixels > (matrix_max_pixels >> 1)) {
                        messages.push('Segment ' + (segment.index + 1) + ' has ' + segment.pixels +
                            ' LEDs, its I2S DMA buffer may not fit into the DMA capable RAM and it is then transmitted by RMT');
                    }
                });
            }

            matrix_summary(messages);
        };

        // test pattern buttons: the form is saved like Save and applied right away, then the panels show
        // the test pattern ("Show All", +LMC=dir) or only the panel of one segment ("Segment n", +LMC=id).
        // The page stays open, the buttons are disabled until the device has answered
        var matrix_test_buttons = $('button[name="mx_tp"]');
        var matrix_test_status = function (text, css) {
            var group = matrix_test_buttons.closest('.form-group');
            group.find('.matrix-test-status').remove();
            if (text) {
                $('<div class="matrix-test-status small"></div>').addClass(css).text(text).appendTo(group);
            }
        };
        matrix_test_buttons.on('click', function (event) {
            event.preventDefault();
            var button = $(this);
            var segment = button.val();
            var label = button.html();
            matrix_test_buttons.prop('disabled', true);
            button.html('<span class="spinner-border spinner-border-sm" role="status"></span> Applying...');
            matrix_test_status('', '');
            $.post('/led-matrix-test.json?SID=' + $.getSessionId(), matrix_form.serialize() + '&mx_tp=' + encodeURIComponent(segment)).done(function (data) {
                if (typeof data === 'string') {
                    data = JSON.parse(data);
                }
                if (data.ok) {
                    $.formValidator.addErrors([]);
                    matrix_test_status(segment == 'all' ? 'Saved, the panels show the test pattern' : 'Saved, only the panel of segment ' + segment + ' is shown', 'text-success');
                }
                else {
                    $.formValidator.addErrors(data.errors || []);
                    matrix_test_status('Not saved, check the marked fields', 'text-danger');
                }
            }).fail(function (xhr) {
                matrix_test_status('Request failed (' + xhr.status + ' ' + xhr.statusText + ')', 'text-danger');
            }).always(function () {
                button.html(label);
                matrix_test_buttons.prop('disabled', false);
                matrix_update();
            });
        });

        $(matrix_fields.join(',')).on('input change', matrix_update);
        // the Enabled buttons write their hidden field without an event
        $('#_mx_en0,#_mx_en1,#_mx_en2,#_mx_en3').on('click', function () {
            window.setTimeout(matrix_update, 0);
        });
        matrix_update();
        // run again after the validator has marked the fields, the inline warnings are skipped then
        window.setTimeout(matrix_update, 250);
    }

});
