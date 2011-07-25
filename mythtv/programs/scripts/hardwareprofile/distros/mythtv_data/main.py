# smolt - Fedora hardware profiler
#
# Copyright (C) 2009 Sebastian Pipping <sebastian@pipping.org>
# Copyright (C) 2011 James Meyer <james.meyer@operamail.com>
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
from distros.distro import Distro
import distros.shared.html as html



class _Mythtv(Distro):
    def key(self):
        return 'mythtv'

    def detected(self, debug=False):
        
        #return False
        return True
        

    def gather(self, gate, debug=False):
        def _stage(text):
            print 'Processing %s' % (text)
        from data_mythtv import create_mythtv_data

        _stage('MythTV Data')
        features = create_mythtv_data(gate)

        machine_data = {}
        html_lines = []
        rst_lines = []
        metrics_dict = {}

        rst_lines.append('MythTV data')
        rst_lines.append('=================================')
        machine_data['protocol'] = '1.2'

        
        machine_data['features'] = features.serialize()
        features.dump_rst(rst_lines)
        rst_lines.append('')

        excerpt_lines = []
        excerpt_lines.append('...')

        self._data = machine_data
        self._html = '\n'.join(html_lines)
        self._rst = '\n'.join(rst_lines)
        self._excerpt = '\n'.join(excerpt_lines)

    def data(self):
        return self._data

    def html(self):
        return self._html

    def rst(self):
        return self._rst

    def rst_excerpt(self):
        return self._excerpt


def create_mythtv():
    return _Mythtv()


if __name__ == '__main__':
    # Enable auto-flushing for stdout
    import sys
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)

    from gate import create_passing_gate
    mythtv = create_mythtv()
    mythtv.gather(create_passing_gate(), debug=True)

    print mythtv.rst()
