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

from xml.etree import ElementTree as ET

class OverlayParser:
    def __init__(self):
        pass

    def parse(self, text):
        self._overlay_name_to_url_map = {}
        for i in ET.XML(text):
            try:
                overlay_url = i.attrib['src']
                overlay_name = i.attrib['name']
                self._overlay_name_to_url_map[overlay_name] = overlay_url
            except KeyError:
                pass

    def get(self):
        return self._overlay_name_to_url_map
