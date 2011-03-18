#!/usr/bin/python

# Mirrorselect 1.x
# Tool for selecting Gentoo source and rsync mirrors.
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

from xml.etree import ElementTree as ET

MIRRORS_3_XML = 'http://www.gentoo.org/main/en/mirrors3.xml'

class MirrorParser3:
    def __init__(self):
        self._reset()

    def _reset(self):
        self._dict = {}

    def parse(self, text):
        self._reset()
        for mirrorgroup in ET.XML(text):
            for mirror in mirrorgroup:
                name = ''
                for e in mirror:
                    if e.tag == 'name':
                        name = e.text
                    if e.tag == 'uri':
                        uri = e.text
                        self._dict[uri] = name

    def tuples(self):
        return [(url, name) for url, name in self._dict.items()]

    def uris(self):
        return [url for url, name in self._dict.items()]

if __name__ == '__main__':
    import urllib
    parser = MirrorParser3()
    parser.parse(urllib.urlopen(MIRRORS_3_XML).read())
    print '===== tuples'
    print parser.tuples()
    print '===== uris'
    print parser.uris()
