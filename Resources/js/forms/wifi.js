/**
 * Author: sascha_lammers@gmx.de
 */

$(function () {
    // wifi.html
    if ($('#wifi_settings').length) {

        $('#wifi_mode').on('change', function() {
            var mode = parseInt($(this).val());
            if (mode == 0) {
                $('#heading-station').closest('.card').hide();
                $('#heading-softap').closest('.card').hide();
            } else if (mode == 1) { // station
                $('#heading-station').closest('.card').show();
                $('#heading-softap').closest('.card').hide();
            } else if (mode == 2) { // soft_ap
                $('#heading-station').closest('.card').hide();
                $('#heading-softap').closest('.card').show();
            } else if (mode == 3) { // station+soft_ap
                $('#heading-station').closest('.card').show();
                $('#heading-softap').closest('.card').show();
            }
        }).trigger('change');

        $('#network_dialog').on('show.bs.modal', function () {
            var SID = $.getSessionId();
            var modal = $(this)
            var reload_timer = null;
            var selected_network = $('#wssid').val();
            var mbody = modal.find('.modal-body');
            modal.find('.btn-primary').prop('disabled', true);
            mbody.find('.networks').hide();
            mbody.find('.scanning').show();
            function scan_failed(jqXHR, textStatus, error) {
                modal.find('.btn-primary').prop('disabled', true);
                mbody.find('.scanning').hide();
                mbody.find('.networks').html("An error occured!<br>" + error).show();
            }
            function auto_reload() {
                $('#wifi_networks_title').html('<img src="images/spinner.gif" width="24" height="24" border="0">');
                check_scan();
            }
            function check_scan() {
                $.get('/scan-wifi?SID=' + SID + '&id=' + random_str(), function (data) {
                    console.log(data);
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
                        mbody.find('.scanning').hide();
                        mbody.find('.networks').html(html).show();
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
