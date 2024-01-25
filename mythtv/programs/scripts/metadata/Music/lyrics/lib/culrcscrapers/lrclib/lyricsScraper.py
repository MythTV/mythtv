#-*- coding: UTF-8 -*-
'''
Scraper for https://lrclib.net/

lrclib

https://github.com/rtcq/syncedlyrics
'''

import requests
import difflib
from lib.utils import *

__title__ = "lrclib"
__priority__ = '110'
__lrc__ = True


class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.SEARCH_URL = 'https://lrclib.net/api/search?q=%s-%s'
        self.LYRIC_URL = 'https://lrclib.net/api/get/%i'

    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            url = self.SEARCH_URL % (song.artist, song.title)
            response = requests.get(url, timeout=10)
            result = response.json()
        except:
            return None
        links = []
        for item in result: 
            artistname = item['artistName']
            songtitle = item['name']
            songid = item['id']
            if (difflib.SequenceMatcher(None, song.artist.lower(), artistname.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, song.title.lower(), songtitle.lower()).ratio() > 0.8):
                links.append((artistname + ' - ' + songtitle, self.LYRIC_URL % songid, artistname, songtitle))
        if len(links) == 0:
            return None
        elif len(links) > 1:
            lyrics.list = links
        for link in links:
            lyr = self.get_lyrics_from_list(link)
            if lyr:
                lyrics.lyrics = lyr
                return lyrics
        return None

    def get_lyrics_from_list(self, link):
        title,url,artist,song = link
        try:
            log('%s: search url: %s' % (__title__, url), debug=self.DEBUG)
            response = requests.get(url, timeout=10)
            result = response.json()
        except:
            return None
        if 'syncedLyrics' in result:
            lyrics = result['syncedLyrics']
            return lyrics
