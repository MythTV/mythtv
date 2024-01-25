#-*- coding: UTF-8 -*-
'''
Scraper for http://music.163.com/

osdlyrics
'''

import os
import requests
import re
import random
import difflib
from lib.utils import *

__title__ = "Music163"
__priority__ = '120'
__lrc__ = True

headers = {}
headers['User-Agent'] = 'Mozilla/5.0 (Windows NT 10.0; WOW64; rv:51.0) Gecko/20100101 Firefox/51.0'


class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.SEARCH_URL = 'http://music.163.com/api/search/get'
        self.LYRIC_URL = 'http://music.163.com/api/song/lyric'

    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = song.artist.replace(' ', '+')
        title = song.title.replace(' ', '+')
        search = '?s=%s+%s&type=1' % (artist, title)
        try:
            url = self.SEARCH_URL + search
            response = requests.get(url, headers=headers, timeout=10)
            result = response.json()
        except:
            return None
        links = []
        if 'result' in result and 'songs' in result['result']:
            for item in result['result']['songs']:
                artists = "+&+".join([a["name"] for a in item["artists"]])
                if (difflib.SequenceMatcher(None, artist.lower(), artists.lower()).ratio() > 0.6) and (difflib.SequenceMatcher(None, title.lower(), item['name'].lower()).ratio() > 0.8):
                    links.append((artists + ' - ' + item['name'], self.LYRIC_URL + '?id=' + str(item['id']) + '&lv=-1&kv=-1&tv=-1', artists, item['name']))
        if len(links) == 0:
            return None
        elif len(links) > 1:
            lyrics.list = links
        for link in links:
            lyr = self.get_lyrics_from_list(link)
            if lyr and lyr.startswith('['):
                lyrics.lyrics = lyr
                return lyrics
        return None

    def get_lyrics_from_list(self, link):
        title,url,artist,song = link
        try:
            log('%s: search url: %s' % (__title__, url), debug=self.DEBUG)
            response = requests.get(url, headers=headers, timeout=10)
            result = response.json()
        except:
            return None
        if 'lrc' in result:
            return result['lrc']['lyric']
