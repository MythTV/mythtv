#-*- coding: UTF-8 -*-
'''
Scraper for https://www.lyricsify.com/
'''

import requests
import re
import difflib
from bs4 import BeautifulSoup
from lib.utils import *

__title__ = "Lyricsify"
__priority__ = '130'
__lrc__ = True

UserAgent = {"Host": "www.lyricsify.com", "User-Agent": "Mozilla/5.0 (X11; Linux x86_64; rv:126.0) Gecko/20100101 Firefox/126.0", "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8", "Accept-Language": "en-US,en;q=0.5", "Accept-Encoding": "gzip, deflate, br, zstd", "DNT": "1", "Alt-Used": "www.lyricsify.com", "Connection": "keep-alive", "Upgrade-Insecure-Requests": "1", "Sec-Fetch-Dest": "document", "Sec-Fetch-Mode": "navigate", "Sec-Fetch-Site": "none", "Sec-Fetch-User": "?1", "Priority": "u=1"}

# lyricsify uses captcha's & cloudflare protection for the search option, only direct lyrics access works

class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.SEARCH_URL = 'https://www.lyricsify.com/lyrics/%s/%s'
        self.LYRIC_URL = 'https://www.lyricsify.com%s'

    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = song.artist.replace("'", '').replace('!', '').replace('?', '').replace('"', '').replace('/', '').replace('.', '').replace('&', '').replace(',', '').replace('(', '').replace(')', '').replace(' ', '-')
        title = song.title.replace("'", '').replace('!', '').replace('?', '').replace('"', '').replace('/', '').replace('.', '').replace('&', '').replace(',', '').replace('(', '').replace(')', '').replace(' ', '-')
        url = self.SEARCH_URL % (artist.lower(), title.lower())
        try:
            log('%s: search url: %s' % (__title__, url), debug=self.DEBUG)
            search = requests.get(url, headers=UserAgent, timeout=10)
            response = search.text
        except:
            return None
        matchcode = re.search('details">(.*?)</div', response, flags=re.DOTALL)
        if matchcode:
            lyricscode = (matchcode.group(1))
            lyr = re.sub('<[^<]+?>', '', lyricscode)
            lyrics.lyrics = lyr
            return lyrics
        return None

'''
    def get_lyrics(self, song):
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = song.artist.replace(' ', '-')
        title = song.title.replace(' ', '-')
        try:
            url = self.SEARCH_URL % (artist, title)
            search = requests.get(url, headers=UserAgent, timeout=10)
            response = search.text
        except:
            return None
        links = []
        soup = BeautifulSoup(response, 'html.parser')
        for link in soup.find_all('a'):
            if link.string and link.get('href').startswith('/lrc/'):
                foundartist = link.string.split(' - ', 1)[0]
                # some links don't have a proper 'artist - title' format
                try:
                    foundsong = link.string.split(' - ', 1)[1].rstrip('.lrc')
                except:
                    continue
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
        matchcode = re.search('/h3>(.*?)</div', response, flags=re.DOTALL)
        if matchcode:
            lyricscode = (matchcode.group(1))
            cleanlyrics = re.sub('<[^<]+?>', '', lyricscode)
            return cleanlyrics
'''
