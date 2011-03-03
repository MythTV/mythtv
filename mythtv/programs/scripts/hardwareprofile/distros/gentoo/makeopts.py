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

import re
import portage

SHORT_PARA_PATTERN = '-[CfIOW]\\s+\\S+|-[jl](\\s+[^-]\\S*)?|-[^-]\\S+'
LONG_PARA_PATTERN = '--\\S+|--\\S+=\\S+'
PARA_PATTERN = re.compile('(%s|%s)\\b' % (SHORT_PARA_PATTERN, LONG_PARA_PATTERN))

class MakeOpts:
    def __init__(self):
        self._makeopts = self._parse(portage.settings['MAKEOPTS'])

    def _parse(self, flags):
        list = []
        for m in re.finditer(PARA_PATTERN, flags):
            text = re.sub('\\s{2,}', ' ', m.group()) # Normalize whitespace
            list.append(text)
        return list

    def get(self):
        return self._makeopts

    def serialize(self):
        return self._makeopts

    def dump(self):
        print 'MAKEOPTS: ' + str(self.get())
        print

if __name__ == '__main__':
    MakeOpts = MakeOpts()
    MakeOpts.dump()

"""
Samples
-C dir -f file -I dir -o file -W file -j 3 -l 4 -j -l --always-make
"""
