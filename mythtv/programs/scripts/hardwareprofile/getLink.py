# -*- coding: utf-8 -*-

### This module isn't used anywhere in the MythTV repository.

# smolt - Fedora hardware profiler
#
# Copyright (C) 2008 Yaakov M. Nemoy <loupgaroublond@gmail.com>
# Copyright (C) 2008 Chris Lamb <chris@chris-lamb.co.uk>
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

from __future__ import print_function
import sys
from optparse import OptionParser
import requests
import json

sys.path.append('/usr/share/smolt/client')

from i18n import _
import smolt
from smolt import debug
from smolt import error
from scan import scan

parser = OptionParser(version = smolt.smoltProtocol)

parser.add_option('-d', '--debug',
                  dest = 'DEBUG',
                  default = False,
                  action = 'store_true',
                  help = _('enable debug information'))
parser.add_option('-s', '--server',
                  dest = 'smoonURL',
                  default = smolt.smoonURL,
                  metavar = 'smoonURL',
                  help = _('specify the URL of the server (default "%default")'))
parser.add_option('-u', '--useragent', '--user_agent',
                  dest = 'user_agent',
                  default = smolt.user_agent,
                  metavar = 'USERAGENT',
                  help = _('specify HTTP user agent (default "%default")'))
parser.add_option('-t', '--timeout',
                  dest = 'timeout',
                  type = 'float',
                  default = smolt.timeout,
                  help = _('specify HTTP timeout in seconds (default %default seconds)'))
parser.add_option('--uuidFile',
                  dest = 'uuidFile',
                  default = smolt.hw_uuid_file,
                  help = _('specify which uuid to use, useful for debugging and testing mostly.'))

(opts, args) = parser.parse_args()

def main():
    from gate import create_default_gate
    profile = smolt.create_profile(create_default_gate(), smolt.read_uuid())
    session = requests.Session()
    session.headers.update({'USER-AGENT': opts.user_agent})

    o = session.post(opts.smoonURL + '', timeout=opts.timeout)

    exceptions = (requests.exceptions.HTTPError,
                  requests.exceptions.URLRequired,
                  requests.exceptions.Timeout,
                  requests.exceptions.ConnectionError,
                  requests.exceptions.InvalidURL)

    #first find out the server desired protocol
    try:
        #fli is a file like item
        pub_uuid_fli = opts.smoonURL + 'client/pub_uuid?uuid=%s' % profile.host.UUID
        pub_uuid_fli = session.get(pub_uuid_fli, timeout=opts.timeout)
    except exceptions as e:
        error(_('Error contacting Server: %s') % e)
        return 1
    try:
        pub_uuid_obj = pub_uuid_fli.json()
        try:
            print(_('To view your profile visit: %s') % smolt.get_profile_link(opts.smoonURL, pub_uuid_obj["pub_uuid"]))
        except ValueError as e:
            error(_('Something went wrong fetching the public UUID'))
    finally:
        session.close()

if __name__ == '__main__':
    main()
