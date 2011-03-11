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
import logging
import os

_SECTION = 'MAIN'


def _get_option_name(hw_uuid, host):
    return '%s__%s' % (hw_uuid, host)


class _UuidDb:
    def __init__(self, database_filename):
        self._database_filename = database_filename
        self._config = ConfigParser.RawConfigParser()
        self._config.read(self._database_filename)
        if not self._config.has_section(_SECTION):
            self._config.add_section(_SECTION)

    def _flush(self):
        try:
            smolt_user_config_dir = os.path.expanduser('~/.smolt/')
            if not os.path.exists(smolt_user_config_dir):
                os.mkdir(smolt_user_config_dir, 0700)
            f = open(self._database_filename, 'w')
            self._config.write(f)
            f.close()
        except:
            logging.error('Flushing UUID database failed')

    def get_pub_uuid(self, hw_uuid, host):
        try:
            pub_uuid = self._config.get(_SECTION, _get_option_name(hw_uuid, host))
            logging.info('Public UUID "%s" read from database' % pub_uuid)
            return pub_uuid
        except ConfigParser.NoOptionError:
            return None

    def set_pub_uuid(self, hw_uuid, host, pub_uuid):
        for i in (hw_uuid, host, pub_uuid):
            if not i:
                raise Exception('No paramater allowed to be None.')
        self._config.set(_SECTION, _get_option_name(hw_uuid, host), pub_uuid)
        logging.info('Public UUID "%s" written to database' % pub_uuid)
        self._flush()


_uuid_db_instance = None
def UuidDb():
    """Simple singleton wrapper with lazy initialization"""
    global _uuid_db_instance
    if _uuid_db_instance == None:
        import config
        from smolt import get_config_attr
        _uuid_db_instance =  _UuidDb(get_config_attr("UUID_DB", os.path.expanduser('~/.smolt/uuiddb.cfg')))
    return _uuid_db_instance
