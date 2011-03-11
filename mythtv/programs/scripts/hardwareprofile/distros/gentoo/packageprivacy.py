# -*- coding: utf-8 -*-
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
from portage.versions import catsplit
from generatedpackages import GeneratedPackages
from overlays import Overlays
from tools.wrappers import get_cp

def is_private_package_atom(atom, installed_from=None, debug=False):
    cat = catsplit(get_cp(atom))[0]
    if GeneratedPackages().is_generated_cat(cat):
        if GeneratedPackages().is_private_cat(cat):
            if debug:
                print '  skipping "%s" (%s, %s)' % \
                    (atom, 'was generated', 'is currently blacklisted')
            return True
        else:
            if installed_from and \
                    installed_from[0] != '' and \
                    not GeneratedPackages().is_dedicated_repo_name(
                        installed_from[0]):
                if debug:
                    print '  removing %s source tree "%s" for atom "%s"' % \
                    ('misleading', installed_from[0], atom)
                installed_from[0] = ''

    if installed_from != None:
        """
        -   We collect private packages iff they come from a non-private
            overlay, because that means they were in there before and are
            actually not private.  An example would be media-sound/xmms.

        -   We collect packages from private overlays iff the package also
            exists in a non-private overlay.  An example would be that
            you posted your ebuild to bugs.gentoo.org and somebody added
            it to the tree in the meantime.
        """
        if not Overlays().is_private_overlay_name(installed_from[0]):
            return False
        if not Overlays().is_private_package_atom(atom):
            if installed_from and installed_from[0] != '':
                if debug:
                    print '  removing %s source tree "%s" for atom "%s"' % \
                    ('private', installed_from[0], atom)
                installed_from[0] = ''
            return False
        if debug:
            print '  skipping "%s" (%s, %s)' % \
                (atom, 'was installed from private tree',
                'is currently not in non-private tree')
        return True
    else:
        not_in_public_trees = Overlays().is_private_package_atom(atom)
        if debug and not_in_public_trees:
            print '  skipping "%s" (not in public trees)' % atom
        return not_in_public_trees


if __name__ == '__main__':
    print is_private_package_atom('=cat/pkg-1.2', debug=True)
    print is_private_package_atom('=cat/pkg-1.2', installed_from=['gentoo', ],
        debug=True)
