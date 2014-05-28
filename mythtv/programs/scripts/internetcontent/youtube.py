#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: youtube.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform youtube video lookups for the MythTV Netvision plugin
#   based on information found on the http://www.youtube.com/ website. It
#   follows the MythTV Netvision grabber standards.
#   This script uses the python module youtube_api.py which should be included
#   with this script.
#   The youtube.py module uses the full access API published by
#   http://www.youtube.com/ see: http://developer.youtubenservices.com/docs
#
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
__title__ ="YouTube";
__author__="R.D. Vaughan"
__version__="0.23"
# 0.1.0 Initial development
# 0.1.1 Added the Tree view option
# 0.1.2 Documentation review
# 0.2.0 Public release
# 0.2.1 Completed abort exception display message improvements
# 0.22  Change to support xml version information display
# 0.23  Added the "command" tag to the xml version information display

__usage_examples__ ='''
(Option Help)
> ./youtube.py -h
Usage: ./youtube.py -hduvlST [parameters] <search text>
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


(Search youtube for videos matching search words)
> ./youtube.py -S "Buckethead" -p 2
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
        <title>YouTube</title>
        <link>http://www.youtube.com/</link>
        <description>Share your videos with friends, family, and the world.</description>
        <numresults>2092</numresults>
        <returned>20</returned>
        <startindex>40</startindex>
        <item>
            <title>Buckethead and Brain Jam [original video]</title>
            <author>bunghole30</author>
            <pubDate>Sat, 20 Sep 2008 20:37:42 GMT</pubDate>
            <description>Hey guy's ;) \m/ Here, we have a video of Buckethead along with Brain, havin a jam session along with a few Buckethead binge buddy antics, please enjoy www.bucketheadland.com www.tdrsmusic.com -bunghole \m/</description>
            <link>http://www.youtube.com/v/S7l3L6Ikb9M?f=videos&amp;app=youtube_gdata&amp;autoplay=1</link>
            <media:group>
                <media:thumbnail url='http://i.ytimg.com/vi/S7l3L6Ikb9M/0.jpg'/>
                <media:content url='http://www.youtube.com/v/S7l3L6Ikb9M?f=videos&amp;app=youtube_gdata&amp;autoplay=1' duration='399' width='' height='' lang=''/>
            </media:group>
            <rating>4.941772</rating>
        </item>
...
        <item>
            <title>Slap That Bass, Buckethead</title>
            <author>R41N570RM</author>
            <pubDate>Fri, 29 Jun 2007 01:25:40 GMT</pubDate>
            <description>Buckethead on Bass Slappin' and Maximum Bob with Willie T. From Secret Recipe</description>
            <link>http://www.youtube.com/v/0QA-1EeVLvg?f=videos&amp;app=youtube_gdata&amp;autoplay=1</link>
            <media:group>
                <media:thumbnail url='http://i.ytimg.com/vi/0QA-1EeVLvg/0.jpg'/>
                <media:content url='http://www.youtube.com/v/0QA-1EeVLvg?f=videos&amp;app=youtube_gdata&amp;autoplay=1' duration='111' width='' height='' lang=''/>
            </media:group>
            <rating>4.890556</rating>
        </item>
    </channel>
</rss>


(Retrieve a Tree View of the YouTube Video Categories)
> ./youtube.py -T
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
        <title>YouTube</title>
        <link>http://www.youtube.com/</link>
        <description>Share your videos with friends, family, and the world.</description>
        <numresults>13239230</numresults>
        <returned>20</returned>
        <startindex>20</startindex>
            <directory name="Feeds" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/youtube.png">
            <directory name="Highest Rated" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/directories/topics/rated.png">
                <item>
                    <title>Project for Awesome - My Public Access Channel!</title>
                    <author>peron75</author>
                    <pubDate>Thu, 17 Dec 2009 13:51:34 GMT</pubDate>
                    <description>Please support all the Project for Awesome videos today with ratings/comments! Thank you! Thank you to Hank and John Green, Dan Brown and everyone involved!!!  I chose the public access station where I began What the Buck! They are fundraising to help with their new building project.  You can help buy simply signing up for this site and then when you shop, they get donations from that! Yay! Thanks if you can sign up! (its Free!) LOL xoxo Michael   Please sign up: http://igive.com/wpaa</description>
                    <link>http://www.youtube.com/v/tdBHzkoXB_8?f=standard&amp;app=youtube_gdata&amp;autoplay=1</link>
                    <media:group>
                        <media:thumbnail url='http://i.ytimg.com/vi/tdBHzkoXB_8/hqdefault.jpg'/>
                        <media:content url='http://www.youtube.com/v/tdBHzkoXB_8?f=standard&amp;app=youtube_gdata&amp;autoplay=1' duration='259' width='' height='' lang=''/>
                    </media:group>
                    <rating>4.972514</rating>
                </item>
...
                <item>
                    <title>Harry Chapin--Taxi</title>
                    <author>Lewismadmax</author>
                    <pubDate>Fri, 04 May 2007 21:10:00 GMT</pubDate>
                    <description>"Baby's so high that shes skying, yeah she's flying afraid to fall, I'll tell you why Baby's crying, cuz' she's dying arent we all". The greatest person to fight world hunger died in 1982 God bless Harry Chapin.</description>
                    <link>http://www.youtube.com/v/c5dwksSbD34?f=videos&amp;app=youtube_gdata&amp;autoplay=1</link>
                    <media:group>
                        <media:thumbnail url='http://i.ytimg.com/vi/c5dwksSbD34/hqdefault.jpg'/>
                        <media:content url='http://www.youtube.com/v/c5dwksSbD34?f=videos&amp;app=youtube_gdata&amp;autoplay=1' duration='401' width='' height='' lang=''/>
                    </media:group>
                    <rating>4.9039855</rating>
                </item>
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

# Used for debugging
#import nv_python_libs.youtube.youtube_api as target

# Verify that the tmdb_api modules are installed and accessible
try:
    import nv_python_libs.youtube.youtube_api as target
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/youtube" containing the modules youtube_api.py (v0.2.0 or greater),
They should have been included with the distribution of youtube.py.
Error(%s)
''' % e)
    sys.exit(1)

if target.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed youtube_api.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % target.__version__)
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
    main.grabberInfo['command'] = u'youtube.py'
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'youtube.png'
    main.grabberInfo['type'] = ['video']
    main.grabberInfo['desc'] = u"Share your videos with friends, family, and the world."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
