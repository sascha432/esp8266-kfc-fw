/**
 * Author: sascha_lammers@gmx.de
 */

 $(function() {
    if ($('#webui').length) {
        var handler = null;
        $.webUIComponent.event_handlers.push(function(events) {
            if (handler && events.i == 'animation-1') {
                try {
                    console.log(events, handler, events.i);
                    var json = JSON.parse(events.v);
                    console.log(json);
                    handler(json);
                    return true;
                } catch(e) {
                }
            }
            return false;
        });
        $.webUIComponent.init_functions.push(function() {
            var animation_config = -1;
            var self = this;
            var on_change_animation = function() {
                var value = parseInt($(this).val());
                console.log(this, animation_config, value);
                if (value == animation_config) {
                    return;
                }
                if (value == 1) {
                    $('#ani').closest('.listbox').find('.title').html('<a href="#" data-toggle="modal" data-target="#animation-config"><span class="oi oi-cog md-font"></span> Animation</a>');
                    $.get('http://192.168.0.61:8001/html/lmani/animation-1.html', function(data) {
                        var dlg = $('#animation-config');
                        dlg.html(data);
                        dlg.modal({show: false}).on('show.bs.modal', function() {
                            dlg.find('.modal-loading').show();
                            dlg.find('.modal-inner-content').hide();
                            var buttons = dlg.find('.btn-primary');
                            console.log(buttons);
                            buttons.hide();
                            $(buttons.get(0)).on('click', function() {

                            });
                            $(buttons.get(1)).on('click', function() {
                                dlg.modal('hide');
                            });
                            handler = function(values) {
                                handler = null;
                                $('#rb_mode').val(values.mode);
                                $('#rb_bpm').val(values.bpm);
                                $('#rb_hue').val(values.hue);
                                $('#rb_mul').val(values.mul[0]);
                                $('#rb_incr').val(values.mul[1]);
                                $('#rb_min').val(values.mul[2]);
                                $('#rb_max').val(values.mul[3]);
                                $('#rb_sp').val(values.speed);
                                $('#rb_cf').val(values.cf);
                                $('#rb_mv').val(values.cm);
                                $('#rb_cre').val(values.ci[0]);
                                $('#rb_cgr').val(values.ci[1]);
                                $('#rb_cbl').val(values.ci[2]);
                                dlg.find('.modal-loading').hide();
                                dlg.find('.modal-inner-content').show();
                                buttons.show();
                            };
                            self.socket.send('+get animation-1');
                        });
                        animation_config = 1;
                    }, 'html');
                }
                else {
                    $('#ani').closest('.listbox').find('.title').html('Animation')
                    animation_config = -1
                }
            };
            $('body').append('<div id="animation-config" class="modal fade" tabindex="-1"></div>');
            $('#ani').on('change', on_change_animation);
            on_change_animation.call($('#ani'));
        });
    }
 });
