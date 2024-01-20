# -*- Mode: python; coding: utf-8; tab-width: 8; indent-tabs-mode: t; -*-
"""
Scraper for https://supermusic.cz

Jose Riha
"""

import re
import requests
import html

import os
import sys
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and Jose Riha"
__title__       = "SuperMusic"
__description__ = "Search https://supermusic.cz for lyrics"
__priority__    = "250"
__version__     = "0.1"
__syncronized__ = False

debug = False

class LyricsFetcher:
    def __init__( self ):
        return

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        artist = lyrics.artist.lower()
        title = lyrics.title.lower()

        try:
            req = requests.post('https://supermusic.cz/najdi.php', data={'hladane': title, 'typhladania': 'piesen', 'fraza': 'off'})
            response = req.text
        except:
            return False
        req.close()
        url = None
        try:
            items = re.search(r'Počet nájdených piesní.+<br><br>(.*)<BR>', response, re.S).group(1)
            for match in re.finditer(r'<a href=(?P<url>"[^"]+?") target="_parent"><b>(?P<artist>.*?)</b></a> - (?P<type>.+?) \(<a href', items):
                matched_url, matched_artist, matched_type = match.groups()
                if matched_type not in ('text', 'akordy a text'):
                    continue
                if matched_artist.lower() == artist:
                    url = matched_url.strip('"')
                    break
        except:
            return False
        print(url)
        if not url:
            return False

        try:
            req = requests.get('https://supermusic.cz/%s' % url)
            response = req.text
            lyr = re.search(r'class=piesen>(.*?)</font>', response, re.S).group(1)
            lyr = re.sub(r'<sup>.*?</sup>', '', lyr)
            lyr = re.sub(r'<br\s*/>\s*', '\n', lyr)
            lyr = re.sub(r'<!--.*?-->', '', lyr, flags=re.DOTALL)
            lyr = re.sub(r'<[^>]*?>', '', lyr, flags=re.DOTALL)
            lyr = lyr.strip('\r\n')
            lyr = html.unescape(lyr)
            lyrics.lyrics = lyr
            return True
        except:
            return False

def performSelfTest():
    found = False
    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__
    lyrics.artist = 'Karel Gott'
    lyrics.album = ''
    lyrics.title = 'Trezor'

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
    etree.SubElement(version, "command").text = 'supermusic.py'
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
