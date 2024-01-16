# -*- Mode: python; coding: utf-8; indent-tabs-mode: nil; -*-

info = {}
from common import main
from            common.broken import LyricsFetcher
# make sure this^^^^^^^^^^^^^ is common.broken for broken grabbers
#       and this-------vvvvvvvvv matches this file's basename
info['command']     = 'alsong.py'

info['name']        = 'ZZ-*Alsong' # last on menu
info['description'] = 'Search http://lyrics.alsong.co.kr'
info['author']      = "Paul Harrison and 'driip'"
info['priority']    = '9999'	# broken scrapers must be last
info['version']     = '2.0'
info['syncronized'] = True

info['artist']      = ''
info['title']       = ''
info['album']       = ''

if __name__ == '__main__':
    main.main(info, LyricsFetcher)
