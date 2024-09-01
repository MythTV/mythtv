#-*- coding: UTF-8 -*-
import re
import requests
import urllib.parse
import difflib
from bs4 import BeautifulSoup
from lib.utils import *

__title__ = 'lyricscom'
__priority__ = '240'
__lrc__ = False

UserAgent = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36"}

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.url = 'https://www.lyrics.com/serp.php?st=%s&qtype=2'

    def get_lyrics(self, song):
        sess = requests.Session()
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            request = sess.get(self.url % urllib.parse.quote_plus(song.artist), headers=UserAgent, timeout=10)
            response = request.text
        except:
            return
        soup = BeautifulSoup(response, 'html.parser')
        url = ''
        for link in soup.find_all('a'):
            if link.string and link.get('href').startswith('artist/'):
                url = 'https://www.lyrics.com/' + link.get('href')
                break
        if url:
            try:
                req = sess.get(url, headers=UserAgent, timeout=10)
                resp = req.text
            except:
                return
            soup = BeautifulSoup(resp, 'html.parser')
            url = ''
            for link in soup.find_all('a'):
                if link.string and (difflib.SequenceMatcher(None, link.string.lower(), song.title.lower()).ratio() > 0.8):
                    url = 'https://www.lyrics.com' + link.get('href')
                    break
            if url:
                try:
                    req2 = sess.get(url, headers=UserAgent, timeout=10)
                    resp2 = req2.text
                except:
                    return
                matchcode = re.search('<pre.*?>(.*?)</pre>', resp2, flags=re.DOTALL)
                if matchcode:
                    lyricscode = (matchcode.group(1))
                    lyr = re.sub('<[^<]+?>', '', lyricscode)
                    lyrics.lyrics = lyr.replace('\\n','\n')
                    return lyrics
