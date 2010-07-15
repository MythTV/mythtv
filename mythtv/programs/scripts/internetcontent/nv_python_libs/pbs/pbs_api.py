#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: pbs_api - Simple-to-use Python interface to the PBS RSS feeds
#                       (http://video.pbs.org/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to
#           search and access text metadata, video and image URLs from PBS Web site.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="pbs_api - Simple-to-use Python interface to the PBS videos (http://video.pbs.org/)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from the PBS Web site. These routines process videos
provided by PBS (http://video.pbs.org/). The specific PBS RSS feeds that are processed are controled through a user XML preference file usually found at
"~/.mythtv/MythNetvision/userGrabberPrefs/pbs.xml"
'''

__version__="v0.1.0"
# 0.1.0 Initial development

import os, struct, sys, re, time, datetime, shutil, urllib
from string import capitalize
import logging
from threading import Thread
from copy import deepcopy
from operator import itemgetter, attrgetter

from pbs_exceptions import (PBSUrlError, PBSHttpError, PBSRssError, PBSVideoNotFound, PBSConfigFileError, PBSUrlDownloadError)

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
They should have been included with the distribution of pbs.py.
Error(%s)
''' % e)
    sys.exit(1)
if mashups_api.__version__ < '0.1.0':
    sys.stderr.write("\n! Error: Your current installed mashups_api.py version is (%s)\nYou must at least have version (0.1.0) or higher.\n" % mashups_api.__version__)
    sys.exit(1)


