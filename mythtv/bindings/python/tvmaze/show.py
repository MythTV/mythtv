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
# - added embedded 'akas' and 'images'
#
# ---------------------------------------------------


from __future__ import unicode_literals

from . import utils
from .season import Season
from .episode import Episode
from .person import Crew, Character


class Show(object):
    def __init__(self, data):
        self.score = data.get('score') if 'score' in data else 100
        show = data.get('show') if 'show' in data else data
        self.id = show.get('id')
        self.name = show.get('name')
        self.url = show.get('url')
        self.type = show.get('type')
        self.language = show.get('language')
        self.genres = show.get('genres')
        self.status = show.get('status')
        self.num_episodes = show.get('runtime')
        self.seasons = {}
        self._episode_list = []
        self.specials = {}
        self.cast = []
        self.crew = []
        self.akas = []
        self.series_images = []
        self._handle_embedded(data.get('_embedded'))
        self.premiere_date = utils.convert_date(show.get('premiered'))
        self.official_site = show.get('officialSite')
        self.schedule = show.get('schedule')
        self.rating = show.get('rating')
        self.weight = show.get('weight')
        self.network = show.get('network')
        self.streaming_service = show.get('webChannel')
        self.external_ids = show.get('externals')
        self.images = show.get('image')
        self.summary = utils.strip_tags(show.get('summary'))
        self.links = show.get('_links')

    def _handle_embedded(self, embedded):
        if embedded is None:
            return

        special_num = 1
        if 'seasons' in embedded:
            seasons = [Season(season) for season in embedded.get('seasons')]
            for season in seasons:
                self.seasons[season.number] = season

        if 'seasons' in embedded and 'episodes' in embedded:
            episodes = [Episode(episode) for episode in embedded.get('episodes')]
            for episode in episodes:
                if not episode.special:
                    self.seasons[episode.season].episodes[episode.number] = episode
                else:
                    episode.season = 0
                    episode.number = special_num
                    special_num += 1
                    self.specials[episode.id] = episode

        if 'seasons' not in embedded and 'episodes' in embedded:
            episodes = [Episode(episode) for episode in embedded.get('episodes')]
            for episode in episodes:
                if not episode.special:
                    self._episode_list.append(episode)
                else:
                    episode.season = 0
                    episode.number = special_num
                    special_num += 1
                    self.specials[episode.id] = episode

        self.cast = [Character(c.get('character'), c.get('person')) for c in embedded.get('cast')] if 'cast' in embedded else []
        self.crew = [Crew(c) for c in embedded.get('crew')] if 'crew' in embedded else []

        if 'akas' in embedded:
            self.akas = [Alias(aka) for aka in embedded.get('akas')]

        if 'images' in embedded:
            self.series_images = [i for i in embedded.get('images')]

    def __str__(self):
        return str(self.id) + ': ' + self.name


class Alias:
    def __init__(self, data):
        self.name = data.get('name')
        if data['country'] is not None:
            self.country = data.get('country')
        else:
            self.country = {}
            self.country['name'] = 'Original Country'
            self.country['code'] = 'OG'
            self.country['timezome'] = 'Original Country Timezone'

    def __str__(self):
        return self.country.get('name') + ': ' + self.name
