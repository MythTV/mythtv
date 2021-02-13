# -*- coding: UTF-8 -*-

#  Copyright (c) 2020 Lachlan Mackenzie
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.


# ---------------------------------------------------
# Roland Ernst
# Changes implemented for MythTV:
# - added method utils.convert_date
#
# ---------------------------------------------------


from __future__ import unicode_literals

from . import utils


class Episode(object):
    def __init__(self, data):
        self.id = data.get('id')
        self.url = data.get('url')
        self.name = data.get('name')
        self.season = data.get('season')
        self.number = data.get('number')
        self.airdate = utils.convert_date(data.get('airdate'))
        self.airtime = data.get('airtime')
        self.timestamp = data.get('airstamp')
        self.duration = data.get('runtime')
        self.images = data.get('image')
        self.summary = utils.strip_tags(data.get('summary'))
        self.links = data.get('_links')
        self.special = self.number == 0

    def __str__(self):
        return 'S{}E{} {}'.format(self.season, self.number, self.name)
        # return f'S{self.season}E{self.number} {self.name}' if not self.special else f'Special: {self.name}'
