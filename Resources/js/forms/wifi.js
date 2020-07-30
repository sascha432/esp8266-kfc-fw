/**
 * Author: sascha_lammers@gmx.de
 */

$(function () {
    // wifi.html
    if ($('#wifi_settings').length) {
        var max_channels = parseInt($('#apch').data('max-channels'));
        $('#channel option').each(function () {
            if (parseInt($(this).val()) > max_channels) {
                $(this).remove();
            }
        });
        function mode_change() {
            var mode = parseInt($('#wssid').val());
            $('#station_mode').hide();
            $('#ap_mode').hide();
            if (mode == 1 || mode == 3) {
                $('#station_mode').show();
            }
            if (mode == 2 || mode == 3) {
                $('#ap_mode').show();
            }
        }
        $('#wssid').change(mode_change);
        mode_change();
        $('form').on('submit', function () {
            $('#aphs').val($('#_aphs').prop('checked') ? '1' : '0');
        });
        $('#network_dialog').on('show.bs.modal', function (event) {
            console.log("show.bs.modal");
            var SID = $.getSessionId();
            var modal = $(this)
            var reload_timer = null;
            var selected_network = $('#wssid').val();
            modal.find('.btn-primary').prop('disabled', true);
            modal.find('.modal-body .networks').hide();
            modal.find('.modal-body .scanning').show();
            function scan_failed(jqXHR, textStatus, error) {
                modal.find('.btn-primary').prop('disabled', true);
                modal.find('.modal-body .scanning').hide();
                modal.find('.modal-body .networks').html("An error occured!<br>" + error).show();
            }
            function auto_reload() {
                $('#wifi_networks_title').html('<img src="images/spinner.gif" width="24" height="24" border="0">');
                check_scan();
            }
            function check_scan() {
                $.get('/scan_wifi?SID=' + SID + '&id=' + random_str(), function (data) {
                    dbg_console.log(data);
                    if (data.pending) {
                        window.setTimeout(check_scan, 1000);
                    } else {
                        var header = '<table class=\"table table-striped\"><thead class=\"thead-light\"><tr><th>SSID</th><th>Channel</th><th>Signal</th><th>MAC</th><th>Encryption</th></tr></thead><tbody>';
                        var footer = '</tbody></table>';
                        $('#wifi_networks_title').html('<a href="#" id="reload_link"><span class="oi oi-reload" title="Reload" aria-hidden="true"></span></a>');
                        $('#reload_link').on("click", function (e) {
                            e.preventDefault();
                            if (reload_timer != null) {
                                window.clearTimeout(reload_timer);
                                reload_timer = null;
                            }
                            modal.find('tr').removeClass("bg-primary").find('td').removeClass('text-white');
                            modal.find('.btn-primary').prop('disabled', true);
                            auto_reload();
                        });
                        var html = header;
                        if (data.msg) {
                            html += '<tr scope="row"><td colspan="99" class="text-center">' + data.msg + '</td></tr>';
                        } else {
                            data.result.sort(function (a, b) {
                                return b.rssi - a.rssi;
                            });
                            for (var i = 0; i < data.result.length; i++) {
                                var r = data.result[i];
                                html += '<tr scope="row" class="' + r.tr_class + '">';
                                if (r.td_class) {
                                    html += '<td class="' + r.td_class + '">';
                                } else {
                                    html += '<td>';
                                }
                                html += r.ssid;
                                html += "</td><td>";
                                html += r.channel;
                                html += "</td><td>";
                                html += r.rssi;
                                html += "</td><td>";
                                html += r.bssid;
                                html += "</td><td>";
                                html += r.encryption;
                                html += "</td></tr>";
                            }
                        }
                        html += footer;
                        modal.find('.modal-body .scanning').hide();
                        modal.find('.modal-body .networks').html(html).show();
                        modal.find('.network-name').each(function () {
                            if (selected_network != '' && selected_network == $(this).html()) {
                                $(this).closest('tr').addClass('bg-primary').find('td').addClass('text-white');
                            }
                        });
                        modal.find('.has-network-name').on("click", function (e) {
                            e.preventDefault();
                            if (reload_timer != null) {
                                window.clearTimeout(reload_timer);
                                reload_timer = null;
                            }
                            modal.find('tr').removeClass("bg-primary").find('td').removeClass('text-white');
                            selected_network = $(this).find('.network-name').html();
                            $(this).addClass('bg-primary').find('td').addClass('text-white');
                            modal.find('.btn-primary').prop('disabled', false).off("click").on("click", function (e) {
                                e.preventDefault();
                                modal.off('hidden.bs.modal').on('hidden.bs.modal', function () {
                                    window.clearTimeout(reload_timer);
                                    reload_timer = null;
                                    if (selected_network) {
                                        $('#wssid').val(selected_network);
                                        $('#wpwd').focus().select();
                                    }
                                    selected_network = $('#wssid').val();
                                });
                                modal.modal('hide');
                            });
                        });
                        reload_timer = window.setTimeout(auto_reload, 30000);
                    }
                }, 'json').fail(scan_failed);
            }
            check_scan();
        });
    }
});
