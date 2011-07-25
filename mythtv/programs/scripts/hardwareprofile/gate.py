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
import logging


class _GateBase:
    def process(self, data_set, value_if_granted, value_else):
        if self.grants(data_set):
            return value_if_granted
        else:
            return value_else


class _Gate(_GateBase):
    def __init__(self, config_files=None):
        if config_files is None:
            config_files = [
                '/etc/smolt/client.cfg',
                os.path.expanduser('~/.smolt/client.cfg'),
            ]
        self.config = ConfigParser.ConfigParser()
        self.config.read(config_files)

    def grants(*args):
        assert 2 <= len(args) <= 3
        if len(args) == 2:
            self, data_set = args
            distro = 'any'
        else:
            self, distro, data_set = args
        res = self._grants(distro, data_set)
        logging.debug('Section "%s", key "%s" --> "%s"' % (distro, data_set, str(res)))
        return res

    def _grants(self, distro, data_set):
        try:
            return self.config.getboolean(distro, data_set)
        except (ValueError,
                ConfigParser.NoOptionError,
                ConfigParser.NoSectionError):
            # TODO warn about error?
            # Allow if in doubt - backwards compat
            return True


class _FakeGate(_GateBase):
    def __init__(self, grant):
        """
        >>> gate = _FakeGate(grant=True)
        >>> gate.grants("whatever")
        True
        >>> gate = _FakeGate(grant=False)
        >>> gate.grants("whatever")
        False
        """
        self._granted = grant

    def grants(self, *args):
        return self._granted


def create_default_gate():
    return _Gate()


def create_gate_from_file(filename):
    return _Gate([filename, ])


def create_passing_gate():
    """
    >>> create_passing_gate().grants("whatever")
    True
    """
    return _FakeGate(grant=True)


def create_blocking_gate():
    """
    >>> create_blocking_gate().grants("whatever")
    False
    """
    return _FakeGate(grant=False)
