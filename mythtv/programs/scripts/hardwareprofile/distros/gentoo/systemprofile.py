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

import os
import re
import portage
from tools.maintreedir import main_tree_dir

class SystemProfile:
    def __init__(self):
        self._profile = self._filter(self._get_profile())

    def _get_profile(self):
        # expand make.profile symlink
        return os.path.realpath(portage.settings.profile_path)

    def _collect_known_profiles(self):
        # TODO extract to parser class?
        f = open('%s/profiles/profiles.desc' % main_tree_dir(), 'r')
        subst_pattern = re.compile('^[^ #]+\s+(\S+)\s+\S+\s*$')
        lines = [re.sub(subst_pattern, '\\1', l) for l in
                f.readlines() if re.match(subst_pattern, l)]
        f.close()
        return set(lines)

    def _filter(self, profile):
        known_profiles_set = self._collect_known_profiles()
        for p in known_profiles_set:
            if profile.endswith('/' + p):
                return p
        return '<private profile>'

    def get(self):
        return self._profile

    def dump(self):
        print 'System profile: ' + self._profile
        print

if __name__ == '__main__':
    systemprofile = SystemProfile()
    systemprofile.dump()
