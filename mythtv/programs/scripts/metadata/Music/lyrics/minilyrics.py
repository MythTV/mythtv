#-*- coding: UTF-8 -*-
"""
Scraper for http://www.viewlyrics.com

taxigps
"""
import sys
import urllib
import urllib2
import socket
import re
import chardet
from hashlib import md5
import chardet
import difflib
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and taxigps'"
__title__       = "MiniLyrics"
__description__ = "Search http://www.viewlyrics.com for lyrics"
__priority__    = "100"
__version__     = "0.1"
__syncronized__ = True


debug = False

socket.setdefaulttimeout(10)

class LyricsFetcher:
    def __init__( self ):
        self.proxy = None

    def htmlEncode(self,string):
        chars = {'\'':'&apos;','"':'&quot;','>':'&gt;','<':'&lt;','&':'&amp;'}
        for i in chars:
            string = string.replace(i,chars[i])
        return string

    def htmlDecode(self,string):
        entities = {'&apos;':'\'','&quot;':'"','&gt;':'>','&lt;':'<','&amp;':'&'}
        for i in entities:
            string = string.replace(i,entities[i])
        return string

    def decryptResultXML(self,value):
        magickey = ord(value[1])
        neomagic = ''
        for i in range(20, len(value)):
            neomagic += chr(ord(value[i]) ^ magickey)
        return neomagic

    def miniLyricsParser(self,response):
        text = self.decryptResultXML(response)
        print text
        lines = text.splitlines()
        ret = []
        for line in lines:
            if line.strip().startswith("<fileinfo filetype=\"lyrics\" "):
                loc = []
                loc.append(self.htmlDecode(re.search('link=\"([^\"]*)\"',line).group(1)))
                if not loc[0].lower().endswith(".lrc"):
                    continue
                if(re.search('artist=\"([^\"]*)\"',line)):
                    loc.insert(0,self.htmlDecode(re.search('artist=\"([^\"]*)\"',line).group(1)))
                else:
                    loc.insert(0,' ')
                if(re.search('title=\"([^\"]*)\"',line)):
                    loc.insert(1,self.htmlDecode(re.search('title=\"([^\"]*)\"',line).group(1)))
                else:
                    loc.insert(1,' ')
                ret.append(loc)
        return ret

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        xml ="<?xml version=\"1.0\" encoding='utf-8'?>\r\n"
        xml+="<search filetype=\"lyrics\" artist=\"%s\" title=\"%s\" " % (lyrics.artist, lyrics.title)
        xml+="ClientCharEncoding=\"utf-8\"/>\r\n"
        md5hash = md5(xml+"Mlv1clt4.0").digest()
        request = "\x02\x00\x04\x00\x00\x00%s%s" % (md5hash, xml)
        del md5hash,xml
        url = "http://www.viewlyrics.com:1212/searchlyrics.htm"
        #url = "http://search.crintsoft.com/searchlyrics.htm"
        req = urllib2.Request(url,request)
        req.add_header("User-Agent", "MiniLyrics")
        if self.proxy:
            opener = urllib2.build_opener(urllib2.ProxyHandler(self.proxy))
        else:
            opener = urllib2.build_opener()
        try:
            response = opener.open(req).read()
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return False
        print response
        lrcList = self.miniLyricsParser(response)
        links = []
        for x in lrcList:
            if (difflib.SequenceMatcher(None, song.artist.lower(), x[0].lower()).ratio() > 0.8) and (difflib.SequenceMatcher(None, song.title.lower(), x[1].lower()).ratio() > 0.8):
                links.append( ( x[0] + ' - ' + x[1], x[2], x[0], x[1] ) )
        if len(links) == 0:
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
        title,url,artist,song = link
        try:
            f = urllib.urlopen(url)
            lyrics = f.read()
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
            return None
        enc = chardet.detect(lyrics)
        lyrics = lyrics.decode(enc['encoding'], 'ignore')
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
    etree.SubElement(version, "command").text = 'minilyrics.py'
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
