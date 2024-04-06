#-*- coding: UTF-8 -*-
"""
Scraper for http://www.genius.com

taxigps
"""
import sys
import re
import urllib.parse
import requests
import html
import difflib
import json

from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and ronie"
__title__       = "Genius"
__description__ = "Search http://www.genius.com for lyrics"
__priority__    = "200"
__version__     = "0.1"
__syncronized__ = False


debug = False

class LyricsFetcher:
    def __init__( self ):
        self.url = 'http://api.genius.com/search?q=%s%%20%s&access_token=Rq_cyNZ6fUOQr4vhyES6vu1iw3e94RX85ju7S8-0jhM-gftzEvQPG7LJrrnTji11'

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        try:
            headers = {'user-agent': 'Mozilla/5.0 (Windows NT 10.0; rv:77.0) Gecko/20100101 Firefox/77.0'}
            url = self.url % (urllib.parse.quote(lyrics.artist), urllib.parse.quote(lyrics.title))
            req = requests.get(url, headers=headers, timeout=10)
            response = req.text
        except:
            return False
        data = json.loads(response)
        try:
            name = data['response']['hits'][0]['result']['primary_artist']['name']
            track = data['response']['hits'][0]['result']['title']
            if (difflib.SequenceMatcher(None, lyrics.artist.lower(), name.lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, lyrics.title.lower(), track.lower()).ratio() > 0.8):
                self.page = data['response']['hits'][0]['result']['url']
            else:
                return False
        except:
            return False
        utilities.log(debug, '%s: search url: %s' % (__title__, self.page))
        try:
            headers = {'user-agent': 'Mozilla/5.0 (Windows NT 10.0; rv:77.0) Gecko/20100101 Firefox/77.0'}
            req = requests.get(self.page, headers=headers, timeout=10)
            response = req.text
        except:
            return False
        response = html.unescape(response)
        matchcode = re.findall('class="Lyrics__Container.*?">(.*?)</div><div', response, flags=re.DOTALL)
        try:
            lyricscode = ""
            for matchCodeItem in matchcode:
                lyricscode = lyricscode + matchCodeItem
            lyr1 = re.sub('<br/>', '\n', lyricscode)
            lyr2 = re.sub('<[^<]+?>', '', lyr1)
            lyr3 = lyr2.replace('\\n','\n').strip()
            if not lyr3 or lyr3 == '[Instrumental]' or lyr3.startswith('Lyrics for this song have yet to be released'):
                return False
            lyrics.lyrics = lyr3
            return True
        except:
            return False

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

    utilities.log(True,  utilities.convert_etree(etree.tostring(xml, encoding='UTF-8',
                                                 pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'genius.py'
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
