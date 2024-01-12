#-*- coding: UTF-8 -*-
'''
Scraper for http://www.baidu.com

ronie
'''

import urllib.request
import socket
import re
import chardet
import difflib
from utilities import *

__title__ = 'Baidu'
__priority__ = '130'
__lrc__ = True

socket.setdefaulttimeout(10)

class LyricsFetcher:
    def __init__(self):
        self.BASE_URL = 'http://music.baidu.com/search/lrc?key=%s-%s'
        self.LRC_URL = 'http://music.baidu.com%s'

    def get_lyrics(self, song):
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title))
        lyrics = Lyrics()
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            url = self.BASE_URL % (song.title, song.artist)
            data = urllib.request.urlopen(url).read().decode('utf-8')
            songmatch = re.search('song-title.*?<em>(.*?)</em>', data, flags=re.DOTALL)
            track = songmatch.group(1)
            artistmatch = re.search('artist-title.*?<em>(.*?)</em>', data, flags=re.DOTALL)
            name = artistmatch.group(1)
            urlmatch = re.search("down-lrc-btn.*?':'(.*?)'", data, flags=re.DOTALL)
            found_url = urlmatch.group(1)
            if (difflib.SequenceMatcher(None, song.artist.lower(), name.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, song.title.lower(), track.lower()).ratio() > 0.8):
                lyr = urllib.request.urlopen(self.LRC_URL % found_url).read()
            else:
                return
        except:
            return

        enc = chardet.detect(lyr)
        lyr = lyr.decode(enc['encoding'], 'ignore')
        lyrics.lyrics = lyr
        return lyrics
