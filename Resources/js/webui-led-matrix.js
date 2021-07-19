/**
 * Author: sascha_lammers@gmx.de
 */

if ($('#webui').length && $('#ani').length) {
    var on_change_animation = function() {
        if ($(this).val() == 1) {
            $('#ani').closest('.listbox').find('.title').html('<a href="#" data-toggle="modal" data-target="#animation-config-rainbow"><span class="oi oi-cog md-font"></span> Animation</a>');
        }
        else {
            $('#ani').closest('.listbox').find('.title').html('Animation')
        }
    };
    $('#ani').on('change', on_change_animation);
    on_change_animation();
}
