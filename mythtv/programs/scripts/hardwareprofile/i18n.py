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

import locale
try:
    locale.setlocale(locale.LC_ALL, '')
except locale.Error:
    locale.setlocale(locale.LC_ALL, 'C')
#print locale.LC_ALL

import os
import gettext

try:
    if os.path.isdir('po'):
        # if there is a local directory called 'po' use it so we can test
        # without installing
        #t = gettext.translation('smolt', 'po', fallback = True)
        t = gettext.translation('smolt', '/usr/share/locale/', fallback = True)
    else:
        t = gettext.translation('smolt', '/usr/share/locale/', fallback = True)
except IndexError:
    locale.setlocale(locale.LC_ALL, 'C')
    t = gettext.translation('smolt', '/usr/share/locale/', fallback=True, languages='en_US')

_ = t.ugettext
