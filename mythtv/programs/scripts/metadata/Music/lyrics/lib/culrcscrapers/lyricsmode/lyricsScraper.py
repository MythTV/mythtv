#-*- coding: UTF-8 -*-
import sys
import requests
import urllib.parse
import re
from lib.utils import *

__title__ = 'lyricsmode'
__priority__ = '220'
__lrc__ = False

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']

    def get_lyrics(self, song):
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = deAccent(song.artist)
        title = deAccent(song.title)
        url = 'http://www.lyricsmode.com/lyrics/%s/%s/%s.html' % (artist.lower()[:1], artist.lower().replace('&','and').replace(' ','_'), title.lower().replace('&','and').replace(' ','_'))
        result = self.direct_url(url)
        if not result:
            result = self.search_url(artist, title)
        if result:
            lyr = result.split('style="position: relative;">')[1].split('<div')[0]
            lyrics.lyrics = lyr.replace('<br />', '')
            return lyrics

    def direct_url(self, url):
        try:
            log('%s: direct url: %s' % (__title__, url), debug=self.DEBUG)
            song_search = requests.get(url, timeout=10)
            response = song_search.text
            if response.find('lyrics_text') >= 0:
                return response
        except:
            log('error in direct url', debug=self.DEBUG)

    def search_url(self, artist, title):
        try:
            url = 'http://www.lyricsmode.com/search.php?search=' + urllib.parse.quote_plus(artist.lower() + ' ' + title.lower())
            log('%s: search url: %s' % (__title__, url), debug=self.DEBUG)
            song_search = requests.get(url, timeout=10)
            response = song_search.text
            matchcode = re.search('lm-list__cell-title">.*?<a href="(.*?)" class="lm-link lm-link--primary', response, flags=re.DOTALL)
            try:
                url = 'http://www.lyricsmode.com' + (matchcode.group(1))
                result = self.direct_url(url)
                if result:
                    return result
            except:
                return
        except:
            log('error in search url', debug=self.DEBUG)
