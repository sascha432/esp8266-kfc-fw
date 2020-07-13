#
# Author: sascha_lammers@gmx.de
#

import sys
import os
from os import path

template_file = path.join(path.dirname(path.realpath(__file__)), 'alarm_html.template')
alarm_html = path.join(path.dirname(path.realpath(__file__)), '../../Resources/html/alarm.html')
alarm_html_new = path.join(path.dirname(path.realpath(__file__)), '../../Resources/html/alarm.auto.html')

def minutes(num):
    s = ''
    for i in range(0, 60):
        s += '<option value="%u"%%A_%u_M_%u%%>%02u</option>' % (i, num, i, i)
    return s

def hours(num):
    s = ''
    for i in range(0, 24):
        s += '<option value="%u"%%A_%u_H_%u%%>%02u</option>' % (i, num, i, i)
    return s

alarm_form = ''
with open(template_file, 'rt') as file:
    contents = file.read()
    for i in range(0, 10):
        tmp = contents.replace('$NUM', str(i)).replace('$HOURS', hours(i)).replace('$MINUTES', minutes(i)).rstrip();
        alarm_form += tmp + '\n'



container = False
container_end = False
with open(alarm_html_new, 'wt') as new_html_file:
    with open(alarm_html, 'rt') as html_file:
        for line in html_file:
            if '<!--ALARM_CONTAINER_START-->' in line:
                container = True
                new_html_file.write(line)
            elif '<!--ALARM_CONTAINER_END-->' in line:
                new_html_file.write(alarm_form)
                container = False
                container_end = True
            if not container:
                new_html_file.write(line)

if not container_end:
    os.unlink(alarm_html_new)
    print("ERROR: container end marker not found")
    sys.exit(-1)

print("Created: %s" % alarm_html_new)

