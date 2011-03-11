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

# TODO move somewhere else?
SMOLT_CONFIG_DIR = os.path.expanduser('~/.smolt')

def get_user_config_dir():
    return SMOLT_CONFIG_DIR

def get_system_config_dir():
    return '/etc/smolt'

def ensure_user_config_dir_exists():
    # TODO improve
    DIR_EXISTS_ERRNO = 17
    try:
        os.mkdir(SMOLT_CONFIG_DIR, 0700)
    except OSError, e:
        if e.errno != DIR_EXISTS_ERRNO:
            raise e
