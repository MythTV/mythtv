#-*- coding: UTF-8 -*-
import sys
import urllib
import re
from optparse import OptionParser
from common import utilities

__author__      = "Paul Harrison and ronie'"
__title__       = "LyricsMode"
__description__ = "Search http://www.lyricsmode.com for lyrics"
__priority__    = "220"
__version__     = "0.1"
__syncronized__ = False

debug = False

class LyricsFetcher:
    def __init__( self ):
        self.clean_lyrics_regex = re.compile( "<.+?>" )
        self.normalize_lyrics_regex = re.compile( "&#[x]*(?P<name>[0-9]+);*" )
        self.clean_br_regex = re.compile( "<br[ /]*>[\s]*", re.IGNORECASE )
        self.search_results_regex = re.compile("<a href=\"[^\"]+\">([^<]+)</a></td>[^<]+<td><a href=\"([^\"]+)\" class=\"b\">[^<]+</a></td>", re.IGNORECASE)
        self.next_results_regex = re.compile("<A href=\"([^\"]+)\" class=\"pages\">next .</A>", re.IGNORECASE)

    def get_lyrics(self, lyrics):
        utilities.log(debug, "%s: searching lyrics for %s - %s - %s" % (__title__, lyrics.artist, lyrics.album, lyrics.title))

        artist = utilities.deAccent(lyrics.artist)
        title = utilities.deAccent(lyrics.title)
        try: # below is borowed from XBMC Lyrics
            url = "http://www.lyricsmode.com/lyrics/%s/%s/%s.html" % (artist.lower()[:1], artist.lower().replace("&","and").replace(" ","_"), title.lower().replace("&","and").replace(" ","_"))
            lyrics_found = False
            while True:
                utilities.log(debug, "%s: search url: %s" % (__title__, url))
                song_search = urllib.urlopen(url).read()
                if song_search.find("<p id=\"lyrics_text\" class=\"ui-annotatable\">") >= 0:
                    break

                if lyrics_found:
                    # if we're here, we found the lyrics page but it didn't
                    # contains the lyrics part (licensing issue or some bug)
                    return False

                # Let's try to use the research box if we didn't yet
                if not 'search' in url:
                    url = "http://www.lyricsmode.com/search.php?what=songs&s=" + urllib.quote_plus(title.lower())
                else:
                    # the search gave more than on result, let's try to find our song
                    url = ""
                    start = song_search.find('<!--output-->')
                    end = song_search.find('<!--/output-->', start)
                    results = self.search_results_regex.findall(song_search, start, end)

                    for result in results:
                        if result[0].lower() in artist.lower():
                            url = "http://www.lyricsmode.com" + result[1]
                            lyrics_found = True
                            break

                    if not url:
                        # Is there a next page of results ?
                        match = self.next_results_regex.search(song_search[end:])
                        if match:
                            url = "http://www.lyricsmode.com/search.php" + match.group(1)
                        else:
                            return False

            lyr = song_search.split("<p id=\"lyrics_text\" class=\"ui-annotatable\">")[1].split('</p><p id=\"lyrics_text_selected\">')[0]
            lyr = self.clean_br_regex.sub( "\n", lyr ).strip()
            lyr = self.clean_lyrics_regex.sub( "", lyr ).strip()
            lyr = self.normalize_lyrics_regex.sub( lambda m: unichr( int( m.group( 1 ) ) ), lyr.decode("ISO-8859-1") )
            lir = []
            for line in lyr.splitlines():
                line.strip()
                if line.find("Lyrics from:") < 0:
                    lir.append(line)
            lyr = u"\n".join( lir )
            if lyr.startswith('These lyrics are missing'):
                return False
            lyrics.lyrics = lyr
            return True
        except:
            utilities.log(True, "%s: %s::%s (%d) [%s]" % (
                   __title__, self.__class__.__name__,
                   sys.exc_info()[ 2 ].tb_frame.f_code.co_name,
                   sys.exc_info()[ 2 ].tb_lineno,
                   sys.exc_info()[ 1 ]
                   ))
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
    etree.SubElement(version, "command").text = 'lyricsmode.py'
    etree.SubElement(version, "type").text = 'lyrics'
    etree.SubElement(version, "description").text = __description__
    etree.SubElement(version, "version").text = __version__
    etree.SubElement(version, "priority").text = __priority__
    etree.SubElement(version, "syncronized").text = 'True' if __syncronized__ else 'False'

    utilities.log(True, etree.tostring(version, encoding='UTF-8', pretty_print=True,
                                       xml_declaration=True))
    sys.exit(0)

def main():
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
