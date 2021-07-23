/**
 * Author: sascha_lammers@gmx.de
 */


 $(function() {
    if ($('#webui').length) {
        var animation_config = null;
        if (!animation_config) {
            // initialize
            $('body').append('<div id="animation-config" class="modal fade" tabindex="-1"></div>');
            animation_config = $('#animation-config');

            var create_modal = function() {
console.log("create_modal");
                animation_config.html(
                    '<div class="modal-dialog"><div class="modal-content"><div class="modal-header"><h5 class="modal-title">Loading...</h5><button type="button" class="close" data-dismiss="modal" aria-label="Close"><span aria-hidden="true">&times;</span></button></div><div class="modal-body"><div class="modal-loading text-center"><img src="/images/spinner.gif" class="p-3" width="120" height="120"></div>' +
                    '</div><div class="modal-footer" style="display: none;"><button type="button" class="btn btn-primary">Apply changes</button><button type="button" class="btn btn-primary">Save &amp; close</button><button type="button" class="btn btn-secondary" data-dismiss="modal">Discard</button></div></div></div>'
                );
            };

            var close_modal = function() {
                animation_config.modal('hide');
            };

            var modal_error = function(jqXHR, textStatus, error) {
                var dlg = animation_config.find('.modal-dialog');
                if (dlg.length) {
                    dlg.find('.modal-title').html('An error occured!');
                    dlg.find('.modal-body').html(error);
                    dlg.find('.modal-footer').html('<button type="button" class="btn btn-secondary" data-dismiss="modal">Close</button>').show();
                }
                else {
console.log('modal_error cannot find modal-dialog');
                }
            };

            var open_modal = function(animation) {
                var self = this;
console.log("open_modal(",animation,")",animation_config.find('.modal-dialog').length, animation.self);
                if (animation_config.find('.modal-dialog').length == 0) {
                    create_modal();
                    animation_config.modal({show: false, keyboard: false}).on('show.bs.modal', function() {
                        var url = '/led-matrix/ani-' + animation + '.html';
console.log('show', animation, url);
                        $.get(url, function(data) {
console.log('get', animation, url);
                            var dlg = animation_config.find('.modal-dialog');
                            if (dlg.length == 0 || dlg.find('.modal-loading').length == 0) {
console.log('.modal-dialog or .modal-loading not avaiable');
                                return;
                            }
                            // find form
                            var container = $(data).find('.webui-modal-dialog-form');
                            var form = container.closest('form');
                            if (container.length == 0 || form.length == 0) {
console.log('cannot locate form or .webui-modal-dialog-form');
                                modal_error(null, null, 'Cannot locate form in response');
                                return;
                            }
                            dlg.find('.modal-title').html(container.data('title'));
                            dlg.find('.modal-body').html('<form>' + form.html() + '</form>');
                            var form = dlg.find('form').on('submit', function(event) {
console.log('form submit', $(this).serialize());
                                event.preventDefault();
                                self.socket.send('+set ani-' + animation + ' ' + $(this).serialize());
                            });
                            form.append('<input type="hidden" name="__websocket_action" id="__websocket_action" value="discard">');
                            var websocket_action = $('#__websocket_action');
                            dlg.find('.modal-footer').show().find('button').first().on('click', function(event) {
                                event.preventDefault();
                                websocket_action.val('apply');
                                form.submit();
                            }).next().on('click', function(event) {
                                event.preventDefault();
                                websocket_action.val('save');
                                form.submit();
                                close_modal();
                            });

                            $.formInitFunc(dlg);
                            $.initFormLedMatrixFunc();


                        }).fail(modal_error);
                    }).on('hidden.bs.modal', function() {
console.log('hidden.bs.modal', animation);
                        var action = $('#__websocket_action');
                        if (action && action.val() == 'discard') {
console.log('sending discard');
                            self.socket.send('+set ani-' + animation + ' __websocket_action=discard');
                        }
                        animation_config.modal('dispose').html('');
                    }).modal('show');
                }
                else {
console.log("modal exists", animation);
                }
            }

            // var handler = null;
            // $.webUIComponent.event_handlers.push(function(events) {
            //     if (handler && events.i == 'animation-1') {
            //         try {
            //             // console.log(events, handler, events.i);
            //             var json = JSON.parse(events.v);
            //             // console.log(json);
            //             handler(json);
            //             return true;
            //         } catch(e) {
            //         }
            //     }
            //     return false;
            // });
            $.webUIComponent.init_functions.push(function() {

                // update link
                var self = this;
                var listbox = $('#ani');
                var listbox_title = listbox.closest('.listbox').find('.title');
                listbox_title.html('<a href="#" id="open-animation-config"><span class="oi oi-cog md-font"></span> Animation</a>');
                $('#open-animation-config').on('click', function(event) {
console.log('onclick', parseInt(listbox.val()));
                    event.preventDefault();
                    open_modal.call(self, parseInt(listbox.val()));
                });
                $('#ani').dblclick(function(event) {
console.log('dblclick', parseInt(listbox.val()));
                    event.preventDefault();
                    open_modal.call(self, parseInt(listbox.val()));
                });
            });
        }
        else {
            if ($('#animation-config').length == 0) {
                console.log("ERROR #animation-config missing");
            }
        }
    }
 });
