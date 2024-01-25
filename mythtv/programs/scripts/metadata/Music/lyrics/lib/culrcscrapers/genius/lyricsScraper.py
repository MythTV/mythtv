#-*- coding: UTF-8 -*-
import sys
import re
import urllib.parse
import requests
import html
import xbmc
import xbmcaddon
import json
import difflib
from lib.utils import *

__title__ = 'genius'
__priority__ = '200'
__lrc__ = False


class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.url = 'http://api.genius.com/search?q=%s%%20%s&access_token=Rq_cyNZ6fUOQr4vhyES6vu1iw3e94RX85ju7S8-0jhM-gftzEvQPG7LJrrnTji11'

    def get_lyrics(self, song):
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            headers = {'user-agent': 'Mozilla/5.0 (Windows NT 10.0; rv:77.0) Gecko/20100101 Firefox/77.0'}
            url = self.url % (urllib.parse.quote(song.artist), urllib.parse.quote(song.title))
            req = requests.get(url, headers=headers, timeout=10)
            response = req.text
        except:
            return None
        data = json.loads(response)
        try:
            name = data['response']['hits'][0]['result']['primary_artist']['name']
            track = data['response']['hits'][0]['result']['title']
            if (difflib.SequenceMatcher(None, song.artist.lower(), name.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, song.title.lower(), track.lower()).ratio() > 0.8):
                self.page = data['response']['hits'][0]['result']['url']
            else:
                return None
        except:
            return None
        log('%s: search url: %s' % (__title__, self.page), debug=self.DEBUG)
        try:
            headers = {'user-agent': 'Mozilla/5.0 (Windows NT 10.0; rv:77.0) Gecko/20100101 Firefox/77.0'}
            req = requests.get(self.page, headers=headers, timeout=10)
            response = req.text
        except:
            return None
        response = html.unescape(response)
        matchcode = re.findall('class="Lyrics__Container.*?">(.*?)</div><div', response, flags=re.DOTALL)
        try:
            lyricscode = ""
            for matchCodeItem in matchcode:
                lyricscode = lyricscode + matchCodeItem
            lyr1 = re.sub('<br/>', '\n', lyricscode)
            lyr2 = re.sub('<[^<]+?>', '', lyr1)
            lyr3 = lyr2.replace('\\n','\n').strip()
            if not lyr3 or lyr3 == '[Instrumental]' or lyr3.startswith('Lyrics for this song have yet to be released'):
                return None
            lyrics.lyrics = lyr3
            return lyrics
        except:
            return None
