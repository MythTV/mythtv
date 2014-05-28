#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: vimeo.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform vimeo.com lookups for the MythTV Netvision plugin
#   based on information found on the http://vimeo.com/ website. It
#   follows the MythTV Netvision grabber standards.
#   This script uses the python module vimeo.py which should be included
#   with this script.
#   The vimeo.py module uses the full access API v2 api published by
#   http://vimeo.com/ see: http://vimeo.com/api/docs/advanced-api
#   Users of this script are encouraged to populate vimeo.com with your own Videos. The richer the source the
#   more valuable the script.
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
__title__ ="Vimeo";
__author__="R.D. Vaughan"
__version__="0.23"
# 0.1.0 Initial development
# 0.1.1 Added the Tree view option
# 0.1.2 Documentation review
# 0.2.0 Public release
# 0.2.1 Improved error display messages on an exception abort
# 0.22  Change to support xml version information display
# 0.23  Added the "command" tag to the xml version information display

__usage_examples__ ='''
> ./vimeo.py -h
Usage: ./vimeo.py -hduvlST [parameters] <search text>
Version: v0.22 Author: R.D.Vaughan

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


(Search for all videos identified with the word "Dragon")
> ./vimeo.py -p 1 -S "Dragon"
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
        <title>Vimeo</title>
        <link>http://vimeo.com</link>
        <description>Vimeo is a respectful community of creative people who are passionate about sharing the videos they make.</description>
        <numresults>4856</numresults>
        <returned>20</returned>
        <startindex>20</startindex>
        <item>
            <title>Typophile Film Festival 5 Opening Titles</title>
            <author>Brent Barson</author>
            <pubDate>Tue, 01 Sep 2009 12:27:46 GMT</pubDate>
            <description>Handcrafted with love by BYU design students and faculty, for the 5th Typophile Film Festival. A visual typographic feast about the five senses, and how they contribute to and enhance our creativity. Everything in the film is realâno CG effects!  Shot with a RED One, a Canon EOS 5D Mark II, a Canon EOS 40D, and a Nikon D80. Stop motion created with Dragon Stop Motion.  Creative Director &amp; Faculty Mentor: Brent Barson  Writing &amp; Storyboarding: Brent Barson, Jessica Blackham, Analisa Estrada, Meg Gallagher, John Jensen, Regan Fred Johnson, Colin âThe Pinâ Pinegar  Construction, Paint &amp; Glue:  Brent Barson, Wynn Burton, Analisa Estrada, Meg Gallagher, Olivia Juarez Knudsen, Casey Lewis, Reeding Roberts, Deven Stephens, Brian Christensen (Brain Sculpture)  Animators: Brent Barson, Wynn Burton, Analisa Estrada, Meg Gallagher, Olivia Juarez Knudsen, Reeding Roberts, Deven Stephens  Cinematographer: Wynn Burton  Editing: Brent Barson, Wynn Burton, Analisa Estrada, Meg Gallagher, Reeding Roberts  Hand Models: Analisa Estrada, Meg Gallagher, Olivia Juarez Knudsen, Deven Stephens, Michelle Stephens  Original Music: micah dahl anderson - www.micahdahl.com  Special thanks to Joe, Jared, Zara, and the Punchut/Typophile crew for enabling this!</description>
            <link>http://vimeo.com/moogaloop.swf?clip_id=6382511&amp;autoplay=1</link>
            <media:group>
                <media:thumbnail url='http://ats.vimeo.com/238/418/23841853_200.jpg'/>
                <media:content url='http://vimeo.com/moogaloop.swf?clip_id=6382511&amp;autoplay=1' duration='221' width='640' height='352' lang=''/>
            </media:group>
            <rating>3233</rating>
        </item>
...
        <item>
            <title>"The Dragon's Claw" by Justice of the Unicorns</title>
            <author>Robert Bruce</author>
            <pubDate>Mon, 20 Apr 2009 08:10:32 GMT</pubDate>
            <description>"The Dragon's Claw" The official music video for Justice of the Unicorns from the album "Angels with Uzis".  Winner of Best Music Video at the 2009 Animation Block Party.  http://justiceoftheunicorns.com  Animated in Flash. Composited in Combustion.  Get your free Cubeecraft Bunny Toy! http://robbruce.blogspot.com/2009/05/mutant-pmp-bunny-of-your-very-own.html  robthebruce.com  Please feel free to email me for rate quotes, questions, answers, weather reports, cookie recipes, or just to say hi. rob@robthebruce.com</description>
            <link>http://vimeo.com/moogaloop.swf?clip_id=4239300&amp;autoplay=1</link>
            <media:group>
                <media:thumbnail url='http://ats.vimeo.com/926/443/9264438_200.jpg'/>
                <media:content url='http://vimeo.com/moogaloop.swf?clip_id=4239300&amp;autoplay=1' duration='206' width='640' height='368' lang=''/>
            </media:group>
            <rating>47</rating>
        </item>
    </channel>
</rss>

(Option return Tree view)
> ./vimeo.py -T
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
        <title>Vimeo</title>
        <link>http://vimeo.com</link>
        <description>Vimeo is a respectful community of creative people who are passionate about sharing the videos they make.</description>
        <numresults>18</numresults>
        <returned>1</returned>
        <startindex>0</startindex>
            <directory name="Newest Channels/Most ..." thumbnail="/usr/local/share/mythtv/mythnetvision/icons/vimeo.png">
            <directory name="Most Recent Channels" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/directories/topics/most_recent.png">
                <item>
                    <title>PSA 1: Family Meal is a Healthy Meal</title>
                    <author>Kevin Shermach</author>
                    <pubDate>Tue, 05 Jan 2010 13:08:56 GMT</pubDate>
                    <description>Channel "NYC YMCA new PSA Campaign": :30, :15 and :10</description>
                    <link>http://vimeo.com/moogaloop.swf?clip_id=8557253&amp;autoplay=1</link>
                    <media:group>
                        <media:thumbnail url='http://ts.vimeo.com.s3.amazonaws.com/402/131/40213146_200.jpg'/>
                        <media:content url='http://vimeo.com/moogaloop.swf?clip_id=8557253&amp;autoplay=1' duration='60' width='480' height='272' lang=''/>
                    </media:group>
                    <rating>0</rating>
                </item>
...
                <item>
                    <title>NIN Sydney 2.24.09 - Mid-show Power Outage [HD]</title>
                    <author>Nine Inch Nails</author>
                    <pubDate>Tue, 24 Feb 2009 13:58:19 GMT</pubDate>
                    <description>Power in the Hordern Pavilion went out midway through the set, forcing the show to stop for 40 minutes and nearly causing it to be canceled.  Filmed by Rob Sheridan with the Canon 5D Mark II.</description>
                    <link>http://vimeo.com/moogaloop.swf?clip_id=3353364&amp;autoplay=1</link>
                    <media:group>
                        <media:thumbnail url='http://ats.vimeo.com/240/077/2400771_200.jpg'/>
                        <media:content url='http://vimeo.com/moogaloop.swf?clip_id=3353364&amp;autoplay=1' duration='268' width='504' height='284' lang=''/>
                    </media:group>
                    <rating>216</rating>
                </item>
            </directory>
            </directory>
            </directory>
    </channel>
</rss>
'''
__search_max_page_items__ = 20
__tree_max_page_items__ = 20

