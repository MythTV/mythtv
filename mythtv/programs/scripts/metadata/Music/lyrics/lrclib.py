#-*- coding: UTF-8 -*-
'''
Scraper for https://lrclib.net/

lrclib

https://github.com/rtcq/syncedlyrics
'''

import requests
import difflib

import sys
from optparse import OptionParser
from common import *

__author__      = "Paul Harrison and ronie"
__title__       = "LrcLib"
__description__ = "Search https://lrclib.net for lyrics"
__priority__    = "110"
__version__     = "0.1"
__syncronized__ = True


debug = False

class LyricsFetcher:
    def __init__( self ):
        self.SEARCH_URL = 'https://lrclib.net/api/search?q=%s-%s'
        self.LYRIC_URL = 'https://lrclib.net/api/get/%i'

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        try:
            url = self.SEARCH_URL % (lyrics.artist, lyrics.title)
            response = requests.get(url, timeout=10)
            result = response.json()
        except:
            return False
        links = []
        for item in result: 
            artistname = item['artistName']
            songtitle = item['name']
            songid = item['id']
            if (difflib.SequenceMatcher(None, lyrics.artist.lower(), artistname.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, lyrics.title.lower(), songtitle.lower()).ratio() > 0.8):
                links.append((artistname + ' - ' + songtitle, self.LYRIC_URL % songid, artistname, songtitle))
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
        title,url,artist,song = link
        try:
            utilities.log(debug, '%s: search url: %s' % (__title__, url))
            response = requests.get(url, timeout=10)
            result = response.json()
        except:
            return None
        if 'syncedLyrics' in result:
            lyrics = result['syncedLyrics']
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
    etree.SubElement(version, "command").text = 'lrclib.py'
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
        utilities.log(True,  "No lyrics found for this track")
        sys.exit(1)


if __name__ == '__main__':
    main()
