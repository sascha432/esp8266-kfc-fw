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

});
