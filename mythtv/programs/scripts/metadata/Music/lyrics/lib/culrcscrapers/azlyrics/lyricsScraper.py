#-*- coding: UTF-8 -*-
import sys
import re
import requests
import html
import xbmc
import xbmcaddon
from lib.utils import *

__title__ = 'azlyrics'
__priority__ = '230'
__lrc__ = False


class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.url = 'https://www.azlyrics.com/lyrics/%s/%s.html'

    def get_lyrics(self, song):
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = re.sub("[^a-zA-Z0-9]+", "", song.artist).lower().lstrip('the ')
        title = re.sub("[^a-zA-Z0-9]+", "", song.title).lower()
        try:
            req = requests.get(self.url % (artist, title), timeout=10)
            response = req.text
        except:
            return None
        req.close()
        try:
            lyricscode = response.split('t. -->')[1].split('</div')[0]
            lyricstext = html.unescape(lyricscode).replace('<br />', '\n')
            lyr = re.sub('<[^<]+?>', '', lyricstext)
            lyrics.lyrics = lyr
            return lyrics
        except:
            return None
