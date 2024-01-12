# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.megalobiz.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv too
info['command']     = 'megalobiz.py'

info['name']        = '*Megalobiz'
info['description'] = 'Search https://megalobiz.com for synchronized lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '140'	# not in v33-
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = 'Michael Jackson'
info['title']       = 'Beat It'
info['album']       = ''

# it takes 8 seconds, have to move to the end -twitham, 2024/01
info['priority']    = '400'

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
