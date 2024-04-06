# -*- Mode: python; coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*-
"""
Scraper for https://www.musixmatch.com/

ronie
https://github.com/rtcq/syncedlyrics
"""

import requests
import json
import time
import difflib

import os
import sys
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and ronie"
__title__       = "MusixMatchLRC"
__description__ = "Search http://musixmatch.com for lyrics"
__priority__    = "100"
__version__     = "0.1"
__syncronized__ = True

debug = False

class LyricsFetcher:
    def __init__( self ):
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
        tokenpath = os.path.join(utilities.getCacheDir(), 'musixmatch_token')
        if os.path.exists(tokenpath):
            tokenfile = open(tokenpath, 'r')
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
                tokenfile = open(tokenpath, 'w')
                json.dump(tokendata, tokenfile)
                tokenfile.close()
        return self.token

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        self.token = self.get_token()
        if not self.token:
            return False
        artist = lyrics.artist.replace(' ', '+')
        title = lyrics.title.replace(' ', '+')
        search = '%s - %s' % (artist, title)
        try:
            url = self.SEARCH_URL % 'track.search'
            query = [('q', search), ('page_size', '5'), ('page', '1'), ('s_track_rating', 'desc'), ('quorum_factor', '1.0'), ('app_id', 'web-desktop-app-v1.0'), ('usertoken', self.token), ('t', self.current_time)]
            response = requests.get(url, params=query, timeout=10)
            result = response.json()
        except:
            return False
        links = []
        if 'message' in result and 'body' in result["message"] and 'track_list' in result["message"]["body"] and result["message"]["body"]["track_list"]:
            for item in result["message"]["body"]["track_list"]:
                artistname = item['track']['artist_name']
                songtitle = item['track']['track_name']
                trackid = item['track']['track_id']
                if (difflib.SequenceMatcher(None, artist.lower(), artistname.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, title.lower(), songtitle.lower()).ratio() > 0.8):
                    links.append((artistname + ' - ' + songtitle, trackid, artistname, songtitle))
        if len(links) == 0:
            return False
        elif len(links) > 1:
            lyrics.list = links
        for link in links:
            lyr = self.get_lyrics_from_list(link)
            if lyr:
                lyrics.lyrics = lyr
                return True
        return False

    def get_lyrics_from_list(self, link):
        title,trackid,artist,song = link
        try:
            utilities.log(debug, '%s: search track id: %s' % (__title__, trackid))
            url = self.SEARCH_URL % 'track.subtitle.get'
            query = [('track_id', trackid), ('subtitle_format', 'lrc'), ('app_id', 'web-desktop-app-v1.0'), ('usertoken', self.token), ('t', self.current_time)]
            response = requests.get(url, params=query, timeout=10)
            result = response.json()
        except:
            return None
        if 'message' in result and 'body' in result["message"] and 'subtitle' in result["message"]["body"] and 'subtitle_body' in result["message"]["body"]["subtitle"]:
            lyrics = result["message"]["body"]["subtitle"]["subtitle_body"]
            return lyrics

def performSelfTest():
    found = False
    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__
    lyrics.artist = 'Dire Straits'
    lyrics.album = 'Brothers In Arms'
    lyrics.title = 'Money For Nothing'

    fetcher = LyricsFetcher()
    found = fetcher.get_lyrics(lyrics)

    if found:
        utilities.log(True, "Everything appears in order.")
        buildLyrics(lyrics)
        sys.exit(0)

    utilities.log(True, "The lyrics for the test search failed!")
    sys.exit(1)

def buildLyrics(lyrics):
    from lxml import etree
    xml = etree.XML(u'<lyrics></lyrics>')
    etree.SubElement(xml, "artist").text = lyrics.artist
    etree.SubElement(xml, "album").text = lyrics.album
    etree.SubElement(xml, "title").text = lyrics.title
    etree.SubElement(xml, "syncronized").text = 'True' if __syncronized__ else 'False'
    etree.SubElement(xml, "grabber").text = lyrics.source

    lines = lyrics.lyrics.splitlines()
    for line in lines:
        etree.SubElement(xml, "lyric").text = line

    utilities.log(True, utilities.convert_etree(etree.tostring(xml, encoding='UTF-8',
                                                pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'musixmatchlrc.py'
    etree.SubElement(version, "type").text = 'lyrics'
    etree.SubElement(version, "description").text = __description__
    etree.SubElement(version, "version").text = __version__
    etree.SubElement(version, "priority").text = __priority__
    etree.SubElement(version, "syncronized").text = 'True' if __syncronized__ else 'False'

    utilities.log(True, utilities.convert_etree(etree.tostring(version, encoding='UTF-8',
                                                pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def main():
    global debug

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Test grabber with a know good search")
    parser.add_option('-s', "--search", action="store_true", default=False,
                      dest="search", help="Search for lyrics.")
    parser.add_option('-a', "--artist", metavar="ARTIST", default=None,
                      dest="artist", help="Artist of track.")
    parser.add_option('-b', "--album", metavar="ALBUM", default=None,
                      dest="album", help="Album of track.")
    parser.add_option('-n', "--title", metavar="TITLE", default=None,
                      dest="title", help="Title of track.")
    parser.add_option('-f', "--filename", metavar="FILENAME", default=None,
                      dest="filename", help="Filename of track.")
    parser.add_option('-d', '--debug', action="store_true", default=False,
                      dest="debug", help=("Show debug messages"))

    opts, args = parser.parse_args()

    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__

    if opts.debug:
        debug = True

    if opts.version:
        buildVersion()

    if opts.test:
        performSelfTest()

    if opts.artist:
        lyrics.artist = opts.artist
    if opts.album:
        lyrics.album = opts.album
    if opts.title:
        lyrics.title = opts.title
    if opts.filename:
        lyrics.filename = opts.filename

    fetcher = LyricsFetcher()
    if fetcher.get_lyrics(lyrics):
        buildLyrics(lyrics)
        sys.exit(0)
    else:
        utilities.log(True, "No lyrics found for this track")
        sys.exit(1)

if __name__ == '__main__':
    main()
