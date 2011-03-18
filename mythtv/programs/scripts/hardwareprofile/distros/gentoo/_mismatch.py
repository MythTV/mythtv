# smolt - Fedora hardware profiler
#
# Copyright (C) 2009 Sebastian Pipping <sebastian@pipping.org>
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

from tools.syncfile import SyncFile
from tools.overlayparser import OverlayParser
import ConfigParser
import os
import sys

layman_config = ConfigParser.ConfigParser()
layman_config.read('/etc/layman/layman.cfg')
layman_storage_path = layman_config.get('MAIN', 'storage')
if not layman_storage_path.endswith('/'):
    layman_storage_path = layman_storage_path + '/'

sync_file = SyncFile(
        'http://www.gentoo.org/proj/en/overlays/layman-global.txt',
        'layman-global.txt')
parser = OverlayParser()
file = open(sync_file.path(), 'r')
parser.parse(file.read())
file.close()

legend_printed = False
for i in parser.get().keys():
    layman_global_name = i
    overlay_folder = os.path.join(layman_storage_path, layman_global_name)
    if not os.path.exists(overlay_folder):
        sys.stderr.write('ERROR: Overlay "%s" not found\n' % layman_global_name)
        continue
    filename = os.path.join(overlay_folder, 'profiles', 'repo_name')
    if not os.path.exists(filename):
        sys.stderr.write('ERROR: Overlay "%s" lacks repo_name entry\n' % \
            layman_global_name)
        continue
    try:
        file = open(filename, 'r')
        repo_name = file.readline().rstrip('\n\r')
        file.close()
    except IOError:
        continue
    if layman_global_name != repo_name:
        if not legend_printed:
            sys.stderr.write('Format:\n')
            sys.stderr.write('    "<repo_name>",  # <layman-global.txt>\n')
            sys.stderr.write('\n')
            sys.stderr.write('Entries:\n')
            legend_printed = True
        print '    "%s",  # %s' % \
            (repo_name, layman_global_name)
