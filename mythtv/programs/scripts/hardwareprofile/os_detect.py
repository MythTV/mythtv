#!/usr/bin/python
# -*- coding: utf-8 -*-

# smolt - Fedora hardware profiler
#
# Copyright (C) 2008 James Meyer <james.meyer@operamail.com>
# Copyright (C) 2008 Yaakov M. Nemoy <loupgaroublond@gmail.com>
# Copyright (C) 2009 Carlos Gonçalves <mail@cgoncalves.info>
# Copyright (C) 2009 François Cami <fcami@fedoraproject.org>
# Copyright (C) 2010 Mike McGrath <mmcgrath@redhat.com>
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
import re
from UserDict import UserDict

try:
    import subprocess
except ImportError, e:
    pass

class odict(UserDict):
    def __init__(self, dict = None):
        self._keys = []
        UserDict.__init__(self, dict)

    def __delitem__(self, key):
        UserDict.__delitem__(self, key)
        self._keys.remove(key)

    def __setitem__(self, key, item):
        UserDict.__setitem__(self, key, item)
        if key not in self._keys: self._keys.append(key)

    def clear(self):
        UserDict.clear(self)
        self._keys = []

    def copy(self):
        dict = UserDict.copy(self)
        dict._keys = self._keys[:]
        return dict

    def items(self):
        return zip(self._keys, self.values())

    def keys(self):
        return self._keys

    def popitem(self):
        try:
            key = self._keys[-1]
        except IndexError:
            raise KeyError('dictionary is empty')

        val = self[key]
        del self[key]

        return (key, val)

    def setdefault(self, key, failobj = None):
        UserDict.setdefault(self, key, failobj)
        if key not in self._keys: self._keys.append(key)

    def update(self, dict):
        UserDict.update(self, dict)
        for key in dict.keys():
            if key not in self._keys: self._keys.append(key)

    def values(self):
        return map(self.get, self._keys)


distro_info= odict()
distro_info['Blag Linux']='/etc/blag-release'
distro_info['MythVantage']='/etc/mythvantage-release'
distro_info['Knoppmyth']='/etc/KnoppMyth-version'
distro_info['LinHES']='/etc/LinHES-release'
distro_info['MythDora']='/etc/mythdora-release'
distro_info['Arch Linux']= '/etc/arch-release'
distro_info['Aurox Linux']= '/etc/aurox-release'
distro_info['Conectiva Linux']= '/etc/conectiva-release'
distro_info['Debian GNU/Linux']= '/etc/debian_release'
distro_info['Debian GNU/Linux']= '/etc/debian_version'
distro_info['Fedora Linux']= '/etc/fedora-release'
distro_info['Gentoo Linux']= '/etc/gentoo-release'
distro_info['Linux from Scratch']= '/etc/lfs-release'
distro_info['Mandrake Linux']= '/etc/mandrake-release'
distro_info['Mandriva Linux']= '/etc/mandriva-release'
distro_info['Pardus Linux']= '/etc/pardus-release'
distro_info['Slackware Linux']= '/etc/slackware-release'
distro_info['Solaris/Sparc']= '/etc/release'
distro_info['Sun JDS']= '/etc/sun-release'
distro_info['PLD Linux']= '/etc/pld-release'
distro_info['SUSE Linux']= '/etc/SuSE-release'
distro_info['Yellow Dog Linux']= '/etc/yellowdog-release'
distro_info['Redhat Linux']= '/etc/redhat-release'



def get_os_info():
  if os.name == 'nt':
    win_version = {
      (1, 4, 0): '95',
      (1, 4, 10): '98',
      (1, 4, 90): 'ME',
      (2, 4, 0): 'NT',
      (2, 5, 0): '2000',
      (2, 5, 1): 'XP'
    }[  os.sys.getwindowsversion()[3],
      os.sys.getwindowsversion()[0],
      os.sys.getwindowsversion()[1] ]
    return 'Windows' + ' ' + win_version
  elif os.name =='posix':
    # parse files first and if distro not found run lsb_release executable
    for distro_name in distro_info.keys():
      path_to_file = distro_info[distro_name]
     # print path_to_file
      if os.path.exists(path_to_file):
        fd = open(path_to_file)
        text = fd.read().strip()
        fd.close()
        if path_to_file.endswith('KnoppMyth-version'):
          text = text
        elif path_to_file.endswith('version'):
          text = distro_name + ' ' + text
          #check /etc/issue for signs of ubuntu
          if distro_name == "Debian GNU/Linux":
                fd = open('/etc/issue.net')
                text_u = fd.read().strip()
                fd.close()
                if text.find("Ubuntu"):
                        text = text_u

        elif path_to_file.endswith('aurox-release'):
          text = distro_name
        elif path_to_file.endswith('lfs-release'):
          text = distro_name + ' ' + text
        elif path_to_file.endswith('arch-release'):
          text = "Arch Linux"
        elif path_to_file.endswith('SuSE-release'):
          text = file(path_to_file).read().split('\n')[0].strip()
          retext = re.compile('\(\w*\)$')
          text = retext.sub( '', text ).strip()
        return text

    executable = 'lsb_release'
    #executable = 'lsb_do_not_run_me'

    params = ['--id', '--codename', '--release', '--short']
    for path in os.environ['PATH'].split(':'):
      full_path_to_executable = os.path.join(path, executable)
      if os.path.exists(full_path_to_executable):
        command = [full_path_to_executable] + params
        try:
            try:
                child = subprocess.Popen(command, stdout=subprocess.PIPE, close_fds=True)
            except NameError:
                child = os.system(' '.join(command))
        except OSError:
          print "Warning: Could not run "+executable+", using alternate method."
          break # parse files instead
        (stdoutdata, stderrdata) = child.communicate()
        if child.returncode != 0:
          print "Warning: an error occurred trying to run "+executable+", using alternate method."
          break # parse files instead
        output = stdoutdata.strip().replace('\n', ' ')
        return output
