#-*- coding: UTF-8 -*-
import sys
import re
import requests
import html
import xbmc
import xbmcaddon
from lib.utils import *

__title__ = 'supermusic'
__priority__ = '250'
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
        artist = song.artist.lower()
        title = song.title.lower()

        try:
            req = requests.post('https://supermusic.cz/najdi.php', data={'hladane': title, 'typhladania': 'piesen', 'fraza': 'off'})
            response = req.text
        except:
            return None
        req.close()
        url = None
        try:
            items = re.search(r'Počet nájdených piesní.+<br><br>(.*)<BR>', response, re.S).group(1)
            for match in re.finditer(r'<a href=(?P<url>"[^"]+?") target="_parent"><b>(?P<artist>.*?)</b></a> - (?P<type>.+?) \(<a href', items):
                matched_url, matched_artist, matched_type = match.groups()
                if matched_type not in ('text', 'akordy a text'):
                    continue
                if matched_artist.lower() == artist:
                    url = matched_url.strip('"')
                    break
        except:
            return None

        if not url:
            return None

        try:
            req = requests.get('https://supermusic.cz/%s' % url)
            response = req.text
            lyr = re.search(r'class=piesen>(.*?)</font>', response, re.S).group(1)
            lyr = re.sub(r'<sup>.*?</sup>', '', lyr)
            lyr = re.sub(r'<br\s*/>\s*', '\n', lyr)
            lyr = re.sub(r'<!--.*?-->', '', lyr, flags=re.DOTALL)
            lyr = re.sub(r'<[^>]*?>', '', lyr, flags=re.DOTALL)
            lyr = lyr.strip('\r\n')
            lyr = html.unescape(lyr)
            lyrics.lyrics = lyr
            return lyrics
        except:
            return None
