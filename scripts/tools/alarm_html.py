#
# Author: sascha_lammers@gmx.de
#

import sys
import os
from os import path

template_file = path.join(path.dirname(path.realpath(__file__)), 'alarm_html.template')

def minutes(num):
    s = ''
    for i in range(0, 60):
        s += '<option value="%u"%%ALARM_%u_MINUTE_%u%%>%02u</option>' % (i, num, i, i)
    return s

def hours(num):
    s = ''
    for i in range(0, 24):
        s += '<option value="%u"%%ALARM_%u_HOUR_%u%%>%02u</option>' % (i, num, i, i)
    return s

with open(template_file, 'rt') as file:
    contents = file.read()
    for i in range(0, 10):
        tmp = contents.replace('$NUM', str(i)).replace('$HOURS', hours(i)).replace('$MINUTES', minutes(i)).rstrip();
        print(tmp)
