#!/usr/bin/env python

from optparse import OptionParser
from tmdb3 import *

import sys

if __name__ == '__main__':
    # this key is registered to this library for testing purposes.
    # please register for your own key if you wish to use this
    # library in your own application.
    # http://help.themoviedb.org/kb/api/authentication-basics
    set_key('1acd79ff610c77f3040073d004f7f5b0')

    parser = OptionParser()
    parser.add_option('-v', "--version", action="store_true", default=False,
                      dest="version", help="Display version.")
    parser.add_option('-d', "--debug", action="store_true", default=False,
                      dest="debug", help="Enables verbose debugging.")
    parser.add_option('-c', "--no-cache", action="store_true", default=False,
                      dest="nocache", help="Disables request cache.")
    opts, args = parser.parse_args()

    if opts.version:
        from tmdb3.tmdb_api import __title__, __purpose__, __version__, __author__
        print __title__
        print ""
        print __purpose__
        print "Version: "+__version__
        sys.exit(0)

    if opts.nocache:
        set_cache(engine='null')
    else:
        set_cache(engine='file', filename='/tmp/pytmdb3.cache')

    if opts.debug:
        request.DEBUG = True

    banner = 'PyTMDB3 Interactive Shell.'
    import code
    try:
        import readline, rlcompleter
    except ImportError:
        pass
    else:
        readline.parse_and_bind("tab: complete")
        banner += ' TAB completion available.'
    namespace = globals().copy()
    namespace.update(locals())
    code.InteractiveConsole(namespace).interact(banner)

