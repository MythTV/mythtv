#!/usr/bin/python
# -*- coding: utf-8 -*-

# smolt - Fedora hardware profiler
#
# Copyright (C) 2008 James Meyer <james.meyer@operamail.com>
# Copyright (C) 2008 Yaakov M. Nemoy <loupgaroublond@gmail.com>
# Copyright (C) 2009 Carlos Gonçalves <mail@cgoncalves.info>
# Copyright (C) 2009 François Cami <fcami@fedoraproject.org>
# Copyright (C) 2010 Mike McGrath <mmcgrath@redhat.com>
# Copyright (C) 2012 Raymond Wagner <rwagner@mythtv.org>
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

class OrderedType( type ):
    # provide global sequencing for OS class and subclasses to ensure
    # the be tested in proper order
    nextorder = 0
    def __new__(mcs, name, bases, attrs):
        attrs['_order'] = mcs.nextorder
        mcs.nextorder += 1
        return type.__new__(mcs, name, bases, attrs)

class OS( object ):
    __metaclass__ = OrderedType
    _requires_func = True
    def __init__(self, ostype=-1, func=None, inst=None):
        if callable(ostype):
            # assume function being supplied directly
            func = ostype
            ostype = None

        # use None to ignore os.type() check, or -1 to default to 'posix'
        self.ostype = ostype if ostype != -1 else 'posix'

        if (func is not None) and not callable(func):
            raise TypeError((self.__class__.__name__ + "() requires a " \
                            "provided function be callable."))

        self.func = func
        self.inst = inst

        if func:
            self.__doc__ = func.__doc__
            self.__name__ = func.__name__
            self.__module__ = func.__module__

    def __get__(self, inst, owner):
        if inst is None:
            # class attribute, return self
            return inst
        func = self.func.__get__(inst, owner) if self.func else None
        return self.__class__(self.ostype, func, inst)

    def __call__(self, func=None):
        if self.inst is None:
            # this is being called as a classmethod, prior to the class
            # being initialized into an object. we want to operate as a
            # descriptor, and receive a function as an argument
            if self.func is not None:
                raise TypeError((self.__class__.__name__ + "() has already " \
                                "been given a processing function"))
            if (func is None) or not callable(func):
                raise TypeError((self.__class__.__name__ + "() takes exactly " \
                                "one callable as a function, (0 given)"))
            self.func = func
        else:
            if self._requires_func and (self.func is None):
                raise RuntimeError((self.__class__.__name__ + "() has not " \
                                   "been given a callable function"))

            # return boolean as to whether match was successful

            if (self.ostype is not None) and (os.name != self.ostype):
                # os.type checking is enabled, and does not match
                return False

            return self.do_test()

    def do_test(self, *args, **kwargs):
        try:
            # wrap function to handle failure for whatever reason
            res = self.func(*args, **kwargs)
        except:
            return False
        else:
            if res:
                # function returned positive, store it and return True
                self.inst.name = res
                return True
            # function returned negative, return False so loop proceeds
            return False

class OSWithFile( OS ):
    _requires_func = False
    def __init__(self, filename, ostype='posix', func=None, inst=None):
        self.filename = filename
        super(OSWithFile, self).__init__(ostype, func, inst)

    def __get__(self, inst, owner):
        if inst is None:
            return inst
        func = self.func.__get__(inst, owner) if self.func else None
        return self.__class__(self.filename, self.ostype, func, inst)

    def do_test(self, *args, **kwargs):
        if not os.path.exists(self.filename):
            # filename does not exist, so assume no match
            return False

        text = open(self.filename).read().strip()
        if self.func:
            # pass text into function for further processing
            return super(OSWithFile, self).do_test(text)
        else:
            # store text as version string, and report success
            self.inst.name = text
            return True

class OSFromUname( OS ):
    @property
    def uname(self):
        # only bother running this once, store the result
        try: return self._uname
        except AttributeError: pass

        try:
            # requires subprocess to operate
            from subprocess import Popen, PIPE
        except ImportError:
            return {}

        # requires uname. if need be, this can be modified to try PATH
        # searching in the environment.
        path = '/usr/bin/uname'
        if not os.path.exists(path):
            return {}

        self._uname = {}
        # pull OS name and release from uname. more can be added if needed
        for k,v in (('OS', '-s'),
                    ('version', '-r')):
            p = Popen([path, v], stdout=PIPE)
            p.wait()
            self._uname[k] = p.stdout.read().strip()

        return self._uname

    def do_test(self, *args, **kwargs):
        if len(self.uname) == 0:
            return False
        return super(OSFromUname, self).do_test(**self.uname)

