#-*- coding: UTF-8 -*-
"""
Scraper for https://xiami.com

Taxigps
"""

import urllib.parse
import socket
import re
import difflib
import json
import chardet
import requests
from utilities import *

__title__ = "Xiami"
__priority__ = '110'
__lrc__ = True

UserAgent = 'Mozilla/5.0 (Windows NT 10.0; WOW64; rv:51.0) Gecko/20100101 Firefox/51.0'

socket.setdefaulttimeout(10)

class LyricsFetcher:
    def __init__( self ):
        self.LIST_URL = 'https://www.xiami.com/search?key=%s'
        self.SONG_URL = 'https://www.xiami.com/song/playlist/id/%s/object_name/default/object_id/0'
        self.session = requests.Session()

    def get_lyrics(self, song):
        log( "%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title))
        lyrics = Lyrics()
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        keyword = "%s %s" % (song.title, song.artist)
        url = self.LIST_URL % (urllib.parse.quote(keyword))
        try:
            response = self.session.get(url, headers={'User-Agent': UserAgent, 'Referer': 'https://www.xiami.com/play'})
            result = response.text
        except:
            log( "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return None
        match = re.compile('<td class="chkbox">.+?value="(.+?)".+?href="//www.xiami.com/song/[^"]+" title="([^"]+)".*?href="//www.xiami.com/artist/[^"]+" title="([^"]+)"', re.DOTALL).findall(result)
        links = []
        for x in match:
            title = x[1]
            artist = x[2]
            if (difflib.SequenceMatcher(None, song.artist.lower(), artist.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, song.title.lower(), title.lower()).ratio() > 0.8):
                links.append( ( artist + ' - ' + title, x[0], artist, title ) )
        if len(links) == 0:
            return None
        elif len(links) > 1:
            lyrics.list = links
        lyr = self.get_lyrics_from_list(links[0])
        if not lyr:
            return None
        lyrics.lyrics = lyr
        return lyrics

    def get_lyrics_from_list(self, link):
        title,id,artist,song = link
        try:
            response = self.session.get(self.SONG_URL % (id), headers={'User-Agent': UserAgent, 'Referer': 'https://www.xiami.com/play'})
            result = response.text
            data = json.loads(result)
            if 'data' in data and 'trackList' in data['data'] and data['data']['trackList'] and 'lyric' in data['data']['trackList'][0] and data['data']['trackList'][0]['lyric']:
                url = data['data']['trackList'][0]['lyric']
        except:
            log( "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return
        try:
            response = self.session.get(url, headers={'User-Agent': UserAgent, 'Referer': 'https://www.xiami.com/play'})
            lyrics = response.content
        except:
            log( "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return
        enc = chardet.detect(lyrics)
        lyrics = lyrics.decode(enc['encoding'], 'ignore')
        return lyrics
