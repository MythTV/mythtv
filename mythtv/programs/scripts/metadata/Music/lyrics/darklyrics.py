#-*- coding: UTF-8 -*-
"""
Scraper for http://www.darklyrics.com/ - the largest metal lyrics archive on the Web.

scraper by smory
"""

import hashlib
import math
from urllib.request import Request, urlopen
from urllib.parse import quote
import re
import time
import chardet
import sys
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and smory'"
__title__       = "DarkLyrics"
__description__ = "Search http://www.darklyrics.com/ - the largest metal lyrics archive on the Web"
__priority__    = "180"
__version__     = "0.2"
__syncronized__ = False

debug = False

class LyricsFetcher:

    def __init__( self ):
        self.base_url = "http://www.darklyrics.com/"
        self.searchUrl = "http://www.darklyrics.com/search?q=%term%"
        self.cookie = self.getCookie()

    def getCookie(self):
         # http://www.darklyrics.com/tban.js
         lastvisitts = str(int(math.ceil(time.time() * 1000 / (60 * 60 * 6 * 1000))))
         lastvisittscookie = 0
         for i in range(len(lastvisitts)):
             lastvisittscookie = ((lastvisittscookie << 5) - lastvisittscookie) + ord(lastvisitts[i])
             lastvisittscookie = lastvisittscookie & lastvisittscookie
         return str(lastvisittscookie)

    def search(self, artist, title):
        term = quote((artist if artist else "") + " " + (title if title else ""))

        try:
            headers = {'user-agent': 'Mozilla/5.0 (X11; Linux x86_64; rv:78.0) Gecko/20100101 Firefox/78.0'}
            cookies={'lastvisitts': self.cookie}
            request = Request(self.searchUrl.replace("%term%", term))
            request.add_header('User-Agent', headers['user-agent'])
            request.add_header("Cookie", "lastvisitts=%s"% self.cookie)
            content = urlopen(request, timeout=10)
            searchResponse = content.read()
        except:
            return None

        searchResult = re.findall(b"<h2><a\shref=\"(.*?#([0-9]+))\".*?>(.*?)</a></h2>", searchResponse)

        if len(searchResult) == 0:
            return None

        links = []

        i = 0
        for result in searchResult:
            a = []
            a.append(result[2] + ( b" " + self.getAlbumName(self.base_url + result[0].decode('utf-8') )if i < 6 else b"")) # title from server + album nane
            a.append(self.base_url + result[0].decode('utf-8'))  # url with lyrics
            a.append(artist)
            a.append(title)
            a.append(result[1]) # id of the side part containing this song lyrics
            links.append(a)
            i += 1

        return links

    def findLyrics(self, url, index):
        try:
            request = urlopen(url)
            res = request.read()
        except:
            return None

        pattern = b"<a\sname=\"%index%\">(.*?)(?:<h3>|<div)"  # require multi line and dot all mode
        pattern = pattern.replace(b"%index%", index)

        match = re.search(pattern, res, re.MULTILINE | re.DOTALL)
        if match:
            s = match.group(1)
            s = s.replace(b"<br />", b"")
            s = s.replace(b"<i>", b"")
            s = s.replace(b"</i>", b"")
            s = s.replace(b"</a>", b"")
            s = s.replace(b"</h3>", b"")
            return s
        else:
            return None

    def getAlbumName(self, url):
        try:
            request = urlopen(url)
            res = request.read()
        except:
            return b""

        match = re.search(b"<h2>(?:album|single|ep|live):?\s?(.*?)</h2>", res, re.IGNORECASE)

        if match:
            ret = (b"(" + match.group(1) + b")").replace(b"\"", b"")
        else:
            ret = b""
        return(ret)


    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))
        links = self.search(lyrics.artist, lyrics.title)

        if(links == None or len(links) == 0):
            return False
        elif len(links) > 1:
            lyrics.list = links

        lyr = self.get_lyrics_from_list(links[0])
        if not lyr:
            return False

        enc = chardet.detect(lyr)
        lyr = lyr.decode(enc['encoding'], 'ignore')
        lyrics.lyrics = lyr
        return True

    def get_lyrics_from_list(self, link):
        title, url, artist, song, index = link
        return self.findLyrics(url, index)

def performSelfTest():
    found = False
    lyrics = utilities.Lyrics()
    lyrics.source = __title__
    lyrics.syncronized = __syncronized__
    lyrics.artist = 'Dagon'
    lyrics.album = 'Terraphobic'
    lyrics.title = 'Cut To The Heart'

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
        line2 = re.sub(u'[^\u0020-\uD7FF\u0009\u000A\u000D\uE000-\uFFFD\u10000-\u10FFFF]+', '', line)
        etree.SubElement(xml, "lyric").text = line2

    utilities.log(True, utilities.convert_etree(etree.tostring(xml, encoding='UTF-8',
                                      pretty_print=True, xml_declaration=True)))
    sys.exit(0)

def buildVersion():
    from lxml import etree
    version = etree.XML(u'<grabber></grabber>')
    etree.SubElement(version, "name").text = __title__
    etree.SubElement(version, "author").text = __author__
    etree.SubElement(version, "command").text = 'darklyrics.py'
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
