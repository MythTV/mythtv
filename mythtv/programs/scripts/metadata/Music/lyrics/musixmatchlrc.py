# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.musixmatchlrc.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'musixmatchlrc.py'

info['name']        = '*Musixmatchlrc'
info['description'] = 'Search https://musixmatch.com for synchronized lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '100'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = 'Kate Bush'
info['title']       = 'Wuthering Heights'
info['album']       = ''

# import os
# LyricsFetcher.get_token.PROFILE = os.path.join(os.environ.get('HOME', '/tmp'), '.mythtv')

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
