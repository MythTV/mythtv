#-*- coding: UTF-8 -*-
'''
Scraper for https://www.musixmatch.com/

musixmatchlrc

https://github.com/rtcq/syncedlyrics
'''

import requests
import json
import time
import difflib
import xbmcvfs
from lib.utils import *

__title__ = "musixmatchlrc"
__priority__ = '100'
__lrc__ = True


class LyricsFetcher:
    def __init__(self, *args, **kwargs):
        self.DEBUG = kwargs['debug']
        self.settings = kwargs['settings']
        self.SEARCH_URL = 'https://apic-desktop.musixmatch.com/ws/1.1/%s'
        self.session = requests.Session()
        self.session.headers.update(
            {
                "authority": "apic-desktop.musixmatch.com",
                "cookie": "AWSELBCORS=0; AWSELB=0",
                "User-Agent": "Mozilla/5.0 (Windows NT 10.0; WOW64; rv:51.0) Gecko/20100101 Firefox/51.0",
            }
        )
        self.current_time = int(time.time())

    def get_token(self):
        self.token = ''
        tokenpath = os.path.join(PROFILE, 'musixmatch_token')
        if xbmcvfs.exists(tokenpath):
            tokenfile = xbmcvfs.File(tokenpath)
            tokendata = json.load(tokenfile)
            tokenfile.close()
            cached_token = tokendata.get("token")
            expiration_time = tokendata.get("expiration_time")
            if cached_token and expiration_time and self.current_time < expiration_time:
                self.token = cached_token
        if not self.token:
            try:
                url = self.SEARCH_URL % 'token.get'
                query = [('user_language', 'en'), ('app_id', 'web-desktop-app-v1.0'), ('t', self.current_time)]
                response = self.session.get(url, params=query, timeout=10)
                result = response.json()
            except:
                return None
            if 'message' in result and 'body' in result["message"] and 'user_token' in result["message"]["body"]:
                self.token = result["message"]["body"]["user_token"]
                expiration_time = self.current_time + 600
                tokendata = {}
                tokendata['token'] = self.token
                tokendata['expiration_time'] = expiration_time
                tokenfile = xbmcvfs.File(tokenpath, 'w')
                json.dump(tokendata, tokenfile)
                tokenfile.close()
        return self.token

    def get_lyrics(self, song):
        self.token = self.get_token()
        if not self.token:
            return
        log("%s: searching lyrics for %s - %s" % (__title__, song.artist, song.title), debug=self.DEBUG)
        lyrics = Lyrics(settings=self.settings)
        lyrics.song = song
        lyrics.source = __title__
        lyrics.lrc = __lrc__
        artist = song.artist.replace(' ', '+')
        title = song.title.replace(' ', '+')
        search = '%s - %s' % (artist, title)
        try:
            url = self.SEARCH_URL % 'track.search'
            query = [('q', search), ('page_size', '5'), ('page', '1'), ('app_id', 'web-desktop-app-v1.0'), ('usertoken', self.token), ('t', self.current_time)]
            response = requests.get(url, params=query, timeout=10)
            result = response.json()
        except:
            return None
        links = []
        if 'message' in result and 'body' in result["message"] and 'track_list' in result["message"]["body"] and result["message"]["body"]["track_list"]:
            for item in result["message"]["body"]["track_list"]:
                artistname = item['track']['artist_name']
                songtitle = item['track']['track_name']
                trackid = item['track']['track_id']
                if (difflib.SequenceMatcher(None, artist.lower(), artistname.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, title.lower(), songtitle.lower()).ratio() > 0.8):
                    links.append((artistname + ' - ' + songtitle, trackid, artistname, songtitle))
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
        title,trackid,artist,song = link
        try:
            log('%s: search track id: %s' % (__title__, trackid), debug=self.DEBUG)
            url = self.SEARCH_URL % 'track.subtitle.get'
            query = [('track_id', trackid), ('subtitle_format', 'lrc'), ('app_id', 'web-desktop-app-v1.0'), ('usertoken', self.token), ('t', self.current_time)]
            response = requests.get(url, params=query, timeout=10)
            result = response.json()
        except:
            return None
        if 'message' in result and 'body' in result["message"] and 'subtitle' in result["message"]["body"] and 'subtitle_body' in result["message"]["body"]["subtitle"]:
            lyrics = result["message"]["body"]["subtitle"]["subtitle_body"]
            return lyrics
