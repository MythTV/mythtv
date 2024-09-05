#-*- coding: UTF-8 -*-
'''
Scraper for https://www.megalobiz.com/

megalobiz

https://github.com/rtcq/syncedlyrics
'''

import requests
import re
from bs4 import BeautifulSoup
from lib.utils import *

__title__ = "Megalobiz"
__priority__ = '140'
__lrc__ = True


class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.SEARCH_URL = 'https://www.megalobiz.com/search/all?qry=%s-%s&searchButton.x=0&searchButton.y=0'
        self.LYRIC_URL = 'https://www.megalobiz.com/%s'

    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            url = self.SEARCH_URL % (song.artist, song.title)
            response = requests.get(url, timeout=10)
            result = response.text
        except:
            return None
        links = []
        soup = BeautifulSoup(result, 'html.parser')
        for link in soup.find_all('a'):
            if link.get('href') and link.get('href').startswith('/lrc/maker/'):
                linktext = link.text.replace('_', ' ').strip()
                if song.artist.lower() in linktext.lower() and song.title.lower() in linktext.lower():
                    links.append((linktext, self.LYRIC_URL % link.get('href'), song.artist, song.title))
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
            result = response.text
        except:
            return None
        matchcode = re.search('span id="lrc_[0-9]+_lyrics">(.*?)</span', result, flags=re.DOTALL)
        if matchcode:
            lyricscode = (matchcode.group(1))
            cleanlyrics = re.sub('<[^<]+?>', '', lyricscode)
            return cleanlyrics
