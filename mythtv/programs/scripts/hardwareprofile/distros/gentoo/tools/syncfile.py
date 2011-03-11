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
from stat import *
import conf
import urllib2
from datetime import date, datetime

MAX_AGE_IN_SECONDS = 5 * 60

def resolve_basename(local_basename):
    return '%s/%s' % (conf.get_user_config_dir(), local_basename)

class SyncFile:
    def __init__(self, url, local_basename):
        self.remote_location = url
        self.local_location = resolve_basename(local_basename)
        conf.ensure_user_config_dir_exists()
        if self._sync_needed():
            self.sync()

    def path(self):
        return self.local_location

    def _sync_needed(self):
        try:
            mod_datetime = datetime.fromtimestamp(
                    os.stat(self.local_location)[ST_MTIME])
            age_in_seconds = (datetime.now() - mod_datetime).seconds
            if age_in_seconds < MAX_AGE_IN_SECONDS:
                return False
        except:
            pass
        return True

    def sync(self):
        opener = urllib2.build_opener()
        try:
            remote_file = opener.open(self.remote_location)
            local_file = open(self.local_location, 'w')
            local_file.write(remote_file.read())
            local_file.close()
            remote_file.close()
        except urllib2.HTTPError:
            pass