import sys, os

class OutStreamEncoder(object):
    """Wraps a stream with an encoder
    """
    def __init__(self, outstream, encoding=None):
        self.out = outstream
        if not encoding:
            self.encoding = sys.getfilesystemencoding()
        else:
            self.encoding = encoding

    def write(self, obj):
        """Wraps the output stream, encoding Unicode strings with the specified encoding"""
        if isinstance(obj, unicode):
            self.out.write(obj.encode(self.encoding))
        else:
            self.out.write(obj)

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)
# Sub class sys.stdout and sys.stderr as a utf8 stream. Deals with print and stdout unicode issues
sys.stdout = OutStreamEncoder(sys.stdout)
sys.stderr = OutStreamEncoder(sys.stderr)

# Makes it easier to debug. Comment out for production
#import nv_python_libs.vimeo.vimeo_api as target

# Verify that the tmdb_api modules are installed and accessible
try:
    import nv_python_libs.vimeo.vimeo_api as target
except Exception,e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/vimeo" containing the modules vimeo_api.py (v0.2.0 or greater),
They should have been included with the distribution of vimeo.py.
Error(%s)
''' % e)
    sys.exit(1)

if target.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed vimeo_api.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % target.__version__)
    sys.exit(1)


# Makes it easier to debug. Comment out for production
import nv_python_libs.mainProcess as process


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
    main.grabberInfo['command'] = u'vimeo.py'
    main.grabberInfo['author'] = __author__
    main.grabberInfo['thumbnail'] = 'vimeo.png'
    main.grabberInfo['type'] = ['video']
    main.grabberInfo['desc'] = u"Vimeo is a respectful community of creative people who are passionate about sharing the videos they make."
    main.grabberInfo['version'] = __version__
    main.grabberInfo['search'] = True
    main.grabberInfo['tree'] = True
    main.grabberInfo['html'] = False
    main.grabberInfo['usage'] = __usage_examples__
    main.grabberInfo['SmaxPage'] = __search_max_page_items__
    main.grabberInfo['TmaxPage'] = __tree_max_page_items__
    main.main()
