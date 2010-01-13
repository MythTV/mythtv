#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: joost.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is intended to perform Joost video lookups for the MythTV Netvision plugin
#   based on information found on the http://www.joost.com/ website. It
#   follows the MythTV Netvision grabber standards.
#   This script uses the python module joost_api.py which should be included
#   with this script.
#   The joost.py module uses the full access API published by
#   http://www.joost.com/ see: http://www.joost.com/test/doc/api/
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
__title__ ="Joost|T";
__author__="R.D.Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development - Tree View Processing
# 0.1.1 Documentation updates
# 0.2.0 Public release

__usage_examples__ ='''
(Option Help)
> ./joost.py -h
Usage: ./joost.py -hduvlT [parameters] <search text>
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
  -T, --treeview        Display a Tree View of a sites videos


(Option Tree view)
> ./joost.py -T
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
        <title>Joost</title>
        <link>http://www.joost.com</link>
        <description>What's Joost? It's a way to watch videos – music, TV, movies and more – over the Internet. We could just call it a website ... with videos ... but that's not the whole story.</description>
        <numresults>103</numresults>
        <returned>20</returned>
        <startindex>20</startindex>
            <directory name="Most Recent" thumbnail="/usr/local/share/mythtv/mythnetvision/icons/directories/topics/most_recent.png">
                <item>
                    <title>WRC 2006: Uddeholm Swedish Rally</title>
                    <author></author>
                    <pubDate>Thu, 17 Sep 2009 12:16:02 GMT</pubDate>
                    <description></description>
                    <link>http://www.joost.com/embed/0191qcjl?autoplay=1</link>
                    <media:group>
                        <media:thumbnail url='http://00.s.joost.com/W/X/j/rYG51N_npT8XSdOeMcA.jpg'/>
                        <media:content url='http://www.joost.com/embed/0191qcjl?autoplay=1' duration='3113320' width='' height='' lang='en'/>
                    </media:group>
                    <rating></rating>
                </item>
...
                <item>
                    <title>Ten Fingers Of Death</title>
                    <author></author>
                    <pubDate>Wed, 24 Jun 2009 14:26:00 GMT</pubDate>
                    <description>A martial Arts teacher, who lives in a forest, teaches Jackie how to fight. His father is opposed to fighting, and punishes Jackie. Early Chan.</description>
                    <link>http://www.joost.com/embed/15800bs?autoplay=1</link>
                    <media:group>
                        <media:thumbnail url='http://01.s.joost.com/Q/L/t/OgAdWnGnwZ8oA8XHgZg.jpg'/>
                        <media:content url='http://www.joost.com/embed/15800bs?autoplay=1' duration='' width='' height='' lang='en'/>
                    </media:group>
                    <rating></rating>
                </item>
            </directory>
    </channel>
</rss>
'''
__search_max_page_items__ = 10
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


# Used for debugging
#import nv_python_libs.joost.joost_api as target

# Verify that the tmdb_api modules are installed and accessable
try:
    import nv_python_libs.joost.joost_api as target
except Exception:
    sys.stderr.write('''
The subdirectory "nv_python_libs/joost" containing the modules joost_api.py (v0.2.0 or greater),
They should have been included with the distribution of joost.py.

''')
    sys.exit(1)

if target.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed joost_api.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % target.__version__)
    sys.exit(1)


# Verify that the common process modules are installed and accessable
try:
    import nv_python_libs.mainProcess as process
except Exception:
    sys.stderr.write('''
The python script "nv_python_libs/mainProcess.py" must be present.

''')
    sys.exit(1)

if process.__version__ < '0.2.0':
    sys.stderr.write("\n! Error: Your current installed mainProcess.py version is (%s)\nYou must at least have version (0.2.0) or higher.\n" % process.__version__)
    sys.exit(1)

if __name__ == '__main__':
    # No api key is required
    apikey = ""
    main = process.mainProcess(target, apikey, )
    main.grabber_title = __title__
    main.grabber_author = __author__
    main.grabber_version = __version__
    main.grabber_usage_examples = __usage_examples__
    main.search_max_page_items = __search_max_page_items__
    main.tree_max_page_items = __tree_max_page_items__
    main.main()
