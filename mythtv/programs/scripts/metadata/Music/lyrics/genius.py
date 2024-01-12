# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from lib.culrcscrapers.genius.lyricsScraper import LyricsFetcher
# make sure this-------^^^^^^^^^ matches this file's basename
#       and this-------vvvvvvvvv one too:
info['command']     = 'genius.py'

info['name']        = 'Genius'
info['description'] = 'Search http://www.genius.com for lyrics'
info['author']      = "Paul Harrison and ronie"
info['priority']    = '200'	# 160 in v33-
info['version']     = '2.0'
info['syncronized'] = False

info['artist']      = 'Dire Straits' # v33-
info['title']       = 'Money For Nothing'
info['album']       = ''
info['artist']      = 'Maren Morris' # v34+
info['title']       = 'My Church'

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
