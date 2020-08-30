#
# Author: sascha_lammers@gmx.de
#

from os import path
import sys
libs_dir = path.realpath(path.join(path.dirname(__file__), '../../libs'))
sys.path.insert(0, libs_dir)

import kfcfw.session
import kfcfw.configuration
import kfcfw.connection
import kfcfw.inline
