#-*- coding: UTF-8 -*-
"""
Scraper for https://www.letssingit.com/

ronnie
"""

import sys
import re
import urllib
import urllib2
import socket
import difflib
import chardet
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and 'ronie'"
__title__       = "LetsSingIt"
__description__ = "Search https://www.letssingit.com/ for lyrics"
__version__     = "0.1"
__priority__    = "120"
__syncronized__ = False

debug = False

socket.setdefaulttimeout(10)

class LyricsFetcher:

    def __init__(self):
        self.url = 'https://search.letssingit.com/?a=search&l=song&s=%s'

    def get_lyrics(self, lyrics):
        utilities.log(debug, '%s: searching lyrics for %s - %s' % (__title__, lyrics.artist, lyrics.title))
        query = '%s+%s' % (urllib.quote_plus(lyrics.artist), urllib.quote_plus(lyrics.title))
        try:
            headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 6.1; rv:25.0) Gecko/20100101 Firefox/25.0', 'Referer': 'https://www.letssingit.com/'}
            request = urllib2.Request(self.url % query, None, headers)
            req = urllib2.urlopen(request)
            response = req.read()
            utilities.log(False, response)
        except:
            return False
        req.close()
        matchcode = re.search('</TD><TD><A href="(.*?)"', response)
        if matchcode:
            lyricscode = (matchcode.group(1))
            clean = lyricscode.lstrip('http://www.letssingit.com/').rsplit('-',1)[0]
            result = clean.replace('-lyrics-', ' ')
            if (difflib.SequenceMatcher(None, query.lower().replace('+', ''), result.lower().replace('-', '')).ratio() > 0.8):
                try:
                    request = urllib2.Request(lyricscode)
                    request.add_header('User-Agent', 'Mozilla/5.0 (Windows NT 6.1; rv:25.0) Gecko/20100101 Firefox/25.0')
                    req = urllib2.urlopen(request)
                    resp = req.read()
                except:
                    return False
                req.close()
                # remove addslots
                resp = re.sub(r'<div id=adslot.*?</div>', '', resp)
                # find all class=lyrics_part_name and class=lyrics_part_text parts
                match = re.findall('<P class=lyrics_part_.*?>(.*?)</P>', resp, flags=re.DOTALL)
                if len(match):
                    for line in match:
                        lyrics.lyrics += line.replace('<br>', '') + '\n'
                else:
                    return False
        return True

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

    utilities.log(True, etree.tostring(xml, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'letssingit.py'
    etree.SubElement(version, "type").text = 'lyrics'
    etree.SubElement(version, "description").text = __description__
    etree.SubElement(version, "version").text = __version__
    etree.SubElement(version, "priority").text = __priority__
    etree.SubElement(version, "syncronized").text = 'True' if __syncronized__ else 'False'

    utilities.log(True, etree.tostring(version, encoding='UTF-8', pretty_print=True,
                                    xml_declaration=True))
    sys.exit(0)

def main():
    global debug

    parser = OptionParser()

    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version and author")
    parser.add_option('-t', "--test", action="store_true", default=False,
                      dest="test", help="Perform self-test for dependencies.")
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

    if (len(args) > 0):
        utilities.log('ERROR: invalid arguments found')
        sys.exit(1)

    fetcher = LyricsFetcher()
    if fetcher.get_lyrics(lyrics):
        buildLyrics(lyrics)
        sys.exit(0)
    else:
        utilities.log(True, "No lyrics found for this track")
        sys.exit(1)

if __name__ == '__main__':
    main()
