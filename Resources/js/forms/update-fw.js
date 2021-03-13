/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // update-fw.html
    if ($('#firmware_update').length && $('#firmware_image').length) {
        var firmware_image = $('#firmware_image');

        var onFocus = function () {
            firmware_image.removeClass('overlay').removeClass('input').off('click');
            $('#firmware_image_overlay').fadeOut(250);
        };
        var onBlur = function () {
            firmware_image = $('#firmware_image');
            firmware_image.addClass('overlay');
            firmware_image.addClass('input');
            var ctop = $('.container').position().top;
            firmware_image.css('top', ctop);
            $('#firmware_image_overlay').css('top', ctop).fadeIn(250);
            firmware_image.on('click', function (event) {
                event.preventDefault();
                onFocus();
            });
        };

        function add_focus_handler() {
            $(window).on('focus', onFocus);
            $(window).on('blur', onBlur);
        }

        function show_progress() {
            $('#firmware_upgrade_progress').removeClass('hidden');
            $('#firmware_upgrade_form').addClass('hidden');
            $('#firmware_upgrade_status').html('').addClass('hidden');
            onFocus();
            $(window).off('focus');
            $(window).off('blur');
        }

        function format_message(message) {
            return '<div class="card"><div class="card-header text-white bg-danger"><h2 class="mb-0">Firmware Upgrade Failed</h2></div><div class="card-body"><h4 class="p-3 m-3">' + message + '</h4></div></div>';
       }

        function show_status(data) {
            if ($(data).find('.accordion').length) {
                message = $(data).find('.accordion').html();
            } else {
                message = format_message(data);
            }
            $('#firmware_upgrade_form').removeClass('hidden');
            $('#firmware_upgrade_progress').addClass('hidden');
            $('#firmware_upgrade_status').html(message).removeClass('hidden');
            onFocus();
            add_focus_handler();
        }

        function update_progress(n) {
            $('#upload-progress-bar').css('width', parseInt(n) + '%');
        }

        add_focus_handler();

        $('#firmware_update').off().on('submit', function(e) {
            e.preventDefault();
            show_progress();
            var data = new FormData();
            data.append('SID', $.getSessionId());
            $(this).find('input').each(function() {
                if ($(this)[0].files) {
                    data.append($(this).attr('name'), $(this)[0].files[0]);
                }
                else {
                    data.append($(this).attr('name'), $(this).val());
                }
            });
            $.ajax({
                xhr: function () {
                    var xhr = $.ajaxSettings.xhr();
                    if (xhr.upload) {
                        xhr.upload.addEventListener("progress", function (event) {
                            var percent = Math.ceil((event.loaded / event.total) * 100);
                            update_progress(Math.min(100.0, percent));
                        }, false);
                    }
                    return xhr;
                },
                url: '/update',
                data: data,
                cache: false,
                contentType: false,
                processData: false,
                type: 'POST',
            }).done(function (data) {
                show_status(data);
            }).fail(function (jqXHR, textStatus) {
                show_status('The upload was aborted due to an error');
            });
        });

    }

    if ($('#firmware_upgrade_status').length) {
        var hash = $(location).attr('hash');
        if (hash == '#u_flash') {
            $('#firmware_upgrade_status').show();
        } else if (hash == '#u_spiffs') {
            $('#firmware_upgrade_status').show().find('h1 span').html('SPI Flash File System');
        }
        function update_action() {
            $('#firmware_update').attr('action', '/update?image_type=' + $('#image_type').val());
        }
        $('#image_type').on('change', update_action);
    }

});
