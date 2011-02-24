# smolt - Fedora hardware profiler
#
# Copyright (C) 2007 Mike McGrath
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.

import os
import commands
import re
import sys
import smolt_config
from gate import Gate

def read_lsb_release():
    if os.access('/usr/bin/lsb_release', os.X_OK):
        return commands.getstatusoutput('/usr/bin/lsb_release')[1].strip()
    return ''

initdefault_re = re.compile(r':(\d+):initdefault:')

def read_runlevel():
    defaultRunlevel = 'Unknown'
    try:
        inittab = file('/etc/inittab').read()
        match = initdefault_re.search(inittab)
        if match:
            defaultRunlevel = match.group(1)
    except IOError:
	try:
		defaultRunlevel = commands.getstatusoutput('/sbin/runlevel')[1].split()[1].strip()
	except:
        	sys.stderr.write(_('Cannot Determine Runlevel'))
    return defaultRunlevel.strip()

def read_os():
    return smolt_config.get_config_attr("OS", "Calvin and Hobbes")

if __name__ == '__main__':
    dict = {
        'LSB release':read_lsb_release(),
        'Run level':read_runlevel(),
        'OS':read_os(),
    }
    for k, v in dict.items():
        print '%s: "%s"' % (k, v)
