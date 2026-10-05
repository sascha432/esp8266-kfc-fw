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

    // led-matrix/matrix.html: warn while the matrix settings are edited. The server rejects values
    // that would read outside the pixel buffer or drive the same pixels twice (hard errors), these
    // are hints only - gaps are allowed and the segments do not have to cover rows * columns
    if ($('#mx_px0').length && $('#mx_px').length) {
        var matrix_max_pixels = parseInt($('#mx_px').val()) || 0;
        var matrix_form = $('#mx_px0').closest('form');
        var matrix_fields = ['#dm', '#mx_rows', '#mx_cols', '#mx_px0', '#mx_px1', '#mx_px2', '#mx_px3', '#mx_ofs0', '#mx_ofs1', '#mx_ofs2', '#mx_ofs3'];

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
                if ($('#mx_px' + i).length) {
                    segments.push({ index: i, offset: matrix_int('#mx_ofs' + i), pixels: matrix_int('#mx_px' + i) });
                }
            }

            $(matrix_fields).each(function (key, selector) {
                matrix_field_warning(selector.substr(1), '');
            });

            var segment_pixels = 0;
            var active_segments = 0;
            $(segments).each(function (key, segment) {
                segment_pixels += segment.pixels;
                if (segment.pixels) {
                    active_segments++;
                }
                if (segment.pixels && (segment.offset + segment.pixels) > matrix_max_pixels) {
                    var message = 'Segment ' + (segment.index + 1) + ': offset ' + segment.offset + ' + ' + segment.pixels + ' pixels = ' +
                        (segment.offset + segment.pixels) + ' exceeds the maximum number of pixels (' + matrix_max_pixels + ')';
                    matrix_field_warning('mx_px' + segment.index, message);
                    messages.push(message);
                }
            });

            // overlapping segments, the first pair is reported
            var overlap = null;
            for (var i = 1; i < segments.length && overlap === null; i++) {
                if (!segments[i].pixels) {
                    continue;
                }
                for (var j = 0; j < i; j++) {
                    if (!segments[j].pixels) {
                        continue;
                    }
                    if (segments[i].offset < (segments[j].offset + segments[j].pixels) && segments[j].offset < (segments[i].offset + segments[i].pixels)) {
                        overlap = [segments[j], segments[i]];
                        break;
                    }
                }
            }
            if (overlap !== null) {
                var message = 'Segment ' + (overlap[1].index + 1) + ' (pixels ' + overlap[1].offset + '..' + (overlap[1].offset + overlap[1].pixels - 1) +
                    ') overlaps segment ' + (overlap[0].index + 1) + ' (pixels ' + overlap[0].offset + '..' + (overlap[0].offset + overlap[0].pixels - 1) + ')';
                matrix_field_warning('mx_ofs' + overlap[1].index, message);
                messages.push(message);
            }

            if (rows && cols) {
                if (matrix_pixels > matrix_max_pixels) {
                    messages.push('Rows x columns = ' + rows + ' x ' + cols + ' = ' + matrix_pixels + ' exceeds the maximum number of pixels (' + matrix_max_pixels + ')');
                }
                if (segment_pixels !== matrix_pixels) {
                    if (segment_pixels < matrix_pixels) {
                        messages.push('Rows x columns = ' + rows + ' x ' + cols + ' = ' + matrix_pixels + ', but the segments configure ' + segment_pixels + ' pixel(s): ' + (matrix_pixels - segment_pixels) + ' pixel(s) are not covered');
                    }
                    else {
                        messages.push('Rows x columns = ' + rows + ' x ' + cols + ' = ' + matrix_pixels + ', but the segments configure ' + segment_pixels + ' pixel(s): ' + (segment_pixels - matrix_pixels) + ' pixel(s) are unused');
                    }
                }
            }

            // transport checks, mirror of matrix_validation.h (warning/info, never blocking)
            var method = matrix_int('#dm');
            var has_neobus = (matrix_method_rmt >= 0 || matrix_method_i2s >= 0);
            var i2s_selected = (matrix_method_i2s >= 0 && method === matrix_method_i2s);
            if (matrix_max_strips && active_segments > matrix_max_strips) {
                messages.push(active_segments + ' segments are configured but the transport can drive only ' + matrix_max_strips);
            }
            if (i2s_selected && !active_segments) {
                messages.push('NeoPixelBus I2S is selected but no segment is configured: nothing will be transmitted');
            }
            if (i2s_selected && matrix_i2s_strips > 0 && active_segments > matrix_i2s_strips) {
                messages.push(active_segments + ' segments: ' + matrix_i2s_strips + ' on I2S, ' + (active_segments - matrix_i2s_strips) +
                    ' transmitted by RMT (mixed mode' + (matrix_i2s_port < 255 ? ', the microphone reserves I2S port ' + matrix_i2s_port : '') + ')');
            }
            if (!has_neobus && active_segments > 1) {
                messages.push(active_segments + ' segments are configured but this transport drives only one chain');
            }
            if (i2s_selected) {
                $(segments).each(function (key, segment) {
                    if (segment.pixels > (matrix_max_pixels >> 1)) {
                        messages.push('Segment ' + (segment.index + 1) + ' has ' + segment.pixels +
                            ' pixels, its I2S DMA buffer may not fit into the DMA capable RAM and it is then transmitted by RMT');
                    }
                });
            }

            matrix_summary(messages);
        };

        $(matrix_fields.join(',')).on('input change', matrix_update);
        matrix_update();
        // run again after the validator has marked the fields, the inline warnings are skipped then
        window.setTimeout(matrix_update, 250);
    }

});
