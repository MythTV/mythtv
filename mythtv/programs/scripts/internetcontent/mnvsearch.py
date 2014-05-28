#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mnvsearch.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform a data base search of MythNetvision data base tables for
#   videos based on a command line search term.
#   It follows the MythTV Netvision grabber standards.
#
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Read the ".../emml/feConfig.xml"
#   2) Check if the CGI Web server should be used or if the script is run locally
#   3) Initialize the correct target functions for processing (local or remote)
#   4) Process the search request and display to stdout
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="Search all tree views";
__mashup_title__ = "mnvsearch"
__author__="R.D. Vaughan"
__version__="0.13"
# 0.1.0 Initial development
# 0.11  Change to support xml version information display
# 0.12  Added the "command" tag to the xml version information display
# 0.13  Converted to new common_api.py library

__usage_examples__ ='''
(Option Help)
> ./mnvsearch.py -h
Usage: ./mnvsearch.py -hduvlS [parameters] <search text>
Version: v0.1.0 Author: R.D.Vaughan

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

> ./mnvsearch.py -v
<grabber>
  <name>Search all tree views</name>
  <author>R.D.Vaughan</author>
  <thumbnail>mnvsearch.png</thumbnail>
  <type>video</type>
  <description>MythNetvision treeview data base search</description>
  <version>v0.11</version>
  <search>true</search>
</grabber>

> ./mnvsearch.py -S "Doctor Who"
<?xml version="1.0" encoding="UTF-8"?>
<rss xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd" xmlns:content="http://purl.org/rss/1.0/modules/content/" xmlns:cnettv="http://cnettv.com/mrss/" xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule" xmlns:media="http://search.yahoo.com/mrss/" xmlns:atom="http://www.w3.org/2005/Atom" xmlns:amp="http://www.adobe.com/amp/1.0" xmlns:dc="http://purl.org/dc/elements/1.1/" version="2.0">
<channel>
    <title>Search all tree views</title>
    <link>http://www.mythtv.org/wiki/MythNetvision</link>
    <description>MythNetvision treeview data base search</description>
    <numresults>21</numresults>
    <returned>20</returned>
    <startindex>20</startindex>
<item>
    <title>Doctor Who - Doctor Who and The Brain of Morbius - Episode 8</title>
    <author>BBC</author>
    <pubDate>Sat, 01 May 2010 15:04:02 GMT</pubDate>
    <description>The Doctor and Sarah Jane confront the Morbius monster and seek help from the Sisterhood.</description>
    <link>file:///usr/local/share/mythtv/mythnetvision/scripts/nv_python_libs/configs/HTML/bbciplayer.html?videocode=b00s5ztx</link>
    <media:group>
        <media:thumbnail url="http://node1.bbcimg.co.uk/iplayer/images/episode/b00s5ztx_120_68.jpg"/>
        <media:content url="file:///usr/local/share/mythtv/mythnetvision/scripts/nv_python_libs/configs/HTML/bbciplayer.html?videocode=b00s5ztx" length="" duration="" width="" height="" lang=""/>
    </media:group>
    <rating>0.0</rating>
</item>
...
<item>
    <title>Every Doctor Who Story 1963-2008 - by Babelcolour</title>
    <author>BabelColour</author>
    <pubDate>Mon, 07 Jul 2008 14:45:12 GMT</pubDate>
    <description>To celebrate the 45th Anniversary of the series, here is every Who story from 1963 to 2008, with the spin-off shows and bbci internet productions &amp; the Children In Need specials, but doesn't include any of the spoofs, comedy sketches or other charity skits not made by the official Who Production Team. Edit: It was made &amp; uploaded before the BBC Proms Special 'Music Of The Spheres'. That's why it isn't included! The fabulous music mix (called 'Whorythmics') was created by jex</description>
    <link>http://www.youtube.com/v/lCZhlEdGIm0?f=videos&amp;app=youtube_gdata&amp;autoplay=1</link>
    <media:group>
        <media:thumbnail url="http://i.ytimg.com/vi/lCZhlEdGIm0/hqdefault.jpg"/>
        <media:content url="http://www.youtube.com/v/lCZhlEdGIm0?f=videos&amp;app=youtube_gdata&amp;autoplay=1" length="" duration="" width="" height="" lang=""/>
    </media:group>
    <rating>4.957553</rating>
</item></channel></rss>
'''
__search_max_page_items__ = 20
__tree_max_page_items__ = 20

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


# Used for debugging
#import nv_python_libs.common.common_api
try:
    '''Import the common python class
    '''
    import nv_python_libs.common.common_api as common_api
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/common" containing the modules common_api.py and
common_exceptions.py (v0.1.3 or greater),
They should have been included with the distribution of MythNetvision
Error(%s)
''' % e)
    sys.exit(1)
if common_api.__version__ < '0.1.3':
    sys.stderr.write("\n! Error: Your current installed common_api.py version is (%s)\nYou must at least have version (0.1.3) or higher.\n" % target.__version__)
    sys.exit(1)

# Used for debugging
#import nv_python_libs.mnvsearch.mnvsearch_api as target
try:
    '''Import the python mnvsearch support classes
    '''
    import nv_python_libs.mnvsearch.mnvsearch_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/mnvsearch" containing the modules mnvsearch_api and
mnvsearch_exceptions.py (v0.1.0 or greater),
They should have been included with the distribution of mnvsearch.py.
Error(%s)
''' % e)
    sys.exit(1)
if target.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed mnvsearch_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % target.__version__)
    sys.exit(1)

# Verify that the main process modules are installed and accessible
try:
    import nv_python_libs.mainProcess as process
except Exception, e:
    sys.stderr.write('''
The python script "nv_python_libs/mainProcess.py" must be present.
Error(%s)
''' % e)
    sys.exit(1)

if process.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed mainProcess.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % process.__version__)
    sys.exit(1)

if __name__ == '__main__':
    # No api key is required
    apikey = ""
    # Set the base processing directory that the grabber is installed
    target.baseProcessingDir = os.path.dirname( os.path.realpath( __file__ ))
    # Make sure the target functions have an instance of the common routines
    target.common = common_api.Common()
    main = process.mainProcess(target, apikey, )
    main.grabberInfo = {}
    main.grabberInfo['title'] = __title__
    main.grabberInfo['command'] = u'mnvsearch.py'
    main.grabberInfo['mashup_title'] = __mashup_title__
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'mnvsearch.png'
    main.grabberInfo['type'] = ['video', ]
    main.grabberInfo['desc'] = u"MythTV Online Content database search."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = False
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
