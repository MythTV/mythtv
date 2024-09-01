#-*- coding: UTF-8 -*-
'''
Scraper for https://www.musixmatch.com/

musixmatch
'''

import os
import requests
import re
import random
import difflib
import html
from bs4 import BeautifulSoup
from lib.utils import *

__title__ = "musixmatch"
__priority__ = '210'
__lrc__ = False

headers = {}
headers['User-Agent'] = 'Mozilla/5.0 (Windows NT 10.0; WOW64; rv:51.0) Gecko/20100101 Firefox/51.0'

# search is not possible as it requires javascript, only direct access to the lyrics work.

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.SEARCH_URL = 'https://www.musixmatch.com/search?query='
        self.LYRIC_URL = 'https://www.musixmatch.com/lyrics/%s/%s'

    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = song.artist.replace("'", '').replace('!', '').replace('?', '').replace('"', '').replace('/', '').replace('.', '').replace('&', '').replace(',', '').replace('(', '').replace(')', '').replace(' ', '-')
        title = song.title.replace("'", '').replace('!', '').replace('?', '').replace('"', '').replace('/', '').replace('.', '').replace('&', '').replace(',', '').replace('(', '').replace(')', '').replace(' ', '-')
        url = self.LYRIC_URL % (artist, title)
        try:
            log('%s: search url: %s' % (__title__, url), debug=self.DEBUG)
            search = requests.get(url, headers=headers, timeout=10)
            response = search.text
        except:
            return None
        matchcode = re.search('Lyrics of (.*?)Writer\(s\): ', response, flags=re.DOTALL)
        if matchcode:
            lyricscode = (matchcode.group(1))
            lyr = re.sub('<[^<]+?>', '\n', lyricscode)
            lyr = html.unescape(lyr)
            lyrics.lyrics = lyr.replace('\n\n\n\n', '\n')
            return lyrics
        return None

'''
    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = song.artist.replace(' ', '+')
        title = song.title.replace(' ', '+')
        search = '%s+%s' % (artist, title)
        try:
            url = self.SEARCH_URL + search
            response = requests.get(url, headers=headers, timeout=10)
            result = response.text
        except:
            return None
        links = []
        soup = BeautifulSoup(result, 'html.parser')
        for item in soup.find_all('li', {'class': 'showArtist'}):
            artistname = item.find('a', {'class': 'artist'}).get_text()
            songtitle = item.find('a', {'class': 'title'}).get_text()
            url = item.find('a', {'class': 'title'}).get('href')
            if (difflib.SequenceMatcher(None, artist.lower(), artistname.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, title.lower(), songtitle.lower()).ratio() > 0.8):
                links.append((artistname + ' - ' + songtitle, self.LYRIC_URL + url, artistname, songtitle))
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
            response = requests.get(url, headers=headers, timeout=10)
            result = response.text
        except:
            return None
        soup = BeautifulSoup(result, 'html.parser')
        lyr = soup.find_all('span', {'class': 'lyrics__content__ok'})
        if lyr:
            lyrics = ''
            for part in lyr:
                lyrics = lyrics + part.get_text() + '\n'
            return lyrics
        else:
            lyr = soup.find_all('span', {'class': 'lyrics__content__error'})
            if lyr:
                lyrics = ''
                for part in lyr:
                    lyrics = lyrics + part.get_text() + '\n'
                return lyrics
'''
