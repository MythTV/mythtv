#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: tedtalks_api - Simple-to-use Python interface to the TedTalks RSS feeds
#                       (http://www.ted.com)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to
#           search and access text metadata, video and image URLs from TedTalks Web site.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="tedtalks_api - Simple-to-use Python interface to the TedTalks videos (http://www.ted.com)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from the TedTalks Web site. These routines process videos
provided by TedTalks (http://www.ted.com). The specific TedTalks RSS feeds that are processed are controled through a user XML preference file usually found at
"~/.mythtv/MythNetvision/userGrabberPrefs/tedtalks.xml"
'''

__version__="v0.1.0"
# 0.1.0 Initial development

import os, struct, sys, re, time, datetime, shutil, urllib
from string import capitalize
import logging
from threading import Thread
from copy import deepcopy
from operator import itemgetter, attrgetter

from tedtalks_exceptions import (TedTalksUrlError, TedTalksHttpError, TedTalksRssError, TedTalksVideoNotFound, TedTalksConfigFileError, TedTalksUrlDownloadError)

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

# Used for debugging
#import nv_python_libs.mashups.mashups_api as target
try:
    '''Import the python mashups support classes
    '''
    import nv_python_libs.mashups.mashups_api as mashups_api
except Exception, e:
    sys.stderr.write('''
The subdirectory "nv_python_libs/mashups" containing the modules mashups_api and
mashups_exceptions.py (v0.1.0 or greater),
They should have been included with the distribution of tedtalks.py.
Error(%s)
''' % e)
    sys.exit(1)
if mashups_api.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed mashups_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % mashups_api.__version__)
    sys.exit(1)


class Videos(object):
    """Main interface to http://www.ted.com
    This is done to support a common naming framework for all python Netvision plugins no matter their
    site target.

    Supports search methods
    The apikey is a not required to access http://www.ted.com
    """
    def __init__(self,
                apikey,
                mythtv = True,
                interactive = False,
                select_first = False,
                debug = False,
                custom_ui = None,
                language = None,
                search_all_languages = False,
                ):
        """apikey (str/unicode):
            Specify the target site API key. Applications need their own key in some cases

        mythtv (True/False):
            When True, the returned meta data is being returned has the key and values massaged to match MythTV
            When False, the returned meta data  is being returned matches what target site returned

        interactive (True/False): (This option is not supported by all target site apis)
            When True, uses built-in console UI is used to select the correct show.
            When False, the first search result is used.

        select_first (True/False): (This option is not supported currently implemented in any grabbers)
            Automatically selects the first series search result (rather
            than showing the user a list of more than one series).
            Is overridden by interactive = False, or specifying a custom_ui

        debug (True/False):
             shows verbose debugging information

        custom_ui (xx_ui.BaseUI subclass): (This option is not supported currently implemented in any grabbers)
            A callable subclass of interactive class (overrides interactive option)

        language (2 character language abbreviation): (This option is not supported by all target site apis)
            The language of the returned data. Is also the language search
            uses. Default is "en" (English). For full list, run..

        search_all_languages (True/False): (This option is not supported by all target site apis)
            By default, a Netvision grabber will only search in the language specified using
            the language option. When this is True, it will search for the
            show in any language

        """
        self.config = {}

        if apikey is not None:
            self.config['apikey'] = apikey
        else:
            pass    # TedTalks does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = u'TedTalks_Grabber'
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'TedTalksUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'TedTalksHttpError': u"! Error: An HTTP communications error with the TedTalks was raised (%s)\n", 'TedTalksRssError': u"! Error: Invalid RSS meta data\nwas received from the TedTalks error (%s). Skipping item.\n", 'TedTalksVideoNotFound': u"! Error: Video search with the TedTalks did not return any results (%s)\n", 'TedTalksConfigFileError': u"! Error: tedtalks_config.xml file missing\nit should be located in and named as (%s).\n", 'TedTalksUrlDownloadError': u"! Error: Downloading a RSS feed or Web page (%s).\n", }

        # Channel details and search results
        self.channel = {'channel_title': u'TedTalks', 'channel_link': u'http://www.ted.com', 'channel_description': u"TED is a small nonprofit devoted to Ideas Worth Spreading.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/tedtalks.png'

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions

        # Initialize Mashups api variables
        mashups_api.common = self.common
        self.mashups_api = mashups_api.Videos(u'')
        self.mashups_api.channel = self.channel
        if language:
            self.mashups_api.config['language'] = self.config['language']
        self.mashups_api.config['debug_enabled'] = self.config['debug_enabled']
        self.mashups_api.getUserPreferences = self.getUserPreferences
    # end __init__()

###########################################################################################################
#
# Start - Utility functions
#
###########################################################################################################

    def getTedTalksConfig(self):
        ''' Read the MNV TedTalks grabber "tedtalks_config.xml" configuration file
        return nothing
        '''
        # Read the grabber tedtalks_config.xml configuration file
        url = u'file://%s/nv_python_libs/configs/XML/tedtalks_config.xml' % (baseProcessingDir, )
        if not os.path.isfile(url[7:]):
            raise TedTalksConfigFileError(self.error_messages['TedTalksConfigFileError'] % (url[7:], ))

        if self.config['debug_enabled']:
            print url
            print
        try:
            self.tedtalks_config = etree.parse(url)
        except Exception, errormsg:
            raise TedTalksUrlError(self.error_messages['TedTalksUrlError'] % (url, errormsg))
        return
    # end getTedTalksConfig()


    def getUserPreferences(self):
        '''Read the tedtalks_config.xml and user preference tedtalks.xml file.
        If the tedtalks.xml file does not exist then create it.
        If the tedtalks.xml file is too old then update it.
        return nothing
        '''
        # Get tedtalks_config.xml
        self.getTedTalksConfig()

        # Check if the tedtalks.xml file exists
        userPreferenceFile = self.tedtalks_config.find('userPreferenceFile').text
        if userPreferenceFile[0] == '~':
             self.tedtalks_config.find('userPreferenceFile').text = u"%s%s" % (os.path.expanduser(u"~"), userPreferenceFile[1:])
        if os.path.isfile(self.tedtalks_config.find('userPreferenceFile').text):
            # Read the grabber tedtalks_config.xml configuration file
            url = u'file://%s' % (self.tedtalks_config.find('userPreferenceFile').text, )
            if self.config['debug_enabled']:
                print url
                print
            try:
                self.userPrefs = etree.parse(url)
            except Exception, errormsg:
                raise TedTalksUrlError(self.error_messages['TedTalksUrlError'] % (url, errormsg))
            create = False
        else:
            create = True

        # If required create/update the tedtalks.xml file
        self.updateTedTalks(create)
        return
    # end getUserPreferences()

    def updateTedTalks(self, create=False):
        ''' Create or update the tedtalks.xml user preferences file
        return nothing
        '''
        userDefaultFile = u'%s/nv_python_libs/configs/XML/defaultUserPrefs/tedtalks.xml' % (baseProcessingDir, )
        if os.path.isfile(userDefaultFile):
            # Read the default tedtalks.xml user preferences file
            url = u'file://%s' % (userDefaultFile, )
            if self.config['debug_enabled']:
                print url
                print
            try:
                userTedTalks = etree.parse(url)
            except Exception, e:
                raise TedTalksUrlError(self.error_messages['TedTalksUrlError'] % (url, e))
        else:
            raise Exception(u'!Error: The default TedTalk file is missing (%s)', userDefaultFile)

        # If there was an existing tedtalks.xml file then add any relevant user settings
        # to this new tedtalks.xml
        if not create:
            for showElement in self.userPrefs.xpath("//sourceURL"):
                showName = showElement.getparent().attrib['name']
                sourceName = showElement.attrib['name']
                elements = userTedTalks.xpath("//sourceURL[@name=$showName]", showName=showName,)
                if len(elements):
                    elements[0].attrib['enabled'] = showElement.attrib['enabled']
                    elements[0].attrib['parameter'] = showElement.attrib['parameter']

        if self.config['debug_enabled']:
            print "After any merging userTedTalks:"
            sys.stdout.write(etree.tostring(userTedTalks, encoding='UTF-8', pretty_print=True))
            print

        # Save the tedtalks.xml file
        prefDir = self.tedtalks_config.find('userPreferenceFile').text.replace(u'/tedtalks.xml', u'')
        if not os.path.isdir(prefDir):
            os.makedirs(prefDir)
        fd = open(self.tedtalks_config.find('userPreferenceFile').text, 'w')
        fd.write(etree.tostring(userTedTalks, encoding='UTF-8', pretty_print=True))
        fd.close()

        # Read the refreshed user config file
        try:
            self.userPrefs = etree.parse(self.tedtalks_config.find('userPreferenceFile').text)
            self.mashups_api.userPrefs = self.userPrefs
        except Exception, errormsg:
            raise TedTalksUrlError(self.error_messages['TedTalksUrlError'] % (url, errormsg))
        return
    # end updateTedTalks()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################

    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the TedTalks web site
        return an array of matching item elements
        return
        '''
        searchVar = self.tedtalks_config.find('searchURLS').xpath(".//href")[0].text
        try:
            searchVar = searchVar.replace(u'SEARCHTERM', urllib.quote_plus(title.encode("utf-8")))
            searchVar = searchVar.replace(u'PAGENUM', unicode(pagenumber))
        except UnicodeDecodeError:
            searchVar = u'?q=%s' % ()
            searchVar = searchVar.replace(u'SEARCHTERM', urllib.quote_plus(title))
            searchVar = searchVar.replace(u'PAGENUM', unicode(pagenumber))
        url = searchVar

        if self.config['debug_enabled']:
            print url
            print

        self.tedtalks_config.find('searchURLS').xpath(".//href")[0].text = url

        # Globally add all the xpath extentions to the "mythtv" namespace allowing access within the
        # XSLT stylesheets
        self.common.buildFunctionDict()
        mnvXpath = etree.FunctionNamespace('http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format')
        mnvXpath.prefix = 'mnvXpath'
        for key in self.common.functionDict.keys():
            mnvXpath[key] = common.functionDict[key]

        # Add the parameter element from the User preferences file
        paraMeter = self.userPrefs.find('search').xpath("//search//sourceURL[@enabled='true']/@parameter")
        if not len(paraMeter):
            raise Exception(u'TedTalks User preferences file "tedtalks.xml" does not have an enabled search with a "parameter" attribute.')
        etree.SubElement(self.tedtalks_config.find('searchURLS').xpath(".//url")[0], "parameter").text = paraMeter[0]

        # Perform a search
        try:
            resultTree = self.common.getUrlData(self.tedtalks_config.find('searchURLS'))
        except Exception, errormsg:
            raise TedTalksUrlDownloadError(self.error_messages['TedTalksUrlDownloadError'] % (errormsg))

        if resultTree is None:
            raise TedTalksVideoNotFound(u"No TedTalks Video matches found for search value (%s)" % title)

        searchResults = resultTree.xpath('//result//item')
        if not len(searchResults):
            raise TedTalksVideoNotFound(u"No TedTalks Video matches found for search value (%s)" % title)

        return searchResults
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Get tedtalks_config.xml
        self.getUserPreferences()

        if self.config['debug_enabled']:
            print "self.tedtalks_config:"
            sys.stdout.write(etree.tostring(self.tedtalks_config, encoding='UTF-8', pretty_print=True))
            print

        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        try:
            data = self.searchTitle(title, pagenumber, self.page_limit)
        except TedTalksVideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            sys.exit(0)
        except TedTalksUrlError, msg:
            sys.stderr.write(u'%s\n' % msg)
            sys.exit(1)
        except TedTalksHttpError, msg:
            sys.stderr.write(self.error_messages['TedTalksHttpError'] % msg)
            sys.exit(1)
        except TedTalksRssError, msg:
            sys.stderr.write(self.error_messages['TedTalksRssError'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+u'</rss>')

        # Set the paging values
        if len(data) == self.page_limit:
            self.channel['channel_returned'] = len(data)
            self.channel['channel_startindex'] = len(data)+(self.page_limit*(int(pagenumber)-1))
            self.channel['channel_numresults'] = len(data)+(self.page_limit*(int(pagenumber)-1)+1)
        else:
            self.channel['channel_returned'] = len(data)+(self.page_limit*(int(pagenumber)-1))
            self.channel['channel_startindex'] = len(data)
            self.channel['channel_numresults'] = len(data)

        # Add the Channel element tree
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)

        for item in data:
            channelTree.append(item)

        # Output the MNV search results
        sys.stdout.write(u'<?xml version="1.0" encoding="UTF-8"?>\n')
        sys.stdout.write(etree.tostring(rssTree, encoding='UTF-8', pretty_print=True))
        sys.exit(0)
    # end searchForVideos()

    def displayTreeView(self):
        '''Gather all videos for each TedTalks show
        Display the results and exit
        '''
        self.mashups_api.page_limit = self.page_limit
        self.mashups_api.grabber_title = self.grabber_title
        self.mashups_api.mashup_title = self.mashup_title
        self.mashups_api.channel_icon = self.channel_icon
        self.mashups_api.mashup_title = u'tedtalks'

        # Easier for debugging
#        self.mashups_api.displayTreeView()
#        print
#        sys.exit(1)

        try:
            self.mashups_api.Search = False
            self.mashups_api.displayTreeView()
        except Exception, e:
            sys.stderr.write(u"! Error: During a TedTalks Video treeview\nError(%s)\n" % (e))
            sys.exit(1)

        sys.exit(0)
    # end displayTreeView()
# end Videos() class
