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
# - added support for requests.Session()
#
# ---------------------------------------------------


from __future__ import unicode_literals

from datetime import datetime
from requests import codes as requestcodes
import sys
import time
from dateutil import parser
from pprint import pprint

from . import endpoints
from .show import Show, Alias
from .episode import Episode
from .season import Season
from .person import Character, Person, Crew
from .embed import Embed


MYTHTV_TVMAZE_API_VERSION = "0.1.0"

# set this to true for showing raw json data
JSONDEBUG = False

# Set an open requests session here
ReqSession = None


def set_session(s):
    global ReqSession
    ReqSession = s


def _query_api(url, params=None):
    if JSONDEBUG:
        print(params)

    http_retries = 0
    res = None
    while http_retries < 2:
        if ReqSession is None:
            if JSONDEBUG:
                print("Using requests directly, without cache")
                print(url)
            import requests
            res = requests.get(url, params)
        else:
            if JSONDEBUG:
                print("Using Request Session %s" % ReqSession)
                print(url)
            res = ReqSession.get(url, params=params)
        if JSONDEBUG:
            print(res.url)
            print(res.request.headers)
        if res.status_code == 429:
            # wait a bit and do a retry once
            if JSONDEBUG:
                print("Rate Limiting, caused by 'HTTP Too Many Requests'")
            if http_retries == 1:
                print('Error: Exiting due to rate limitation')
                sys.exit(1)
            time.sleep(10)
            http_retries += 1
        elif res.status_code != requestcodes.OK:
            print('Page request was unsuccessful: ' + str(res.status_code), res.reason)
            sys.exit(1)
        else:
            if JSONDEBUG:
                print("Successful http request '%s':" % res.url)
            break

    if JSONDEBUG:
        pprint(res.json())
    if res is None:
        return None
    else:
        return res.json()


def search_show(show_name):
    res = _query_api(endpoints.search_show_name, {'q': show_name})
    return [Show(show) for show in res] if res is not None else []


def search_show_best_match(show_name, embed=None):
    embed = Embed(embed)
    res = _query_api(endpoints.search_show_best_match, {'q': show_name, embed.key: embed.value})
    return Show(res) if res is not None else None


def get_show_external(imdb_id=None, tvdb_id=None, tvrage_id=None):
    if len(list(filter(None, [imdb_id, tvdb_id, tvrage_id]))) == 0:
        return None
    if imdb_id is not None:
        return _get_show_external_id('imdb', imdb_id)
    if tvdb_id is not None:
        return _get_show_external_id('thetvdb', tvdb_id)
    if tvrage_id is not None:
        return _get_show_external_id('tvrage', tvrage_id)


def _get_show_external_id(external_name, external_id):
    res = _query_api(endpoints.search_external_show_id, {external_name: external_id})
    return Show(res) if res is not None else None


def get_show(tvmaze_id, populated=False, embed=None):
    embed = Embed(embed) if not populated else Embed(['seasons', 'cast', 'crew', 'akas', 'images'])
    res = _query_api(endpoints.show_information.format(str(tvmaze_id)), {embed.key: embed.value})
    # print(res.keys())
    if populated:
        episodes = [episode for episode in _get_show_episode_list_raw(tvmaze_id, specials=True)]
        # print(episodes)
        res['_embedded']['episodes'] = episodes
    return Show(res) if res is not None else None


def get_show_episode_list(tvmaze_id, specials=False):
    specials = 1 if specials else None
    res = _get_show_episode_list_raw(tvmaze_id, specials)
    return [Episode(episode) for episode in res] if res is not None else []


def _get_show_episode_list_raw(tvmaze_id, specials):
    return _query_api(endpoints.show_episode_list.format(str(tvmaze_id)), {'specials': specials})


def get_show_specials(tvmaze_id):
    res = _query_api(endpoints.show_episode_list.format(str(tvmaze_id)), {'specials': 1})
    specials = [Episode(episode) for episode in res if episode['number'] is None] if res is not None else []
    special_num = 1
    for special in specials:
        special.season = 0
        special.number = special_num
        special_num += 1
    return specials


def get_show_episode(tvmaze_id, season, episode):
    res = _query_api(endpoints.show_episode.format(str(tvmaze_id)), {'season': season, 'number': episode})
    return Episode(res) if res is not None else None


def get_show_episodes_by_date(tvmaze_id, date_input):
    if type(date_input) is str:
        try:
            date = parser.parse(date_input)
        except parser._parser.ParserError:                      ### XXX check this
            return []
    elif type(date_input) is datetime:
        date = date_input
    else:
        return []
    res = _query_api(endpoints.show_episodes_on_date.format(str(tvmaze_id)), {'date': date.isoformat()[:10]})
    return [Episode(episode) for episode in res] if res is not None else []


def get_show_season_list(tvmaze_id):
    res = _query_api(endpoints.show_season_list.format(str(tvmaze_id)))
    return [Season(season) for season in res] if res is not None else []


def get_season_episode_list(tvmaze_season_id):
    res = _query_api(endpoints.season_episode_list.format(str(tvmaze_season_id)))
    # episode['number'] is None when it is classed as a special, for now don't include specials
    return [Episode(episode) for episode in res if episode['number'] is not None] if res is not None else []


def get_show_aliases(tvmaze_id):
    res = _query_api(endpoints.show_alias_list.format(str(tvmaze_id)))
    return [Alias(alias) for alias in res] if res is not None else []


def get_episode_information(tvmaze_episode_id, embed=None):
    embed = Embed(embed)
    res = _query_api(endpoints.episode_information.format(str(tvmaze_episode_id)), {embed.key: embed.value})
    return Episode(res) if res is not None else None


def get_show_cast(tvmaze_id):
    res = _query_api(endpoints.show_cast.format(str(tvmaze_id)))
    return [Character(cast['character'], cast['person']) for cast in res]


def get_show_crew(tvmaze_id):
    res = _query_api(endpoints.show_crew.format(str(tvmaze_id)))
    return [Crew(crew_member) for crew_member in res]
