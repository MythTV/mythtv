# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.azlyrics.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'azlyrics.py'

info['name']        = 'AZLyrics'
info['description'] = 'Search https://azlyrics.com for lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '230'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = False

info['artist']      = 'La Dispute'
info['title']       = 'Such Small Hands'
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
