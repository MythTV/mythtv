#-*- coding: UTF-8 -*-
import sys
import urllib
import re
from optparse import OptionParser
from common import *

__author__      = "Paul Harrison and ronie'"
__title__       = "LyricsTime"
__description__ = "Search http://www.lyricstime.com for lyrics"
__priority__    = "220"
__version__     = "0.1"
__syncronized__ = False

debug = False

def replace_char(string):
    invalid_char = [" ",",","'",]
    for char in invalid_char:
        string = string.replace(char,"-")
    return string

class LyricsFetcher:
    def __init__( self ):
        self.clean_lyrics_regex = re.compile( "<.+?>" )
        self.normalize_lyrics_regex = re.compile( "&#[x]*(?P<name>[0-9]+);*" )
        self.clean_br_regex = re.compile( "<br[ /]*>[\s]*", re.IGNORECASE )
        self.clean_info_regex = re.compile( "\[[a-z]+?:.*\]\s" )

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        try: # ***** parser - changing this changes search string
            url = "http://www.lyricstime.com/%s-%s-lyrics.html" % (replace_char(lyrics.artist.lower()).replace("&","and").replace("---","-").replace("--","-"),replace_char(lyrics.title.lower()).replace("&","and").replace("---","-").replace("--","-"))
            song_search = urllib.urlopen(url).read()
            utilities.log(debug, "%s: search url: %s" % (__title__, url))
            lyr = song_search.split('<div id="songlyrics" style="padding-right:20px;">')[1].split('</div>')[0]
            lyr = self.clean_br_regex.sub( "\n", lyr ).strip()
            lyr = self.clean_lyrics_regex.sub( "", lyr ).strip()
            lyr = self.normalize_lyrics_regex.sub(
                      lambda m: unichr( int( m.group( 1 ) ) ), lyr.decode("ISO-8859-1") )
            lyr = u"\n".join( [ lyric.strip() for lyric in lyr.splitlines() ] )
            lyr = self.clean_info_regex.sub( "", lyr )
            lyrics.lyrics = lyr
            return True
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % ( __title__, self.__class__.__name__, sys.exc_info()[ 2 ].tb_frame.f_code.co_name, sys.exc_info()[ 2 ].tb_lineno, sys.exc_info()[ 1 ] ))
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
    etree.SubElement(version, "command").text = 'lyricstime.py'
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
