#!/usr/bin/env python
# -*- coding: utf-8 -*-

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
from optparse import OptionParser
import time
from urlparse import urljoin
import os
import random
import getpass
from tempfile import NamedTemporaryFile

try:
    import subprocess
except ImportError, e:
    pass

sys.path.append('/usr/share/smolt/client')

from i18n import _
import smolt
from smolt import debug
from smolt import error
from smolt import get_config_attr
from smolt import to_ascii
from scan import scan, rating
from gate import GateFromConfig

parser = OptionParser(version = smolt.clientVersion)

parser.add_option('-d', '--debug',
                  dest = 'DEBUG',
                  default = False,
                  action = 'store_true',
                  help = _('enable debug information'))
parser.add_option('--config',
                  dest = 'the_only_config_file',
                  default = None,
                  metavar = 'file.cfg',
                  help = _('specify the location of the (only) config file to use'))
parser.add_option('-s', '--server',
                  dest = 'smoonURL',
                  default = smolt.smoonURL,
                  metavar = 'smoonURL',
                  help = _('specify the URL of the server (default "%default")'))
parser.add_option('--username',
                  dest = 'userName',
                  default = None,
                  metavar = 'userName',
                  help = _('(optional) Fedora Account System registration'))
parser.add_option('--password',
                  dest = 'password',
                  default = None,
                  metavar = 'password',
                  help = _('password, will prompt if not specified'))
parser.add_option('-p', '--printOnly',
                  dest = 'printOnly',
                  default = False,
                  action = 'store_true',
                  help = _('print information only, do not send'))
parser.add_option('-a', '--autoSend',
                  dest = 'autoSend',
                  default = False,
                  action = 'store_true',
                  help = _('don\'t prompt to send, just send'))
parser.add_option('-r', '--retry',
                  dest = 'retry',
                  default = False,
                  action = 'store_true',
                  help = _('continue to send until success'))
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
parser.add_option('-c', '--checkin',
                  dest = 'checkin',
                  default = False,
                  action = 'store_true',
                  help = _('this is an automated checkin, will only run if the "smolt" service has been started'))
parser.add_option('-S', '--scanOnly',
                  dest = 'scanOnly',
                  default = False,
                  action = 'store_true',
                  help = _('only scan this machine for known hardware errata, do not send profile.'))
parser.add_option('--submitOnly',
                  dest = 'submitOnly',
                  default = False,
                  action = 'store_true',
                  help = _('do not scan this machine for know hardware errata, only submit profile.'))
parser.add_option('--uuidFile',
                  dest = 'uuidFile',
                  default = smolt.hw_uuid_file,
                  help = _('specify which uuid to use, useful for debugging and testing mostly.'))
#parser.add_option('-b', '--bodhi',
#                  dest = 'bodhi',
#                  default = False,
#                  action = 'store_true',
#                  help = _('Submit this profile to Bodhi as well, for Fedora Developmnent'))
parser.add_option('-n', '--newPublicUUID',
                  dest = 'new_pub',
                  default = False,
                  action = 'store_true',
                  help = _('Request a new public UUID'))
parser.add_option('--http-proxy',
                  dest = 'httpproxy',
                  default = None,
                  help = _('HTTP proxy'))


(opts, args) = parser.parse_args()

if opts.the_only_config_file != None:
    GateFromConfig(opts.the_only_config_file)

smolt.DEBUG = opts.DEBUG
smolt.hw_uuid_file = opts.uuidFile
if opts.httpproxy == None:
    proxies = None
else:
    proxies = {'http':opts.httpproxy}

if opts.checkin and os.path.exists('/var/lock/subsys/smolt'):
    # Smolt is set to run
    opts.autoSend = True
elif opts.checkin:
    # Tried to check in but checkins are disabled
    print _('Smolt set to checkin but checkins are disabled (hint: service smolt start)')
    sys.exit(6)

# read the profile
try:
    profile = smolt.get_profile()
except smolt.UUIDError, e:
    sys.stderr.write(_('%s\n' % e))
    sys.exit(9)

if opts.new_pub:
    pub_uuid = profile.regenerate_pub_uuid(user_agent=opts.user_agent,
                              smoonURL=opts.smoonURL,
                              timeout=opts.timeout)
    print _('Success!  Your new public UUID is: %s' % pub_uuid)
    sys.exit(0)

if opts.scanOnly:
    scan(profile, opts.smoonURL)
    rating(profile, opts.smoonURL)
    sys.exit(0)

