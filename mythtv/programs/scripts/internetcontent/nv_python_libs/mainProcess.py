#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mainProcess.py
# Python Script
# Author: R.D. Vaughan
# Purpose:
#   This python script is the common process for all python MythTV Netvision plugin grabber processing.
#   It follows the MythTV standards set for Netvision grabbers.
# Command example:
# See help (-u and -h) options
#
# Design:
#   1) Verify the command line options (display help or version and exit)
#   2) Call the target site with an information request according to the selected options
#   3) Prepare and display XML results from the information request
#
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="Netvision Common Query Processing";
__author__="R.D.Vaughan"
__version__="v0.2.5"
# 0.1.0 Initial development
# 0.1.1 Refining the code like the additional of a grabber specifing the maximum number of items to return
# 0.1.2 Added the Tree view option
# 0.1.3 Updated deocumentation
# 0.2.0 Public release
# 0.2.1 Added the ability to have a mashup name independant of the mashup title
# 0.2.2 Added support of the -H option
# 0.2.3 Added support of the XML version information
# 0.2.4 Added the "command" tag to the XML version information
# 0.2.5 Fixed a bug in the setting up of a search mashup page item maximum

import sys, os
from optparse import OptionParser
import re
from string import capitalize


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


try:
    from StringIO import StringIO
    from lxml import etree
