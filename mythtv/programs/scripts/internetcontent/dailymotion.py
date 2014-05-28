#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: dailymotion.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform Dailymotion video lookups for the MythTV Netvision plugin
#   based on information found on the http://www.dailymotion.com website. It
#   follows the MythTV Netvision grabber standards.
#   This script uses the python module dailymotion_api.py which should be included
#   with this script.
#   The dailymotion.py module uses the full access API published by
#   http://www.dailymotion.com see: http://www.dailymotion.com/ca-en/doc/api/player
#
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Import the specific target site API library.
#   2) Set the title for the scripts and the API optional key for the target video site
#   3) Call the common processing routine
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="Dailymotion";
__author__="R.D. Vaughan"
__version__="0.23"
# 0.1.0 Initial development
# 0.1.1 Update documentation
# 0.2.0 Public release
# 0.2.1 Improved the diplayed messages when exception occur
# 0.22  Change to support xml version information display
# 0.23  Added the "command" tag to the xml version information display


__usage_examples__ ='''
(Option Help)
> ./dailymotion.py -h
Usage: ./dailymotion.py -hduvlST [parameters] <search text>
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
  -T, --treeview        Display a Tree View of a sites videos


(Search dailymotion for videos matching search words)
> ./dailymotion.py -S "Birds"
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
        <title>Dailymotion</title>
        <link>http://www.dailymotion.com</link>
        <description>Dailymotion is about finding new ways to see, share and engage your world through the power of online video.</description>
        <numresults>16</numresults>
        <returned>15</returned>
        <startindex>15</startindex>
        <item>
            <title>Bob Marley - Three Little Birds</title>
            <author>hushhush112</author>
            <pubDate>Wed, 24 Jan 2007 04:29:56 +0100</pubDate>
            <description>"Three Little Birds" is a song by Bob Marley &amp; The Wailers from their 1977 album Exodus. The single reached the Top 20 in England.</description>
            <link>http://www.dailymotion.com/swf/x11te1?autoPlay=1</link>
            <media:group>
                <media:thumbnail url='http://ak2.static.dailymotion.com/static/video/163/467/1764361:jpeg_preview_large.jpg?20090610235117'/>
                <media:content url='http://www.dailymotion.com/swf/x11te1?autoPlay=1' duration='204' width='320' height='240' lang='en'/>
            </media:group>
            <rating>5.0</rating>
        </item>
...
        <item>
            <title>Birds Eye View Film Festival 2006: Highlights (Part 1)</title>
            <author>BirdsEyeViewFilm</author>
            <pubDate>Wed, 11 Feb 2009 17:11:32 +0100</pubDate>
            <description>Highlights from the Birds Eye View International Women's Day Gala at the BFI Southbank - the opening event for Birds Eye View Film Festival 2006. Special guests included Gurinder Chadha, Arabella Weir, Jerry Hall and Jessica Stevenson (Hynes).  Each year we showcase the very best in new features, documentaries and short films from women filmmakers from across the globe, alongside premiere screenings, special multi-media events, Q&amp;As, panel discussions and parties.  Birds Eye View celebrates international women filmmakers.  http://www.birds-eye-view.co.uk</description>
            <link>http://www.dailymotion.com/swf/x8c61t?autoPlay=1</link>
            <media:group>
                <media:thumbnail url='http://ak2.static.dailymotion.com/static/video/146/400/14004641:jpeg_preview_large.jpg?20090416211915'/>
                <media:content url='http://www.dailymotion.com/swf/x8c61t?autoPlay=1' duration='199' width='320' height='240' lang='en'/>
            </media:group>
            <rating>5.0</rating>
        </item>
    </channel>
</rss>


(Retrieve a Tree View of the Dailymotion Video Categories)
> ./dailymotion.py -T
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
        <title>Dailymotion</title>
        <link>http://www.dailymotion.com</link>
        <description>Dailymotion is about finding new ways to see, share and engage your world through the power of online video.</description>
        <numresults>0</numresults>
        <returned>1</returned>
        <startindex>0</startindex>
            <directory name="Featured/Most/Best/Current ..." thumbnail="/usr/local/share/mythtv/mythnetvision/icons/dailymotion.jpg">
            <directory name="Featured Videos" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/directories/topics/featured.png">
                <item>
                    <title>Born of Hope - Full Movie</title>
                    <author>BornofHope</author>
                    <pubDate>Tue, 01 Dec 2009 09:49:05 +0100</pubDate>
                    <description>This version of the film is not the best one to watch.  Please select the Extended Version http://www.dailymotion.com/video/xbhonj_born-of-hope-extended-version_shortfilms  Born of Hope is an independent feature film inspired by the Lord of the Rings and produced in the UK. www.bornofhope.comA scattered people, the descendents of storied sea kings of the ancient West, struggle to survive in a lonely wilderness as a dark force relentlessly bends its will toward their destruction. Yet amidst these valiant, desperate people, hope remains. A royal house endures unbroken from father to son.This 70 minute original drama is set in the time before the War of the Ring and tells the story of the Dúnedain, the Rangers of the North, before the return of the King. Inspired by only a couple of paragraphs written by Tolkien in the appendices of the Lord of the Rings we follow Arathorn and Gilraen, the parents of Aragorn, from their first meeting through a turbulent time in their people's history.</description>
                    <link>http://www.dailymotion.com/swf/xbc5ut?autoPlay=1</link>
                    <media:group>
                        <media:thumbnail url='http://ak2.static.dailymotion.com/static/video/732/340/19043237:jpeg_preview_large.jpg?20091215100200'/>
                        <media:content url='http://www.dailymotion.com/swf/xbc5ut?autoPlay=1' duration='4088' width='320' height='240' lang='en'/>
                    </media:group>
                    <rating>4.5</rating>
                </item>
...
                <item>
                    <title>Rubik's Cube : un Français recordman du monde</title>
                    <author>newzy-fr</author>
                    <pubDate>Wed, 10 Oct 2007 21:02:52 +0200</pubDate>
                    <description>Il s'appelle Thibaut Jacquinot, il a été champion de France 2006 de Rubik's Cube et il détient toujours le record du monde de la discipline. Démonstration réalisée sans trucages...Plus de vidéos rubik's Cube sur www.newzy.fr</description>
                    <link>http://www.dailymotion.com/swf/x36jkt?autoPlay=1</link>
                    <media:group>
                        <media:thumbnail url='http://ak2.static.dailymotion.com/static/video/751/443/5344157:jpeg_preview_large.jpg?20090618140735'/>
                        <media:content url='http://www.dailymotion.com/swf/x36jkt?autoPlay=1' duration='32' width='320' height='240' lang='en'/>
                    </media:group>
                    <rating>4.9</rating>
                </item>
            </directory>
            </directory>
            </directory>
    </channel>
</rss>
'''
__search_max_page_items__ = 10
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

# Used for debugging normally commented out
#import nv_python_libs.dailymotion.dailymotion_api as target

# Verify that the tmdb_api modules are installed and accessible
try:
    import nv_python_libs.dailymotion.dailymotion_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/dailymotion" containing the modules dailymotion_api.py (v0.2.0 or greater),
They should have been included with the distribution of dailymotion.py.
Error(%s)
''' % e)
    sys.exit(1)

if target.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed dailymotion_api.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % target.__version__)
    sys.exit(1)


# Verify that the common process modules are installed and accessible
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
    main = process.mainProcess(target, apikey, )
    main.grabberInfo = {}
    main.grabberInfo['title'] = __title__
    main.grabberInfo['command'] = u'dailymotion.py'
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'dailymotion.png'
    main.grabberInfo['type'] = ['video']
    main.grabberInfo['desc'] = u"Dailymotion is about finding new ways to see, share and engage your world through the power of online video."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
