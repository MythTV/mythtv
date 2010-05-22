#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tmdb_nv.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform Movie trailer searches for the MythTV Netvision plugin
#   based on information found on the http://themoviedb.org/ website. It
#   follows the MythTV Netvision grabber standards.
#   This script uses the python module tmdb_api.py which should be included
#   with this script.
#   The tmdb_api.py module uses the full access v2.1 XML api published by
#   themoviedb.org see: http://api.themoviedb.org/2.1/
#   Users of this script are encouraged to populate themoviedb.org with Movie
#   information, trailers, posters and fan art. The richer the source the more
#   valuable the script.
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Import the specific target site API library.
#   2) Set the title for the scrips and the API optional key for the target video site
#   3) Call the common processing routine
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="TMDB Trailers|S";
__author__="R.D.Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development
# 0.1.1 Refining grabber. Added maximum items per page and usage examples.
# 0.1.2 Added the support function code "|S" to the title.
# 0.1.3 Changed publish date to the standard RSS format.
# 0.1.4 Documentation updates
# 0.2.0 Public release


__usage_examples__ = '''
> ./tmdb_nv.py -h
Usage: ./tmdb_nv.py -hduvlS [parameters] <search text>
Version: v0.2.0 Author: R.D.Vaughan

For details on the MythTV Netvision plugin see the wiki page at:
http://www.mythtv.org/wiki/MythNetvision

Options:
  -h, --help            show this help message and exit
  -d, --debug           Show debugging info (URLs, raw XML ... etc, info
                        varies per grabber)
  -u, --usage           Display examples for executing the script
  -v, --version         Display grabber name and supported options
  -l LANGUAGE, --language=LANGUAGE
                        Select data that matches the specified language fall
                        back to English if nothing found (e.g. 'es' Español,
                        'de' Deutsch ... etc). Not all sites or grabbers
                        support this option.
  -p PAGE NUMBER, --pagenumber=PAGE NUMBER
                        Display specific page of the search results. Default
                        is page 1. Page number is ignored with the Tree View
                        option (-T).
  -S, --search          Search for videos


(Movie Trailer search)
> ./tmdb_nv.py -p 1 -S Avatar
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0"
xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
xmlns:content="http://purl.org/rss/1.0/modules/content/"
xmlns:cnettv="http://cnettv.com/mrss/"
xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
xmlns:media="http://search.yahoo.com/mrss/"
xmlns:atom="http://www.w3.org/2005/Atom"
xmlns:amp="http://www.adobe.com/amp/1.0"
xmlns:dc="http://purl.org/dc/elements/1.1/">
    <channel>
        <title>themoviedb.org</title>
        <link>http://themoviedb.org</link>
        <description>themoviedb.org is an open “wiki-style” movie database</description>
        <numresults>1</numresults>
        <returned>20</returned>
        <startindex>2</startindex>
        <item>
            <title>Avatar</title>
            <author></author>
            <pubDate>Fri, 18 Dec 2009 00:00:00 GMT</pubDate>
            <description>A band of humans are pitted in a battle against a distant planet's indigenous population.</description>
            <link>http://www.youtube.com/watch?v=cRdxXPV9GNQ</link>
            <media:group>
                <media:thumbnail url='http://images.themoviedb.org/posters/76198/avatar_xlg_cover.jpg'/>
                <media:content url='http://www.youtube.com/watch?v=cRdxXPV9GNQ' duration='162' width='' height='' lang=''/>
            </media:group>
            <rating>9.75</rating>
        </item>
    </channel>
</rss>
'''
__max_page_items__ = 15

import sys, os

class OutStreamEncoder(object):
    """Wraps a stream with an encoder"""
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, unicode):
            try:
                self.out.write(obj.encode(self.encoding))
            except IOError:
                pass
        else:
            try:
                self.out.write(obj)
            except IOError:
                pass

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')


# Verify that the tmdb_api modules are installed and accessable
try:
    import MythTV.tmdb.tmdb_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "tmdb" containing the modules tmdb_api.py (v0.1.1 or greater), tmdb_ui.py,
tmdb_exceptions.py must have been installed with the MythTV python bindings.
Error:(%s)
''' %  e)
    sys.exit(1)

if target.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed tmdb_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % target.__version__)
    sys.exit(1)

# Verify that the common process modules are installed and accessable
try:
    import nv_python_libs.mainProcess as process
except Exception, e:
    sys.stderr.write('''
The python script "nv_python_libs/mainProcess.py" must be present.
Error:(%s)
''' %  e)
    sys.exit(1)

if process.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed mainProcess.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % process.__version__)
    sys.exit(1)

if __name__ == '__main__':
    # themoviedb.org api key given by Travis Bell for Mythtv
    apikey = "c27cb71cff5bd76e1a7a009380562c62"
    main = process.mainProcess(target, apikey, )
    main.grabber_title = __title__
    main.grabber_author = __author__
    main.grabber_version = __version__
    main.grabber_usage_examples = __usage_examples__
    main.grabber_max_page_items = __max_page_items__
    main.main()
