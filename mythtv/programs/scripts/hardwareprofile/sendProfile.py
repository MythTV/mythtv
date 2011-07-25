#!/usr/bin/python
# -*- coding: utf-8 -*-

# smolt - Fedora hardware profiler
#
# Copyright (C) 2007 Mike McGrath
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

import sys
import time
import os
import getpass


def ensure_code_reachability():
    _code_location = '/usr/share/smolt/client'
    if sys.path[-1] == _code_location:
        return
    sys.path.append(_code_location)


def command_line():
    ensure_code_reachability()
    from i18n import _
    import smolt

    from optparse import OptionParser
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
                    dest = 'cron_mode',
                    default = False,
                    action = 'store_true',
                    help = _('do an automated checkin as when run from cron (implies --autoSend)'))
    parser.add_option('-S', '--scanOnly',
                    dest = 'send_profile',
                    default = True,
                    action = 'store_false',
                    help = _('only scan this machine for known hardware errata, do not send profile.'))
    parser.add_option('--submitOnly',
                    dest = 'scan_remote',
                    default = True,
                    action = 'store_false',
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

    if opts.cron_mode:
        # Smolt is set to run
        opts.autoSend = True

    return opts, args


def make_display_excerpts(profile):
    ensure_code_reachability()
    from i18n import _
    from smolt import to_ascii

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
    return excerpts


def dump_excerpts(excerpts):
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


def present_and_require_confirmation(profile):
    import subprocess
    from tempfile import NamedTemporaryFile

    ensure_code_reachability()
    from i18n import _
    from smolt import error

    excerpts = make_display_excerpts(profile)

    submit = False
    while not submit:
        dump_excerpts(excerpts)

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


def do_send_profile(uuiddb, uuid, profile, opts, proxies):
    (error_code, pub_uuid, admin) = profile.send(uuiddb, uuid, user_agent=opts.user_agent,
                        smoonURL=opts.smoonURL,
                        timeout=opts.timeout,
                        proxies=proxies,
                        batch=opts.cron_mode)
    return (error_code, pub_uuid, admin)


def send_profile(uuiddb, uuid, profile, opts, proxies):
    ensure_code_reachability()
    from i18n import _
    from smolt import error

    if opts.retry:
        while 1:
            (error_code, pub_uuid, admin) = do_send_profile(uuiddb, uuid, profile, opts, proxies)
            if not error_code:
                break
            error(_('Retry Enabled - Retrying'))
            time.sleep(30)
    else:
        (error_code, pub_uuid, admin) = do_send_profile(uuiddb, uuid, profile, opts, proxies)
        if error_code:
            print _('Could not send - Exiting')
            sys.exit(1)

    return (error_code, pub_uuid, admin)


def mention_profile_web_view(opts, pub_uuid, admin):
    ensure_code_reachability()
    import smolt
    from i18n import _

    pubUrl = smolt.get_profile_link(opts.smoonURL, pub_uuid)
    print
    print _('To share your profile: \n\t%s (public)') % pubUrl
    if not smolt.secure:
        print _('\tAdmin Password: %s') % admin


def get_proxies(opts):
    if opts.httpproxy == None:
        proxies = None
    else:
        proxies = {'http':opts.httpproxy}
    return proxies


def read_profile(gate, uuid):
    ensure_code_reachability()
    from i18n import _
    import smolt

    try:
        profile = smolt.create_profile(gate, uuid)
    except smolt.UUIDError, e:
        sys.stderr.write(_('%s\n' % e))
        sys.exit(9)
    return profile


def register_with_fedora_account_system(opts):
    ensure_code_reachability()
    from i18n import _

    if not opts.password:
        password = getpass.getpass('\n' + _('Password:') + ' ')
    else:
        password = opts.password

    if profile.register(userName=opts.userName, password=password, user_agent=opts.user_agent, smoonURL=opts.smoonURL, timeout=opts.timeout):
        print _('Registration Failed, Try again')


def do_scan_remote(profile, opts, gate):
    ensure_code_reachability()
    from scan import scan, rating

    scan(profile, opts.smoonURL, gate)
    try:
        rating(profile, opts.smoonURL, gate)
    except ValueError:
        print "Could not get rating!"


def mention_missing_uuid():
    ensure_code_reachability()
    from i18n import _
    print
    print _('No Public UUID found!  Please re-run with -n to generate a new public uuid')


def main_request_new_public_uuid(uuiddb, uuid, profile, opts):
    ensure_code_reachability()
    from i18n import _
    from smolt import error, ServerError

    try:
        pub_uuid = profile.regenerate_pub_uuid(uuiddb, uuid, user_agent=opts.user_agent,
                            smoonURL=opts.smoonURL,
                            timeout=opts.timeout)
    except ServerError, e:
        error(_('Error contacting server: %s') % str(e))
        sys.exit(1)

    print _('Success!  Your new public UUID is: %s' % pub_uuid)
    sys.exit(0)


def main_scan_only(profile, opts, gate):
    do_scan_remote(profile, opts, gate)
    sys.exit(0)


def main_print_only(profile):
    for line in profile.getProfile():
        if not line.startswith('#'):
            print line.encode('utf-8')
    sys.exit(0)


def main_send_profile(uuiddb, uuid, profile, opts, gate):
    proxies = get_proxies(opts)

    if not opts.autoSend:
        present_and_require_confirmation(profile)

    (error_code, pub_uuid, admin) = send_profile(uuiddb, uuid, profile, opts, proxies)

    if opts.userName:
        register_with_fedora_account_system(opts)

    if opts.scan_remote and not opts.cron_mode:
        do_scan_remote(profile, opts, gate)

    if pub_uuid:
        mention_profile_web_view(opts, pub_uuid, admin)
    elif not opts.cron_mode:
        mention_missing_uuid()


def main():
    ensure_code_reachability()
    from i18n import _
    import smolt
    from gate import create_default_gate, create_gate_from_file
    from uuiddb import create_default_uuiddb

    (opts, args) = command_line()

    if opts.the_only_config_file is None:
        gate = create_default_gate()
    else:
        gate = create_gate_from_file(opts.the_only_config_file)

    smolt.DEBUG = opts.DEBUG
    smolt.hw_uuid_file = opts.uuidFile

    profile = read_profile(gate, smolt.read_uuid())

    if opts.new_pub:
        uuiddb = create_default_uuiddb()
        uuid = smolt.read_uuid()
        main_request_new_public_uuid(uuiddb, uuid, profile, opts)
    elif not opts.send_profile:
        main_scan_only(profile, opts)
    elif opts.printOnly and not opts.autoSend:
        main_print_only(profile)
    else:
        uuiddb = create_default_uuiddb()
        uuid = smolt.read_uuid()
        main_send_profile(uuiddb, uuid, profile, opts, gate)


if __name__ == '__main__':
    main()
