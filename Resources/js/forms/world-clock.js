/**
 * Author: sascha_lammers@gmx.de
 */

$(function() {
    // world-clock.html
    if ($('#world_clock').length) {
        // remove timezone if the name is blank
        $('input[name^=nm_]').on('blur', function() {
            if ($(this).val() == '') {
                var num = $(this).attr('id').substring(3);
                $('#tz_' + num).val('');
                $('#tn_' + num).val('');
            }
        });
        // select the timezone from the value and text
        $('select[name^=tz_]').each(function() {
            var $this = $(this);
            var id = $this.attr('id');
            var nameId = '#tn_' + id.substring(3);
            $this.html(window.posix_timezone_list_options);
            var nameStr = $(nameId).val();
            // select correct timezone
            $this.find("option").filter(function() {
                return $(this).text() == nameStr;
            }).prop('selected', true);
            // update timezone name
            $this.on('change', function() {
                var text = $this.find('option:selected').text();
                $(nameId).val(text);
            });
        });
    }
});