class Videos(object):
    """Main interface to http://video.pbs.org/
    This is done to support a common naming framework for all python Netvision plugins no matter their
    site target.

    Supports search methods
    The apikey is a not required to access http://video.pbs.org/
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
            pass    # PBS does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = u'PBS_Grabber'
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'PBSUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'PBSHttpError': u"! Error: An HTTP communications error with the PBS was raised (%s)\n", 'PBSRssError': u"! Error: Invalid RSS meta data\nwas received from the PBS error (%s). Skipping item.\n", 'PBSVideoNotFound': u"! Error: Video search with the PBS did not return any results (%s)\n", 'PBSConfigFileError': u"! Error: pbs_config.xml file missing\nit should be located in and named as (%s).\n", 'PBSUrlDownloadError': u"! Error: Downloading a RSS feed or Web page (%s).\n", }

        # Channel details and search results
        self.channel = {'channel_title': u'PBS', 'channel_link': u'http://video.pbs.org/', 'channel_description': u"Discover award-winning programming ‚Äì right at your fingertips ‚Äì on PBS Video. Catch the episodes you may have missed and watch your favorite shows whenever you want.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/pbs.png'

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

    def getPBSConfig(self):
        ''' Read the MNV PBS grabber "pbs_config.xml" configuration file
        return nothing
        '''
        # Read the grabber pbs_config.xml configuration file
        url = u'file://%s/nv_python_libs/configs/XML/pbs_config.xml' % (baseProcessingDir, )
        if not os.path.isfile(url[7:]):
            raise PBSConfigFileError(self.error_messages['PBSConfigFileError'] % (url[7:], ))

        if self.config['debug_enabled']:
            print url
            print
        try:
            self.pbs_config = etree.parse(url)
        except Exception, e:
            raise PBSUrlError(self.error_messages['PBSUrlError'] % (url, errormsg))
        return
    # end getPBSConfig()


    def getUserPreferences(self):
        '''Read the pbs_config.xml and user preference pbs.xml file.
        If the pbs.xml file does not exist then create it.
        If the pbs.xml file is too old then update it.
        return nothing
        '''
        # Get pbs_config.xml
        self.getPBSConfig()

        # Check if the pbs.xml file exists
        userPreferenceFile = self.pbs_config.find('userPreferenceFile').text
        if userPreferenceFile[0] == '~':
             self.pbs_config.find('userPreferenceFile').text = u"%s%s" % (os.path.expanduser(u"~"), userPreferenceFile[1:])
        if os.path.isfile(self.pbs_config.find('userPreferenceFile').text):
            # Read the grabber pbs_config.xml configuration file
            url = u'file://%s' % (self.pbs_config.find('userPreferenceFile').text, )
            if self.config['debug_enabled']:
                print url
                print
            try:
                self.userPrefs = etree.parse(url)
            except Exception, e:
                raise PBSUrlError(self.error_messages['PBSUrlError'] % (url, errormsg))
            # Check if the pbs.xml file is too old
            nextUpdateSecs = int(self.userPrefs.find('updateDuration').text)*86400 # seconds in a day
            nextUpdate = time.localtime(os.path.getmtime(self.pbs_config.find('userPreferenceFile').text)+nextUpdateSecs)
            now = time.localtime()
            if nextUpdate > now or self.Search:
                self.mashups_api.userPrefs = self.userPrefs
                return
            create = False
        else:
            create = True

        # If required create/update the pbs.xml file
        self.updatePBS(create)
        return
    # end getUserPreferences()

    def updatePBS(self, create=False):
        ''' Create or update the pbs.xml user preferences file
        return nothing
        '''
        userPBS = u'''
<userPBS>
<!--
    All PBS shows that have represented on the http://video.pbs.org/ web page are included
    in as directories. A user may enable it disable an individual show so that it will be
    included in the treeview. By default ALL shows are enabled.
    NOTE: As the search is based on the treeview data disabling shows will also reduve the
          number of possible search results .
    Updates to the "pbs.xml" file is made every X number of days as determined by the value of
    the "updateDuration" element in this file. The default is every 3 days.
-->
<!-- Number of days between updates to the config file -->
<updateDuration>3</updateDuration>

<!--
    The PBS Search
    "enabled" If you want to remove a source URL then change the "enabled" attribute to "false".
    "xsltFile" The XSLT file name that is used to translate data into MNV item format
    "type" The source type "xml", "html" and "xhtml"
    "url" The link that is used to retrieve the information from the Internet
    "pageFunction" Identifies a XPath extension function that returns the start page/index for the
                    specific source.
    "mnvsearch" (optional) Identifies that search items are to include items from the MNV table using the
                mnvsearch_api.py functions. This attributes value must match the "feedtitle" value
                as it is in the "internetcontentarticles" table. When present the "xsltFile",
                "url" and "pageFunction" attributes are left empty as they will be ignored.
-->
<search name="PBS Search">
  <subDirectory name="PBS">
    <sourceURL enabled="true" name="PBS" xsltFile="" type="xml" url="" pageFunction="" mnvsearch="PBS"/>
  </subDirectory>
</search>

<!--
    The PBS Video RSS feed and HTML URLs.
    "globalmax" (optional) Is a way to limit the number of items processed per source for all
                treeview URLs. A value of zero (0) means there are no limitations.
    "max" (optional) Is a way to limit the number of items processed for an individual sourceURL.
          This value will override any "globalmax" setting. A value of zero (0) means
          there are no limitations and would be the same if the attribute was no included at all.
    "enabled" If you want to remove a source URL then change the "enabled" attribute to "false".
    "xsltFile" The XSLT file name that is used to translate data into MNV item format
    "type" The source type "xml", "html" and "xhtml"
    "url" The link that is used to retrieve the information from the Internet
    "parameter" (optional) Specifies source specific parameter that are passed to the XSLT stylesheet.
                Multiple parameters require the use of key/value pairs. Example:
                parameter="key1:value1;key2:value2" with the ";" as the separator value.
-->

'''

        # Get the current show links from the PBS web site
        showData = self.common.getUrlData(self.pbs_config.find('treeviewUrls'))

        if self.config['debug_enabled']:
            print "create(%s)" % create
            print "showData:"
            sys.stdout.write(etree.tostring(showData, encoding='UTF-8', pretty_print=True))
            print

        # If there is any data then add to new pbs.xml element tree
        showsDir = showData.xpath('//directory')
        if len(showsDir):
            for dirctory in showsDir:
                userPBS+=etree.tostring(dirctory, encoding='UTF-8', pretty_print=True)
            userPBS+=u'</userPBS>'
        userPBS = etree.XML(userPBS)

        if self.config['debug_enabled']:
            print "Before any merging userPBS:"
            sys.stdout.write(etree.tostring(userPBS, encoding='UTF-8', pretty_print=True))
            print

        # If there was an existing pbs.xml file then add any relevant user settings to this new pbs.xml
        if not create:
            userPBS.find('updateDuration').text = self.userPrefs.find('updateDuration').text
            for showElement in self.userPrefs.xpath("//sourceURL[@enabled='false']"):
                showName = showElement.getparent().attrib['name']
                sourceName = showElement.attrib['name']
                elements = userPBS.xpath("//sourceURL[@name=$showName]", showName=showName, sourceName=sourceName)
                if len(elements):
                    elements[0].attrib['enabled'] = u'false'

        if self.config['debug_enabled']:
            print "After any merging userPBS:"
            sys.stdout.write(etree.tostring(userPBS, encoding='UTF-8', pretty_print=True))
            print

        # Save the pbs.xml file
        prefDir = self.pbs_config.find('userPreferenceFile').text.replace(u'/pbs.xml', u'')
        if not os.path.isdir(prefDir):
            os.makedirs(prefDir)
        fd = open(self.pbs_config.find('userPreferenceFile').text, 'w')
        fd.write(u'<userPBS>\n'+u''.join(etree.tostring(element, encoding='UTF-8', pretty_print=True) for element in userPBS)+u'</userPBS>')
        fd.close()

        # Read the refreshed user config file
        try:
            self.userPrefs = etree.parse(self.pbs_config.find('userPreferenceFile').text)
            self.mashups_api.userPrefs = self.userPrefs
        except Exception, e:
            raise PBSUrlError(self.error_messages['PBSUrlError'] % (url, errormsg))
        return
    # end updatePBS()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################

    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        self.mashups_api.page_limit = self.page_limit
        self.mashups_api.grabber_title = self.grabber_title
        self.mashups_api.mashup_title = self.mashup_title
        self.mashups_api.channel_icon = self.channel_icon
        self.mashups_api.mashup_title = u'pbs'

        # Easier for debugging
#        self.mashups_api.searchForVideos(title, pagenumber)
#        print
#        sys.exit()

        try:
            self.mashups_api.Search = True
            self.mashups_api.searchForVideos(title, pagenumber)
        except Exception, e:
            sys.stderr.write(u"! Error: During a PBS Video search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        sys.exit(0)
    # end searchForVideos()

    def displayTreeView(self):
        '''Gather all videos for each PBS show
        Display the results and exit
        '''
        self.mashups_api.page_limit = self.page_limit
        self.mashups_api.grabber_title = self.grabber_title
        self.mashups_api.mashup_title = self.mashup_title
        self.mashups_api.channel_icon = self.channel_icon
        self.mashups_api.mashup_title = u'pbs'

        # Easier for debugging
        self.mashups_api.displayTreeView()
        print
        sys.exit(1)

        try:
            self.mashups_api.Search = False
            self.mashups_api.displayTreeView()
        except Exception, e:
            sys.stderr.write(u"! Error: During a PBS Video treeview\nError(%s)\n" % (e))
            sys.exit(1)

        sys.exit(0)
    # end displayTreeView()
# end Videos() class