except Exception, e:
    sys.stderr.write(u'\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
    sys.exit(1)

# Check that the lxml library is current enough
# From the lxml documents it states: (http://codespeak.net/lxml/installation.html)
# "If you want to use XPath, do not use libxml2 2.6.27. We recommend libxml2 2.7.2 or later"
# Testing was performed with the Ubuntu 9.10 "python-lxml" version "2.1.5-1ubuntu2" repository package
version = ''
for digit in etree.LIBXML_VERSION:
    version+=str(digit)+'.'
version = version[:-1]
if version < '2.7.2':
    sys.stderr.write(u'''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


class siteQueries():
    '''Methods that quering video Web sites for metadata and outputs the results to stdout any errors are output
    to stderr.
    '''
    def __init__(self,
                apikey,
                target,
                mythtv = False,
                interactive = False,
                select_first = False,
                debug = False,
                custom_ui = None,
                language = None,
                search_all_languages = False, ###CHANGE - Needs to be added
                ):
        """apikey (str/unicode):
            Specify the themoviedb.org API key. Applications need their own key.
            See http://api.themoviedb.org/2.1/ to get your own API key

        mythtv (True/False):
            When True, the movie metadata is being returned has the key and values massaged to match MythTV
            When False, the movie metadata is being returned matches what TMDB returned

        interactive (True/False):
            When True, uses built-in console UI is used to select the correct show.
            When False, the first search result is used.

        select_first (True/False):
            Automatically selects the first series search result (rather
            than showing the user a list of more than one series).
            Is overridden by interactive = False, or specifying a custom_ui

        debug (True/False):
             shows verbose debugging information

        custom_ui (tvdb_ui.BaseUI subclass):
            A callable subclass of tvdb_ui.BaseUI (overrides interactive option)

        language (2 character language abbreviation):
            The language of the returned data. Is also the language search
            uses. Default is "en" (English). For full list, run..

            >>> MovieDb().config['valid_languages'] #doctest: +ELLIPSIS
            ['da', 'fi', 'nl', ...]

        search_all_languages (True/False):
            By default, TMDB will only search in the language specified using
            the language option. When this is True, it will search for the
            show in any language

        """
        self.config = {}

        self.config['apikey'] = apikey
        self.config['target'] = target.Videos(apikey, mythtv = mythtv,
                interactive = interactive,
                select_first = select_first,
                debug = debug,
                custom_ui = custom_ui,
                language = language,
                search_all_languages = search_all_languages,)
        self.searchKeys = [u'title', u'releasedate', u'overview', u'url', u'thumbnail', u'video', u'runtime', u'viewcount']

        self.searchXML = {'header': """<?xml version="1.0" encoding="UTF-8"?>""", 'rss': """
<rss version="2.0"
xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
xmlns:content="http://purl.org/rss/1.0/modules/content/"
xmlns:cnettv="http://cnettv.com/mrss/"
xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
xmlns:media="http://search.yahoo.com/mrss/"
xmlns:atom="http://www.w3.org/2005/Atom"
xmlns:amp="http://www.adobe.com/amp/1.0"
xmlns:dc="http://purl.org/dc/elements/1.1/"
xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">""", 'channel': """
    <channel>
        <title>%(channel_title)s</title>
        <link>%(channel_link)s</link>
        <description>%(channel_description)s</description>
        <numresults>%(channel_numresults)d</numresults>
        <returned>%(channel_returned)d</returned>
        <startindex>%(channel_startindex)d</startindex>""", 'item': """
        <item>
            <title>%(item_title)s</title>
            <author>%(item_author)s</author>
            <pubDate>%(item_pubdate)s</pubDate>
            <description>%(item_description)s</description>
            <link>%(item_link)s</link>
            <media:group>
                <media:thumbnail url='%(item_thumbnail)s'/>
                <media:content url='%(item_url)s' duration='%(item_duration)s' width='%(item_width)s' height='%(item_height)s' lang='%(item_lang)s'/>
            </media:group>
            <rating>%(item_rating)s</rating>
        </item>""", 'end_channel': """
    </channel>""", 'end_rss': """
</rss>
""",
    'nextpagetoken': """
        <nextpagetoken>%(nextpagetoken)s</nextpagetoken>""",
    'prevpagetoken': """
        <prevpagetoken>%(prevpagetoken)s</prevpagetoken>"""
    }

        self.treeViewXML = {'header': """<?xml version="1.0" encoding="UTF-8"?>""", 'rss': """
<rss version="2.0"
xmlns:itunes="http://www.itunes.com/dtds/podcast-1.0.dtd"
xmlns:content="http://purl.org/rss/1.0/modules/content/"
xmlns:cnettv="http://cnettv.com/mrss/"
xmlns:creativeCommons="http://backend.userland.com/creativeCommonsRssModule"
xmlns:media="http://search.yahoo.com/mrss/"
xmlns:atom="http://www.w3.org/2005/Atom"
xmlns:amp="http://www.adobe.com/amp/1.0"
xmlns:dc="http://purl.org/dc/elements/1.1/"
xmlns:mythtv="http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format">""", 'start_channel': """
    <channel>
        <title>%(channel_title)s</title>
        <link>%(channel_link)s</link>
        <description>%(channel_description)s</description>
        <numresults>%(channel_numresults)d</numresults>
        <returned>%(channel_returned)d</returned>
        <startindex>%(channel_startindex)d</startindex>""", "start_directory": """
            <directory name="%s" thumbnail="%s">""", 'item': """
                <item>
                    <title>%(item_title)s</title>
                    <author>%(item_author)s</author>
                    <pubDate>%(item_pubdate)s</pubDate>
                    <description>%(item_description)s</description>
                    <link>%(item_link)s</link>
                    <media:group>
                        <media:thumbnail url='%(item_thumbnail)s'/>
                        <media:content url='%(item_url)s' duration='%(item_duration)s' width='%(item_width)s' height='%(item_height)s' lang='%(item_lang)s'/>
                    </media:group>
                    <rating>%(item_rating)s</rating>
                </item>""", "end_directory": """
            </directory>""", 'end_channel': """
    </channel>""", 'end_rss': """
</rss>
""", }


    # end __init__()

    def searchForVideos(self, search_text, pagenumber):
        '''Search for vidoes that match the search text and output the results a XML to stdout
        '''
        self.firstVideo = True
        self.config['target'].page_limit = self.page_limit
        self.config['target'].grabber_title = self.grabber_title
        self.config['target'].mashup_title = self.mashup_title

        data_sets = self.config['target'].searchForVideos(search_text, pagenumber)
        if data_sets == None:
            return
        if not len(data_sets):
            return

        for data_set in data_sets:
            if self.firstVideo:
                sys.stdout.write(self.searchXML['header'])
                sys.stdout.write(self.searchXML['rss'])
                self.firstVideo = False
            sys.stdout.write(self.searchXML['channel'] % data_set[0])
            if 'nextpagetoken' in data_set[0]:
                sys.stdout.write(self.searchXML['nextpagetoken'] % data_set[0])
            if 'prevpagetoken' in data_set[0]:
                sys.stdout.write(self.searchXML['prevpagetoken'] % data_set[0])
            for item in data_set[1]:
                sys.stdout.write(self.searchXML['item'] % item)
            sys.stdout.write(self.searchXML['end_channel'])
        sys.stdout.write(self.searchXML['end_rss'])
    # end searchForVideos()

    def displayTreeView(self):
        '''Create a Tree View specific to a target site and output the results a XML to stdout. Nested
        directories are permissable.
        '''
        self.firstVideo = True
        self.config['target'].page_limit = self.page_limit
        self.config['target'].grabber_title = self.grabber_title
        self.config['target'].mashup_title = self.mashup_title

        data_sets = self.config['target'].displayTreeView()
        if data_sets == None:
            return
        if not len(data_sets):
            return

        for data_set in data_sets:
            if self.firstVideo:
                sys.stdout.write(self.treeViewXML['header'])
                sys.stdout.write(self.treeViewXML['rss'])
                self.firstVideo = False
            sys.stdout.write(self.treeViewXML['start_channel'] % data_set[0])
            for directory in data_set[1]:
                if isinstance(directory, list):
                    if directory[0] == '':
                        sys.stdout.write(self.treeViewXML['end_directory'])
                        continue
                    sys.stdout.write(self.treeViewXML['start_directory'] % (directory[0], directory[1]))
                    continue
                sys.stdout.write(self.treeViewXML['item'] % directory)
            sys.stdout.write(self.treeViewXML['end_channel'])
        sys.stdout.write(self.treeViewXML['end_rss'])
    # end displayTreeView()

    def displayHTML(self, videocode):
        '''Request a custom Web page from the grabber and display on stdout
        '''
        self.firstVideo = True
        self.config['target'].page_limit = self.page_limit
        self.config['target'].grabber_title = self.grabber_title
        self.config['target'].mashup_title = self.mashup_title
        self.config['target'].HTMLvideocode = videocode

        sys.stdout.write(self.config['target'].displayCustomHTML())
        return
    # end displayHTML()

# end Class siteQueries()

class mainProcess:
    '''Common processing for all Netvision python grabbers.
    '''
    def __init__(self, target, apikey, ):
        '''
        '''
        self.target = target
        self.apikey = apikey
    # end __init__()


    def main(self):
        """Gets video details a search term
        """
        parser = OptionParser()

        parser.add_option(  "-d", "--debug", action="store_true", default=False, dest="debug",
                            help=u"Show debugging info (URLs, raw XML ... etc, info varies per grabber)")
        parser.add_option(  "-u", "--usage", action="store_true", default=False, dest="usage",
                            help=u"Display examples for executing the script")
        parser.add_option(  "-v", "--version", action="store_true", default=False, dest="version",
                            help=u"Display grabber name and supported options")
        parser.add_option(  "-l", "--language", metavar="LANGUAGE", default=u'', dest="language",
                            help=u"Select data that matches the specified language fall back to English if nothing found (e.g. 'es' Español, 'de' Deutsch ... etc).\nNot all sites or grabbers support this option.")
        parser.add_option(  "-p", "--pagenumber", metavar="PAGE NUMBER", default=1, dest="pagenumber",
                            help=u"Display specific page of the search results. Default is page 1.\nPage number is ignored with the Tree View option (-T).")
        functionality = u''
        if self.grabberInfo['search']:
            parser.add_option(  "-S", "--search", action="store_true", default=False, dest="search",
                                help=u"Search for videos")
            functionality+='S'

        if self.grabberInfo['tree']:
            parser.add_option(  "-T", "--treeview", action="store_true", default=False, dest="treeview",
                                help=u"Display a Tree View of a sites videos")
            functionality+='T'

        if self.grabberInfo['html']:
            parser.add_option(  "-H", "--customhtml", action="store_true", default=False,
                                dest="customhtml", help=u"Return a custom HTML Web page")
            functionality+='H'

        parser.usage=u"./%%prog -hduvl%s [parameters] <search text>\nVersion: %s Author: %s\n\nFor details on the MythTV Netvision plugin see the wiki page at:\nhttp://www.mythtv.org/wiki/MythNetvision" % (functionality, self.grabberInfo['version'], self.grabberInfo['author'])

        opts, args = parser.parse_args()

        # Make alls command line arguments unicode utf8
        for index in range(len(args)):
            args[index] = unicode(args[index], 'utf8')

        if opts.debug:
            sys.stdout.write("\nopts: %s\n" % opts)
            sys.stdout.write("\nargs: %s\n\n" % args)

        # Process version command line requests
        if opts.version:
            version = etree.XML(u'<grabber></grabber>')
            etree.SubElement(version, "name").text = self.grabberInfo['title']
            etree.SubElement(version, "author").text = self.grabberInfo['author']
            etree.SubElement(version, "thumbnail").text = self.grabberInfo['thumbnail']
            etree.SubElement(version, "command").text = self.grabberInfo['command']
            for t in self.grabberInfo['type']:
                etree.SubElement(version, "type").text = t
            etree.SubElement(version, "description").text = self.grabberInfo['desc']
            etree.SubElement(version, "version").text = self.grabberInfo['version']
            if self.grabberInfo['search']:
                etree.SubElement(version, "search").text = 'true'
            if self.grabberInfo['tree']:
                etree.SubElement(version, "tree").text = 'true'
            sys.stdout.write(etree.tostring(version, encoding='UTF-8', pretty_print=True))
            sys.exit(0)

        # Process usage command line requests
        if opts.usage:
            sys.stdout.write(self.grabberInfo['usage'])
            sys.exit(0)

        if self.grabberInfo['search']:
            if opts.search and not len(args) == 1:
                sys.stderr.write("! Error: There must be one value for the search option. Your options are (%s)\n" % (args))
                sys.exit(1)
            if opts.search and args[0] == u'':
                sys.stderr.write("! Error: There must be a non-empty search argument, yours is empty.\n")
                sys.exit(1)

        if self.grabberInfo['html']:
            if opts.customhtml and not len(args) == 1:
                sys.stderr.write("! Error: There must be one value for the search option. Your options are (%s)\n" % (args))
                sys.exit(1)
            if opts.customhtml and args[0] == u'':
                sys.stderr.write("! Error: There must be a non-empty Videocode argument, yours is empty.\n")
                sys.exit(1)

        if not self.grabberInfo['search'] and not self.grabberInfo['tree'] and not self.grabberInfo['html']:
            sys.stderr.write("! Error: You have not selected a valid option.\n")
            sys.exit(1)

        Queries = siteQueries(self.apikey, self.target,
                    mythtv = True,
                    interactive = False,
                    select_first = False,
                    debug = opts.debug,
                    custom_ui = None,
                    language = opts.language,
                    search_all_languages = False,)

        # Set the maximum number of items to display per Mythtvnetvision search page
        if self.grabberInfo['search']:
            if opts.search:
                if not 'SmaxPage' in self.grabberInfo.keys():
                    Queries.page_limit = 20   # Default items per page
                else:
                    Queries.page_limit = self.grabberInfo['SmaxPage']

        # Set the maximum number of items to display per Mythtvnetvision tree view page
        if self.grabberInfo['tree']:
            if opts.treeview:
                if not 'TmaxPage' in self.grabberInfo.keys():
                    Queries.page_limit = 20   # Default items per page
                else:
                    Queries.page_limit = self.grabberInfo['TmaxPage']

        # Set the grabber title
        Queries.grabber_title = self.grabberInfo['title']

        # Set the mashup title
        if not 'mashup_title' in self.grabberInfo.keys():
            Queries.mashup_title = Queries.grabber_title
        else:
            if self.grabberInfo['search']:
                if opts.search:
                    Queries.mashup_title = self.grabberInfo['mashup_title'] + "search"
            if self.grabberInfo['tree']:
                if opts.treeview:
                    Queries.mashup_title = self.grabberInfo['mashup_title'] + "treeview"
            if self.grabberInfo['html']:
                if opts.customhtml:
                    Queries.mashup_title = self.grabberInfo['mashup_title'] + "customhtml"

        # Process requested option
        if self.grabberInfo['search']:
            if opts.search:                 # Video search -S
                self.page_limit = Queries.page_limit
                Queries.searchForVideos(args[0], opts.pagenumber)

        if self.grabberInfo['tree']:
            if opts.treeview:               # Video treeview -T
                self.page_limit = Queries.page_limit
                Queries.displayTreeView()

        if self.grabberInfo['html']:
            if opts.customhtml:               # Video treeview -H
                Queries.displayHTML(args[0])

        sys.exit(0)
    # end main()
