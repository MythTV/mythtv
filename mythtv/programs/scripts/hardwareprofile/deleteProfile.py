#!/usr/bin/python2

# smolt - Fedora hardware profiler
#
# Copyright (C) 2007 Mike McGrath
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

import sys
import urlgrabber.grabber
from optparse import OptionParser
from urlparse import urljoin

sys.path.append('/usr/share/smolt/client')

from i18n import _
import smolt
from smolt import error
from smolt import debug

def serverMessage(page):
    for line in page.split("\n"):
        if 'ServerMessage:' in line:
            error(_('Server Message: "%s"') % line.split('ServerMessage: ')[1])
            if 'Critical' in line:
                sys.exit(3)

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
parser.add_option('-p', '--printOnly',
                  dest = 'printOnly',
                  default = False,
                  action = 'store_true',
                  help = _('print information only, do not send'))
parser.add_option('-u', '--useragent',
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

smolt.DEBUG = opts.DEBUG
smolt.hw_uuid_file = opts.uuidFile
# read the profile
profile = smolt.Hardware()

grabber = urlgrabber.grabber.URLGrabber(user_agent=opts.user_agent, timeout=opts.timeout)

delHostString = 'uuid=%s' % profile.host.UUID

try:
    o=grabber.urlopen(urljoin(opts.smoonURL + '/', '/client/delete'), data=delHostString, http_headers=(
                    ('Content-length', '%i' % len(delHostString)),
                    ('Content-type', 'application/x-www-form-urlencoded')))
except urlgrabber.grabber.URLGrabError, e:
    sys.stderr.write(_('Error contacting Server:'))
    sys.stderr.write(str(e))
    sys.stderr.write('\n')
    sys.exit(1)
else:
    serverMessage(o.read())
    o.close()

sys.stdout.write(_('Profile removed, please verify at'))
sys.stdout.write(' ')
sys.stdout.write(urljoin(opts.smoonURL + '/', '/client/show?%s\n' % delHostString))


