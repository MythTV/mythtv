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

import ConfigParser
import portage
from portage.const import REPO_NAME_LOC
import re
import os
from portage import config
from portage.versions import catpkgsplit
from portage.dbapi.porttree import portdbapi
from tools.maintreedir import main_tree_dir
from tools.syncfile import SyncFile
from tools.overlayparser import OverlayParser
from xml.parsers.expat import ExpatError
import logging

import sys
import distros.shared.html as html
from gate import Gate
from tools.wrappers import get_cp

_REPO_NAME_WHITELIST = (
    "gentoo",  # Gentoo main tree
    "funtoo",  # Funtoo main tree
    "gentoo_prefix",  # Gentoo prefix main tree

    "g-ctan",  # app-portage/g-ctan target repository
)

_REPO_NAME_RENAME_MAP = {
    "bangert-ebuilds":"bangert",
    "china":"gentoo-china",
    "dev-jokey":"jokey",
    "digital-trauma.de":"trauma",
    "Falco's git overlay":"falco",
    "gcj-overlay":"java-gcj-overlay",
    "geki-overlay":"openoffice-geki",
    "gentoo-haskell":"haskell",
    "gentoo-lisp-overlay":"lisp",
    "kde4-experimental":"kde4-experimental",  # Keep as is (overlay "kde")
    "kde":"kde",  # Keep as is (overlay "kde-testing")
    "leio-personal":"leio",
    "maekke's overlay":"maekke",
    "majeru":"oss-overlay",
    "mpd-portage":"mpd",
    "ogo-lu_zero":"lu_zero",
    "pcsx2-overlay":"pcsx2",
    "proaudio":"pro-audio",
    "scarabeus_local_overlay":"scarabeus",
    "soor":"soor-overlay",
    "steev_github":"steev",
    "suka's dev overlay - experimental stuff of all sorts":"suka",
    "tante_overlay":"tante",
    "vdr-xine-overlay":"vdr-xine",
    "webapp-experimental":"webapps-experimental",
    "x-njw-gentoo-local":"njw",
}

class _Overlays:
    def __init__(self):
        self._publish = Gate().grants('gentoo', 'repositories')
        self._fill_overlays()
        tree_config = config()
        tree_config['PORTDIR_OVERLAY'] = ' '.join(self._get_active_paths())
        tree_config['PORTDIR'] = main_tree_dir()
        self._dbapi = portdbapi(main_tree_dir(), tree_config)

    def _parse_overlay_meta(self, filename):
        parser = OverlayParser()
        file = open(filename, 'r')
        parser.parse(file.read())
        file.close()
        return parser.get()

    def _get_known_overlay_map(self):
        sync_file = SyncFile(
                'http://www.gentoo.org/proj/en/overlays/layman-global.txt',
                'layman-global.txt')

        # Parse, retry atfer re-sync if errors
        try:
            for retry in (2, 1, 0):
                try:
                    return self._parse_overlay_meta(sync_file.path())
                except ExpatError:
                    if retry > 0:
                        logging.info('Re-syncing %s due to parse errors' % sync_file.path())
                        sync_file.sync()
        except IOError:
            pass
        return {}

    def _fill_overlays(self):
        self._global_overlays_dict = self._get_known_overlay_map()
        enabled_installed_overlays = \
                portage.settings['PORTDIR_OVERLAY'].split(' ')

        def overlay_name(overlay_location):
            repo_name_file = os.path.join(overlay_location, REPO_NAME_LOC)
            file = open(repo_name_file, 'r')
            name = file.readline().strip()
            file.close()
            return name

        def is_non_private(overlay_location):
            try:
                name = overlay_name(overlay_location)
            except:
                return False

            # TODO improve
            return name in ('g-ctan', ) or \
                    (name in self._global_overlays_dict)

        def fix_repo_name(repo_name):
            try:
                return _REPO_NAME_RENAME_MAP[repo_name]
            except KeyError:
                return repo_name

        non_private_active_overlay_paths = [e for e in
                enabled_installed_overlays if is_non_private(e)]
        non_private_active_overlay_names = [fix_repo_name(overlay_name(e)) \
                for e in non_private_active_overlay_paths]

        # Dirty hack to get the main tree's name in, too
        main_tree_name = fix_repo_name(overlay_name(main_tree_dir()))
        enabled_installed_overlays.append(main_tree_name)
        if not self.is_private_overlay_name(main_tree_name):
            non_private_active_overlay_names.append(main_tree_name)

        self._active_overlay_paths = non_private_active_overlay_paths
        if self._publish:
            self._active_overlay_names = non_private_active_overlay_names
            self._repo_count_non_private = len(non_private_active_overlay_names)
            self._repo_count_private = len(enabled_installed_overlays) - \
                    self._repo_count_non_private
        else:
            self._active_overlay_names = []
            self._repo_count_non_private = 0
            self._repo_count_private = 0

    def _get_active_paths(self):
        return self._active_overlay_paths

    def is_private_package_atom(self, atom):
        cp = get_cp(atom)
        return not self._dbapi.cp_list(cp)

    def is_private_overlay_name(self, overlay_name):
        if not overlay_name:
            return True

        if overlay_name in _REPO_NAME_WHITELIST:
            return False

        if overlay_name in _REPO_NAME_RENAME_MAP:
            return False

        return not overlay_name in self._global_overlays_dict

    def serialize(self):
        return sorted(set(self._active_overlay_names))

    def get_metrics(self, target_dict):
        target_dict['repos'] = (self._publish, \
                self._repo_count_private, \
                self._repo_count_non_private)

    def dump_html(self, lines):
        lines.append('<h2>Repositories</h2>')
        lines.append('<ul>')
        for name in self.serialize():
            lines.append('<li><a href="http://gentoo-overlays.zugaina.org/%(name)s/">%(name)s</a></li>' % {'name':html.escape(name)})
        lines.append('</ul>')

    def dump_rst(self, lines):
        lines.append('Repositories')
        lines.append('-----------------------------')
        for v in self.serialize():
            lines.append('- ' + v)

    def _dump(self):
        lines = []
        self.dump_rst(lines)
        print '\n'.join(lines)
        print

    """
    def dump(self):
        print 'Active overlays:'
        print '  Names:'
        print self.get_active_names()
        print '  Paths:'
        print self._get_active_paths()
        print '  Total: ' + str(self.total_count())
        print '    Known: ' + str(self.known_count())
        print '    Private: ' + str(self.private_count())
        print
    """


_overlays_instance = None
def Overlays():
    """
    Simple singleton wrapper around _Overlays class
    """
    global _overlays_instance
    if _overlays_instance == None:
        _overlays_instance = _Overlays()
    return _overlays_instance


if __name__ == '__main__':
    overlays = Overlays()
    overlays._dump()
