# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.lyricsmode.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'lyricsmode.py'

info['name']        = 'LyricsMode'
info['description'] = 'Search http://www.lyricsmode.com for lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '220'     # 240 in v33-
info['version']     = '2.0'
info['syncronized'] = False

info['artist']      = 'Dire Straits' # v33-
info['title']       = 'Money For Nothing'
info['album']       = ''

info['artist']      = 'Maren Morris' # v34+
info['title']       = 'My Church'

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