class OSInfoType( type ):
    def __new__(mcs, name, bases, attrs):
        OSs = []
        for k,v in attrs.items():
            if isinstance(v, OS):
                # build list of stored OS types
                OSs.append((v._order, k))
        # sort by ordering value inserted by OrderedType metaclass
        attrs['_oslist'] = [i[1] for i in sorted(OSs)]
        return type.__new__(mcs, name, bases, attrs)

    def __call__(cls):
        obj = cls.__new__(cls)
        obj.__init__()
        for attr in obj._oslist:
            # loop through all availble OS types in specified order
            if getattr(obj, attr)():
                # if type returns success, return value
                return obj.name
        else:
            # fall through to Unknown
            return 'Unknown'

class get_os_info( object ):
    __metaclass__ = OSInfoType

    @OS('nt')
    def windows(self):
        win_version = {
              (1, 4, 0): '95',
              (1, 4, 10): '98',
              (1, 4, 90): 'ME',
              (2, 4, 0): 'NT',
              (2, 5, 0): '2000',
              (2, 5, 1): 'XP'
        }[os.sys.getwindowsversion()[3],
          os.sys.getwindowsversion()[0],
          os.sys.getwindowsversion()[1] ]
        return "Windows " + win_version

    blag_linux  = OSWithFile('/etc/blag-release')
    mythvantage = OSWithFile('/etc/mythvantage-release')
    knoppmyth   = OSWithFile('/etc/KnoppMyth-version')
    linhes      = OSWithFile('/etc/LinHES-release')
    mythdora    = OSWithFile('/etc/mythdora-release')

    @OSWithFile('/etc/arch-release')
    def archlinux(self, text):
        return 'Arch Linux'

    @OSWithFile('/etc/aurox-release')
    def auroxlinux(self, text):
        return 'Aurox Linux'

    conectiva   = OSWithFile('/etc/conectiva-release')
    debian      = OSWithFile('/etc/debian_release')

    @OSWithFile('/etc/debian_version')
    def ubuntu(self, text):
        text = open('/etc/issue.net').read().strip()
        if text.find('Ubuntu'):
            try:
                mtext = open('/var/log/installer/media-info').read().strip()
            except:
                pass
            else:
                if 'Mythbuntu' in mtext:
                    text.replace('Ubuntu', 'Mythbuntu')
            return text
        return False

    debian2     = OSWithFile('/etc/debian_version')
    fedora      = OSWithFile('/etc/fedora-release')
    gentoo      = OSWithFile('/etc/gentoo-release')
    lfsfile     = OSWithFile('/etc/lfs-release')
    mandrake    = OSWithFile('/etc/mandrake-release')
    mandriva    = OSWithFile('/etc/mandriva-release')
    pardus      = OSWithFile('/etc/pardus-release')
    slackware   = OSWithFile('/etc/slackware-release')
    solaris     = OSWithFile('/etc/release')
    sunjds      = OSWithFile('/etc/sun-release')
    pldlinux    = OSWithFile('/etc/pld-release')

    @OSWithFile('/etc/SuSE-release')
    def suselinux(self, text):
        import re
        text = text.split('\n')[0].strip()
        return re.sub('\(\w*\)$', '', text)

    yellowdog   = OSWithFile('/etc/yellowdog-release')
    redhat      = OSWithFile('/etc/redhat-release')

    @OSFromUname
    def freebsd(self, OS, version):
        if OS == 'FreeBSD':
            return 'FreeBSD '+version
        return False

    @OSFromUname
    def OSX(self, OS, version):
        if OS != 'Darwin':
            return False
        major,minor,point = [int(a) for a in version.split('.')]
        return 'OS X 10.%s.%s' % (major-4, minor)

    @OS
    def linuxstandardbase(self):
        from subprocess import Popen, PIPE
        executable = 'lsb_release'
        for path in os.environ['PATH'].split(':'):
            fullpath = os.path.join(path, executable)
            if os.path.exists(fullpath):
                break
        else:
            return False

        p = Popen([fullpath, '--id', '--codename', '--release', '--short'],
                        stdout=PIPE, close_fds=True)
        p.wait()
        return p.stdout.read().strip().replace('\n', ' ')

    @OSFromUname
    def Linux(self, OS, version):
        if OS == 'Linux':
            return 'Unknown Linux '+version
        return False
