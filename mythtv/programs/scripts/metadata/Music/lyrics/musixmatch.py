# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.musixmatch.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'musixmatch.py'

info['name']        = 'Musixmatch'
info['description'] = 'Search https://musixmatch.com for lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '210'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = False

info['artist']      = 'Kate Bush'
info['title']       = 'Wuthering Heights'
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
