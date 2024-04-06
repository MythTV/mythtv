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

from builtins import map
from builtins import str
from builtins import object
import configparser
import logging
import os , sys
sys.path.extend(list(map(os.path.abspath, ['../../'])))
from smolt_config import get_config_attr
from request import Request

_SECTION = 'MAIN'


def _get_option_name(hw_uuid, host):
    return '%s__%s' % (hw_uuid, host)

class UUIDError(Exception):
    def __init__(self, message):
        self.msg = message

    def __str__(self):
        return str(self.msg)

class PubUUIDError(Exception):
    def __init__(self, message):
        self.msg = message

    def __str__(self):
        return str(self.msg)

class _UuidDb(object):
    hw_uuid = None

    def __init__(self, database_filename):
        self._database_filename = database_filename
        self._config = configparser.RawConfigParser()
        self._config.read(self._database_filename)
        if not self._config.has_section(_SECTION):
            self._config.add_section(_SECTION)
        self.hw_uuid_file = get_config_attr('HW_UUID', '/etc/smolt/hw-uuid')

    def _flush(self):
        try:
            smolt_user_config_dir = os.path.expanduser('~/.smolt/')
            if not os.path.exists(smolt_user_config_dir):
                os.mkdir(smolt_user_config_dir, 0o700)
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
        except configparser.NoOptionError:
            try:
                req = Request('/client/pub_uuid/%s' % self.get_priv_uuid())
                pudict = json.loads(req.open().read())
                self.set_pub_uuid(self.hw_uuid, Request(), pudict['pub_uuid'])
                return pudict['pub_uuid']
            except Exception as e:
                error(_('Error determining public UUID: %s') % e)
                sys.stderr.write(_("Unable to determine Public UUID!  This could be a network error or you've\n"))
                sys.stderr.write(_("not submitted your profile yet.\n"))
                raise PubUUIDError('Could not determine Public UUID!\n')

    def set_pub_uuid(self, hw_uuid, host, pub_uuid):
        for i in (hw_uuid, host, pub_uuid):
            if not i:
                raise Exception('No parameter is allowed to be None')
        self._config.set(_SECTION, _get_option_name(hw_uuid, host), pub_uuid)
        logging.info('Public UUID "%s" written to database' % pub_uuid)
        self._flush()

    def gen_uuid(self):
        try:
            with open('/proc/sys/kernel/random/uuid') as rand_uuid:
                rand_uuid_string = rand_uuid.read().strip()
                return rand_uuid_string
        except IOError:
            try:
                import uuid
                return uuid.uuid4()
            except:
                raise UUIDError('Could not generate new UUID!')

    def get_priv_uuid(self):
        if self.hw_uuid:
            return self.hw_uuid

        try:
            with open(self.hw_uuid_file) as hwuuidfile:
                self.hw_uuid = hwuuidfile.read().strip()
        except IOError:
            try:
                self.hw_uuid = self.genUUID()
            except UUIDError:
                raise UUIDError('Unable to determine UUID of system!')
            try:
                # make sure directory exists, create if not
                with open(self.hw_uuid_file, 'w') as hwuuidfile:
                    hwuuidfile.write(self.hw_uuid)
            except Exception as e:
                raise UUIDError('Unable to save UUID to %s. Please run once as root.' % self.hw_uuid_file)

        return self.hw_uuid

_uuid_db_instance = None
def UuidDb():
    """Simple singleton wrapper with lazy initialization"""
    global _uuid_db_instance
    if _uuid_db_instance is None:
        import config
        from smolt import get_config_attr
        _uuid_db_instance =  _UuidDb(get_config_attr("UUID_DB", os.path.expanduser('~/.smolt/uuiddb.cfg')))
    return _uuid_db_instance
