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
import os

class _Gate:
    def __init__(self, the_only_config_file):
        config_files = (the_only_config_file == None) and \
                ['/etc/smolt/client.cfg',
                    os.path.expanduser('~/.smolt/client.cfg')] or \
                [the_only_config_file]
        self.config = ConfigParser.ConfigParser()
        self.config.read(config_files)

    def grants(*args):
        assert 2 <= len(args) <= 3
        if len(args) == 2:
            self, data_set = args
            return self._grants("any", data_set)
        else:
            self, distro, data_set = args
            return self._grants(distro, data_set)

    def _grants(self, distro, data_set):
        try:
            return self.config.getboolean(distro, data_set)
        except (ValueError,
                ConfigParser.NoOptionError,
                ConfigParser.NoSectionError):
            # TODO warn about error?
            # Allow if in doubt - backwards compat
            return True

    def process(self, data_set, value_if_granted, value_else):
        if self.grants(data_set):
            return value_if_granted
        else:
            return value_else

_gate = None

def Gate():
    """Simple singleton wrapper with lazy initialization"""
    global _gate
    if _gate == None:
        _gate = _Gate(None)
    return _gate

def GateFromConfig(the_only_config_file):
    """Simple singleton wrapper with lazy initialization"""
    global _gate
    if _gate == None:
        _gate = _Gate(the_only_config_file)
    return _gate
