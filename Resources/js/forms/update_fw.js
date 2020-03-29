/**
 * Author: sascha_lammers@gmx.de
 */

 // update_fw.html
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

    window.onblur = onBlur;
    window.onfocus = onFocus;
}
