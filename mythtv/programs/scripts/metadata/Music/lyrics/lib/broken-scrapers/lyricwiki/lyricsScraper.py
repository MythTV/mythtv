#-*- coding: UTF-8 -*-
import sys
import re
import json
import requests
from urllib.error import HTTPError
import urllib.parse
from html.parser import HTMLParser
import xbmc
import xbmcaddon
from lib.utils import *

__title__ = 'lyricwiki'
__priority__ = '200'
__lrc__ = False

LIC_TXT = 'we are not licensed to display the full lyrics for this song at the moment'


class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.url = 'http://lyrics.wikia.com/api.php?func=getSong&artist=%s&song=%s&fmt=realjson'

    def get_lyrics(self, song):
        log('%s: searching lyrics for %s - %s' % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        try:
            req = requests.get(self.url % (urllib.parse.quote(song.artist), urllib.parse.quote(song.title)), timeout=10)
            response = req.text
        except:
            return None
        data = json.loads(response)
        try:
            self.page = data['url']
        except:
            return None
        if not self.page.endswith('action=edit'):
            log('%s: search url: %s' % (__title__, self.page), debug=self.DEBUG)
            try:
                req = requests.get(self.page, timeout=10)
                response = req.text
            except requests.exceptions.HTTPError as error:  # strange... sometimes lyrics are returned with a 404 error
                if error.response.status_code == 404:
                    response = error.response.text
                else:
                    return None
            except:
                return None
            matchcode = re.search("class='lyricbox'>(.*?)<div", response)
            try:
                lyricscode = (matchcode.group(1))
                htmlparser = HTMLParser()
                lyricstext = htmlparser.unescape(lyricscode).replace('<br />', '\n')
                lyr = re.sub('<[^<]+?>', '', lyricstext)
                if LIC_TXT in lyr:
                    return None
                lyrics.lyrics = lyr
                return lyrics
            except:
                return None
        else:
            return None
