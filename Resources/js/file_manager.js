/**
 * Author: sascha_lammers@gmx.de
 */

 $(function() {

    if ($('#file_manager').length) {
        var currentDirectory = "/";
        var url = $.getHttpLocation("/file_manager/");
        var defaultParams = '?SID=' + $.getSessionId();

        function replace_vars(prototype, attrib, link, fullname, name, modified, size) {
            var _class;
            if (size === undefined) {
                link = '#' + decodeURI(link);
                _class = "dir";
            } else {
                link = url + "download?filename=" + link;
                _class = "file";
            }
            if (attrib == 2) {
                _class += " text-danger";
            } else if (attrib == 1) {
                _class += " text-secondary";
            }
            var html = prototype.
                replace(new RegExp("\\${CLASS}", "g"), _class).
                replace(new RegExp("\\${LINK}", "g"), link).
                replace(new RegExp("\\${FULLNAME}", "g"), fullname).
                replace(new RegExp("\\${FILENAME}", "g"), name).
                replace(new RegExp("\\${MODIFIED}", "g"), modified);
            if (size !== undefined) {
                return html.replace(new RegExp("\\${SIZE}", "g"), size);
            }
            if (name == "." || name == "..") {
                html = '<tr scope="row"><td>&nbsp;' + html.substring(html.indexOf('</td>'));
            }
            return html;
        }

        function split_dir(dir) {
            var parts = dir.split(/\/([^\/]+)/).filter(function(element, index) {
                return !(element == '' | (element == '/' && index > 0));
            });
            if (parts[0] != '/') {
                parts.unshift('/');
            }
            return parts;
        }

        function refresh_files(dir) {

            if (dir !== undefined) {
                currentDirectory = dir;
                if (currentDirectory === "") {
                    currentDirectory = "/";
                }
            }
            console.log(currentDirectory);

            $('#files').hide();
            $('#spinner').show();
            $.get(url + "list" + defaultParams + "&dir=" + currentDirectory, function (data) {
                var dirs_prototype = $("#dirs_prototype").html();
                var files_prototype = $("#files_prototype").html();
                var dirs_ro_prototype = $("#dirs_ro_prototype").html();
                var files_ro_prototype = $("#files_ro_prototype").html();
                var html = '<li><a href="#" class="dir refresh-files breadcrumb-brand"><span class="oi oi-reload" title="Reload" aria-hidden="true"></span></a></li><span>&nbsp;&nbsp;&nbsp;&nbsp;</span>';
                $('#total_size').html(data.total);
                $('#total_size').attr('title', data.total_b);
                $('#used_space').html(data.used + " (" + data.usage + ")");
                $('#used_space').attr('title', data.used_b);
                console.log(currentDirectory);
                currentDirectory = data.dir;
                var parts = split_dir(data.dir);
                var dir = '';
                for (var i = 0; i < parts.length; i++) {
                    if (i > 1) {
                        dir += '/';
                    }
                    dir += parts[i];
                    display = i ? parts[i] :'<span class="oi oi-home" title="/" aria-hidden="true"></span>';
                    if (i == parts.length - 1) {
                        html += '<li class="breadcrumb-item active" aria-current="page"">' + display + '</li>';
                    } else {
                        html += '<li class="breadcrumb-item"><a href="#' + dir + '" class="dir">' + display + '</a></li>';
                    }
                }
                $('nav .breadcrumb').html(html);

                html = replace_vars(dirs_prototype, 3, encodeURI(currentDirectory), "", ".", "N/A");
                var pos = currentDirectory.replace(/\/+$/g, '').lastIndexOf('/');
                if (pos !== -1) {
                    var dir = currentDirectory.substring(0, pos);
                    if (dir === "") {
                        dir = "/";
                    }
                    html += replace_vars(dirs_prototype, 0, encodeURI(dir), "", "..", "N/A");
                }
                console.log(currentDirectory);

                data.files.sort(function(a, b) {
                    if (a.f < b.f) {
                        return -1;
                    } else if (a.f > b.f) {
                        return 1;
                    }
                    return 0;
                });

                var files_html = '';
                for (var i = 0; i < data.files.length; i++) {
                    var t = data.files[i];
                    if (data.files[i].d) {
                        html += replace_vars(t.m ? dirs_ro_prototype : dirs_prototype, t.m, t.f, t.f, t.n, t.t ? t.t : "N/A")
                    } else {
                        files_html += replace_vars(t.m ? files_ro_prototype : files_prototype, t.m, t.f, t.f, t.n, t.t ? t.t : "N/A", t.s)
                    }
                }
                html += files_html;
                $('#spinner').hide();
                $('#files').html(html).show();
                $('.refresh-files').attr('href', '#' + currentDirectory);

                $('a.dir').off('click').on('click', function (e) {
                    refresh_files(decodeURI(e.currentTarget.hash.substring(1)));
                });

            }).fail(function (jqXHR, textStatus, error) {
                pop_error('danger', 'ERROR!', error, null, true);
            });
        }

        function add_feedback(dialog, message) {
            pop_error_clear($(dialog));
            var selector = $(dialog + ' .modal-body').find('input[type=text]');
            if (selector.length) {
                form_invalid_feedback(selector, message);
            } else {
                pop_error('danger', 'ERROR!', message, $(dialog + ' .modal-body'));
            }
            $(dialog).find('button').prop('disabled', false);
        }

        function ajax_request(url, dialog) {
            $.get(url, function (data) {
                if (data.substring(0, 6) === "ERROR:") {
                    add_feedback(dialog, data.substring(6));
                } else {
                    $(dialog).modal('hide');
                    refresh_files();
                }
            }).fail(function (jqXHR, textStatus, error) {
                add_feedback(dialog, error)
            });
        }

        function cleanup_dialog(dialog) {
            pop_error_clear(dialog);
            dialog.find('.is-invalid').removeClass('is-invalid');
            dialog.find('.invalid-feedback').remove();
            dialog.find('button').prop('disabled', false);
        }

        function mkdir(dir) {
            ajax_request(url + "mkdir" + defaultParams + "&dir=" + currentDirectory + "&new_dir=" + dir, '#mkdir_dialog');
        }

        function remove(filename, type) {
            ajax_request(url + "remove" + defaultParams + "&dir=" + currentDirectory + "&filename=" + filename + "&type=" + type, '#remove_dialog');
        }

        function rename(old_name, new_name, type) {
            ajax_request(url + "rename" + defaultParams + "&dir=" + currentDirectory + "&filename=" + old_name + "&to=" + encodeURI(new_name) + "&type=" + type, '#rename_dialog');
        }

        $('#mkdir_dialog').on("show.bs.modal", function (e) {
            cleanup_dialog($('#mkdir_dialog'));
            $('#new_dir').val('');
        }).on('shown.bs.modal', function (e) {
            $('#new_dir').focus();
        });

        $('#upload_dialog').on("show.bs.modal", function (e) {
            cleanup_dialog($('#upload_dialog'));
            $('#upload_filename').val('').attr('placeholder', '');
            $('#upload_file').val('');
            $('#upload_status').hide();
            $('#upload_form').show();
            $('#upload_form').off('submit').on('submit', function(e) {
                $('#upload_current_dir').val(currentDirectory);
                if ($('#upload_filename').val() == "") {
                    $('#upload_filename').val($('#upload_filename').attr('placeholder'));
                }
                $('#upload_dialog').find('button').prop('disabled', true);
                try {
                    var formData = new FormData();
                    formData.append("upload_filename", $('#upload_filename').val());
                    formData.append("upload_file",  $('#upload_file')[0].files[0]);
                    formData.append("ajax_upload", "1");
                    formData.append("upload_current_dir", currentDirectory);

                    var request = new XMLHttpRequest();
                    request.open("POST", $.getHttpLocation('/file_manager/upload') + defaultParams + '&id=' + random_str(), true);

                    var upload_status = $('#upload_status');
                    upload_status.show().html($.__prototypes.filemanager_upload_progress);

                    $('#upload_form').hide();
                    request.onload = function(e) {
                        var response = request.responseText ? request.responseText : request.response;
                        if (response.substring(0, 6) == "ERROR:") {
                            response = response.substring(6);
                            request.status = 0;
                        }
                        if (request.status == 1) {
                            refresh_files(currentDirectory);
                            upload_status.html('<p class="text-center"><button type="button" class="btn btn-primary" data-dismiss="modal">Upload complete</button></p>');
                        } else {
                            $('#upload_form').show();
                            upload_status.hide();
                            pop_error('danger', 'ERROR!', response, $('#upload_dialog .modal-body'), true);
                        }
                        $('#upload_dialog').find('button').prop('disabled', false);
                    };
                    var fake_position = 0;
                    function start_fake_progress() {
                        if (fake_position == 0) {
                            fake_position = 1;
                            var next_step = 20;
                            function fake_progress() {
                                fake_position += next_step;
                                if (next_step > 1) {
                                    next_step -= 0.01;
                                }
                                if (fake_position > 10000) {
                                    fake_position = 1;
                                    next_step = 20;
                                }
                                $('#upload_progress').attr('aria-valuenow', Math.ceil(fake_position)).css('width', Math.ceil(fake_position / 10000 * 100) + '%');
                                window.setTimeout(fake_progress, 250);
                            }
                            window.setTimeout(fake_progress, 250);
                        }
                    }
                    request.upload.onprogress = function(e) {
                        var position = e.loaded || e.position;
                        if (e.lengthComputable) {
                            var percent = Math.ceil(position / e.total * 1000);
                            $('#upload_percent').html((percent / 10) + "%");
                            $('#upload_progress').attr('aria-valuemax', e.total).attr('aria-valuenow', position);
                        } else {
                            start_fake_progress();
                        }
                    };
                    request.send(formData);

                    e.preventDefault();

                } catch(exception) {
                    // ajax upload might not be supposed, just submit the form
                }
            });
        }).on('shown.bs.modal', function (e) {
            $('#upload_filename').focus();
        });

        $('#upload_file').on('change', function (e) {
            var filename = $(this).val();
            var pos = filename.replace(/\\/g, '/').lastIndexOf('/');
            if (pos != -1) {
                filename = filename.substring(pos + 1);
            }
            filename = filename.substring(0, 31 - currentDirectory.length - 1);
            $('#upload_filename').attr('placeholder', filename).focus();
        });

        $(window).on('popstate', function (e) {
            e.preventDefault();
            refresh_files(window.location.hash.substring(1));
        });

        $('#remove_dialog').on("show.bs.modal", function (e) {
            var target = $(e.relatedTarget);
            cleanup_dialog($('#remove_dialog'));
            $('#remove_dialog').find('p').html("Delete <strong>" + target.data("name") + "</strong>");
            $("#remove_name").val(target.data('filename'));
            $("#remove_type").val(target.data('type'));
        }).on('shown.bs.modal', function (e) {
            $("#remove_dialog .btn-primary").focus();
        });

        $('#rename_dialog').on("show.bs.modal", function (e) {
            var target = $(e.relatedTarget);
            cleanup_dialog($('#rename_dialog'));
            $("#old_name").val(target.data('filename'));
            $("#new_name").val(target.data('name'));
            $("#rename_type").val(target.data('type'));
        }).on('shown.bs.modal', function (e) {
            $("#new_name").focus();
        });

        $('#mkdir_dialog .btn-primary').on("click", function () {
            $('#mkdir_dialog').find('button').prop('disabled', true);
            mkdir($('#new_dir').val());
        });

        $('#remove_dialog .btn-primary').on("click", function () {
            $('#remove_dialog').find('button').prop('disabled', true);
            remove($('#remove_name').val(), $('#remove_type').val());
        });

        $('#rename_dialog .btn-primary').on("click", function () {
            $('#rename_dialog').find('button').prop('disabled', true);
            rename($('#old_name').val(), $('#new_name').val(), $('#rename_type').val());
        });

        refresh_files(window.location.hash.substring(1));
    }

});
