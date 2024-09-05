#-*- coding: UTF-8 -*-
'''
Scraper for https://www.rclyricsband.com/
'''

import requests
import re
import html
import difflib
from bs4 import BeautifulSoup
from lib.utils import *

__title__ = "RCLyricsBand"
__priority__ = '130'
__lrc__ = True

UserAgent = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/87.0.4280.88 Safari/537.36"}

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.SEARCH_URL = 'https://rclyricsband.com/'
        self.LYRIC_URL = 'https://rclyricsband.com/%s'


    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = song.artist
        title = song.title
        try:
            url = self.SEARCH_URL
            searchdata = {}
            searchdata['search'] = '%s %s' % (artist, title)
            search = requests.post(url, data=searchdata, headers=UserAgent, timeout=10)
            response = search.text
        except:
            return None
        links = []
        soup = BeautifulSoup(response, 'html.parser')
        for link in soup.find_all('a', {'class': 'song_search'}):
            if link.string:
                foundsong = link.string.split(' - ')[0]
                foundartist = link.string.split(' - ')[-1]
                if (difflib.SequenceMatcher(None, artist.lower(), foundartist.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, title.lower(), foundsong.lower()).ratio() > 0.8):
                    links.append((foundartist + ' - ' + foundsong, self.LYRIC_URL % link.get('href'), foundartist, foundsong))
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
            search = requests.get(url, headers=UserAgent, timeout=10)
            response = search.text
        except:
            return None
        matchcode = re.search("lrc_text_format'>(.*?)</p", response, flags=re.DOTALL)
        if matchcode:
            lyricscode = (matchcode.group(1))
            cleanlyrics = re.sub('<br>', '\n', lyricscode)
            cleanlyrics = html.unescape(cleanlyrics)
            return cleanlyrics
