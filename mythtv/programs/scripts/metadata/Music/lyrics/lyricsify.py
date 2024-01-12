# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.lyricsify.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'lyricsify.py'

info['name']        = '*Lyricsify'
info['description'] = 'Search https://lyricsify.com for synchronized lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '130'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = 'Madonna'
info['title']       = 'Crazy For You'
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
