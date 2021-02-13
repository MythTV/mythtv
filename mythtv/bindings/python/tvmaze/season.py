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

import string
from . import utils


class Season(object):
    def __init__(self, data, special=False):
            self.id = data.get('id')
            self.url = data.get('url')
            self.number = data.get('number')
            self.name = data.get('name')
            self.num_episodes = data.get('episodeOrder')
            self.episodes = {}
            self.premiere_date = utils.convert_date(data.get('premiereDate'))
            self.end_date = data.get('endDate')
            self.network = data.get('network')
            self.streaming_service = data.get('webChannel')
            self.images = data.get('image')
            self.summary = ""
            self.summary = utils.strip_tags(data.get('summary'))
            self.links = data.get('_links')

    def __str__(self):
        return string.capwords(' '.join(self.url.split('/')[-1].split('-')))
