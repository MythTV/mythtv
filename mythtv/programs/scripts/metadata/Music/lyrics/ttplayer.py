# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from            common.broken import LyricsFetcher
# make sure this^^^^^^^^^^^^^ is common.broken for broken grabbers
#       and this-------vvvvvvvvv matches this file's basename
info['command']     = 'ttplayer.py'

info['name']        = 'ZZ-*TTPlayer' # last on menu
info['description'] = 'Search http://lrcct2.ttplayer.com for lyrics'
info['author']      = "Paul Harrison and 'taxigps'"
info['priority']    = '9999'	# broken scrapers must be last
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = ''
info['title']       = ''
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
