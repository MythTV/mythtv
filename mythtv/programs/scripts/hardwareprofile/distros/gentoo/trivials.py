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

import portage
from systemprofile import SystemProfile

import os
import sys
import distros.shared.html as html
from gate import Gate

class Trivials:
    def __init__(self):
        self._publish_arch_related = Gate().grants('gentoo', 'arch_related')
        self._publish_system_profile = Gate().grants('gentoo', 'system_profile')

        self._trivials = {}

        self._trivial_scalars = {}
        for upper in ('ARCH', 'CHOST'):
            value = portage.settings[upper].strip()
            lower = upper.lower()
            if self._publish_arch_related:
                self._trivial_scalars[lower] = value
            else:
                self._trivial_scalars[lower] = 'WITHHELD'
            self._trivials[lower] = self._trivial_scalars[lower]

        if self._publish_system_profile:
            system_profile = SystemProfile().get()
        else:
            system_profile = 'WITHHELD'
        self._trivial_scalars['system_profile'] = system_profile
        self._trivials['system_profile'] = self._trivial_scalars['system_profile']

        self._trivial_vectors = {}
        self._trivial_vectors['accept_keywords'] = \
            self._accept_keywords()
        self._trivials['accept_keywords'] = \
            self._trivial_vectors['accept_keywords']

    def _accept_keywords(self):
        if not self._publish_arch_related:
            return []

        # Let '~arch' kill 'arch' so we don't get both
        list = portage.settings['ACCEPT_KEYWORDS'].split(' ')
        unstable = set(e for e in list if e.startswith('~'))
        return [e for e in list if not ('~' + e) in unstable]

    def serialize(self):
        return self._trivials

    def get_metrics(self, target_dict):
        target_dict['arch_related'] = (self._publish_arch_related, \
                0, \
                self._publish_arch_related and 3 or 0)
        target_dict['system_profile'] = (self._publish_system_profile, \
                0, \
                self._publish_system_profile and 1 or 0)

    def dump_html(self, lines):
        lines.append('<h2>General</h2>')
        key_data = {
            'arch':'ARCH',
            'chost':'CHOST',
            'accept_keywords':'ACCEPT_KEYWORDS',
            'system_profile':'System Profile',
        }
        for k, label in sorted(key_data.items()):
            v = self._trivials[k]
            lines.append('<dl>')
            lines.append('<dt>%s</dt>' % html.escape(label))
            if type(v).__name__ == 'list':
                lines.append('<dd>%s</dd>' % html.escape(', '.join(v)))
            else:
                lines.append('<dd>%s</dd>' % html.escape(v))
            lines.append('</dl>')

    def dump_rst(self, lines):
        lines.append('General')
        lines.append('-----------------------------')
        key_data = {
            'arch':'ARCH',
            'chost':'CHOST',
            'accept_keywords':'ACCEPT_KEYWORDS',
            'system_profile':'System Profile',
        }
        for k, label in sorted(key_data.items()):
            v = self._trivials[k]
            lines.append(label)
            if type(v).__name__ == 'list':
                lines.append('  %s' % ', '.join(v))
            else:
                lines.append('  %s' % str(v))
            lines.append('')

    def _dump(self):
        lines = []
        self.dump_rst(lines)
        print '\n'.join(lines)
        print

    """
    def dump(self):
        print 'Trivial scalars:'
        for k, v in self._trivial_scalars.items():
            print '  %s: %s' % (k, v)
        print 'Trivial vectors:'
        for k, v in self._trivial_vectors.items():
            print '  %s: %s' % (k, v)
        print
    """

if __name__ == '__main__':
    trivials = Trivials()
    trivials._dump()