if not opts.autoSend:
    if opts.printOnly:
        for line in profile.getProfile():
            if not line.startswith('#'):
                print line
        sys.exit(0)

    def inner_indent(text):
        return ('\n' + 5 * ' ').join(text.split('\n'))

    excerpts = {
        'label_intro':_('Smolt has collected four types of information:'),
        'label_question':_('Do you want to ..'),
        'label_question_view':_('(v)iew details on collected information?'),
        'label_question_send':_('(s)end this information to the Smolt server?'),
        'label_question_quit':_('(q)uit Smolt?'),
        'label_general':_('General'),
        'label_devices':_('Devices'),
        'label_fs_related':_('File system-related'),
        'label_distro_specific':_('Distribution-specific'),

        'general':inner_indent(to_ascii(profile.get_general_info_excerpt())),
        'devices':inner_indent(to_ascii(profile.get_devices_info_excerpt())),
        'file_system':inner_indent(to_ascii(profile.get_file_system_info_excerpt())),
        'distro':inner_indent(to_ascii(profile.get_distro_info_excerpt())),
    }

    submit = False
    while not submit:
        print """\
=====================================================
%(label_intro)s

  %(label_general)s
     %(general)s

  %(label_devices)s
     %(devices)s

  %(label_fs_related)s
     %(file_system)s

  %(label_distro_specific)s
     %(distro)s

=====================================================
%(label_question)s
  %(label_question_view)s
  %(label_question_send)s
  %(label_question_quit)s
""" % excerpts
        try:
            choice = raw_input(_('Your choice (s)end (v)iew (q)uit: ')).strip()
        except KeyboardInterrupt:
            error(_('Exiting...'))
            sys.exit(4)
        if choice in (_('s|y|yes')).split('|'):
            submit = True
            print '\n\n'
        elif choice in (_('q|n|no')).split('|'):
            sys.exit(0)
        elif choice in (_('v')).split('|'):
            f = NamedTemporaryFile()
            for line in profile.getProfile():
                try:
                    f.write(line + '\n')
                except UnicodeEncodeError:
                    pass
            f.flush()
            os.chmod(f.name, 0400)
            try:
                pager_command = os.environ['PAGER']
            except KeyError:
                if os.path.exists('/usr/bin/less'):
                    pager_command = '/usr/bin/less'
                elif os.path.exists('/bin/less'):
                    pager_command = '/bin/less'
                else:
                    #fallback to more  , could use /bin/more but might as well let the path sort it out.
                    pager_command = 'more'
            try:
                subprocess.call([pager_command, f.name])
            except NameError:
                os.system(' '.join([pager_command, f.name]))
            f.close()
            print '\n\n'
        else:
            error(_('Exiting...'))
            sys.exit(4)


if opts.retry:
    while 1:
        result, pub_uuid, admin = profile.send(user_agent=opts.user_agent,
                              smoonURL=opts.smoonURL,
                              timeout=opts.timeout,
                              proxies=proxies)
        if not result:
            sys.exit(0)
        error(_('Retry Enabled - Retrying'))
        time.sleep(30)
else:
    result, pub_uuid, admin = profile.send(user_agent=opts.user_agent,
                                    smoonURL=opts.smoonURL,
                                    timeout=opts.timeout,
                                    proxies=proxies)

    if result:
        print _('Could not send - Exiting')
        sys.exit(1)

if opts.userName:
    if not opts.password:
        password = getpass.getpass('\n' + _('Password:') + ' ')
    else:
        password = opts.password

    if profile.register(userName=opts.userName, password=password, user_agent=opts.user_agent, smoonURL=opts.smoonURL, timeout=opts.timeout):
        print _('Registration Failed, Try again')
if not opts.submitOnly and not opts.checkin:
    scan(profile, opts.smoonURL)
    try:
        rating(profile, opts.smoonURL)
    except ValueError:
        print "Could not get rating!"
print

if pub_uuid:
    pubUrl = smolt.get_profile_link(opts.smoonURL, pub_uuid)
    print _('To share your profile: \n\t%s (public)') % pubUrl
    hw_uuid_file = get_config_attr("HW_PUBID", "/etc/smolt/hw-uuid.pub")
    hw_uuid_pub = os.path.basename(pubUrl)
    if not smolt.secure:
        print _('\tAdmin Password: %s') % admin

else:
    print _('No Public UUID found!  Please re-run with -n to generate a new public uuid')

