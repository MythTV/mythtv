#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mashups_api - Simple-to-use Python interface to Mashups of RSS feeds and HTML video data
#
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to
#           search and access text metadata, video and image URLs from various Internet sources.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mashups_api - Simple-to-use Python interface to Mashups of RSS feeds and HTML video data"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from various Internet sources. These routines process the RSS feeds and information into MNV standard channel, directory and item RSS XML files. The specific Mashups are specified through a user XML preference file usually found at
"~/.mythtv/MythNetvision/userGrabberPrefs/xxxxMashup.xml" where "xxxx" is the specific mashup name matching the associated grabber name that calls these functions.
'''

__version__="v0.1.5"
# 0.1.0 Initial development
# 0.1.1 Added Search Mashup capabilities
# 0.1.2 Fixed a couple of error messages with improper variable names
# 0.1.3 Add the ability for a Mashup to search the "internetcontentarticles" table
# 0.1.4 Add the ability for a Mashup to pass variables to a XSLT style sheet
# 0.1.5 Removed a redundant build of the common XSLT function dictionary

import os, struct, sys, time, datetime, shutil, urllib
from socket import gethostname, gethostbyname
from copy import deepcopy
import logging

from mashups_exceptions import (MashupsUrlError, MashupsHttpError, MashupsRssError, MashupsVideoNotFound, MashupsConfigFileError, MashupsUrlDownloadError)

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


class Videos(object):
    """Main interface to any Mashup
    This is done to support a common naming framework for all python Netvision plugins
    no matter their site target.

    Supports MNV Mashup Search and Treeview methods
    The apikey is a not required for Mashups
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
            pass    # Mashups does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = u'Mashups_Grabber'
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        if language:
            self.config['language'] = language
        else:
            self.config['language'] = u'en'

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'MashupsUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'MashupsHttpError': u"! Error: An HTTP communications error with the Mashups was raised (%s)\n", 'MashupsRssError': u"! Error: Invalid RSS meta data\nwas received from the Mashups error (%s). Skipping item.\n", 'MashupsVideoNotFound': u"! Error: Video search with the Mashups did not return any results (%s)\n", 'MashupsConfigFileError': u"! Error: mashups_config.xml file missing\nit should be located in and named as (%s).\n", 'MashupsUrlDownloadError': u"! Error: Downloading a RSS feed or Web page (%s).\n", }

        # Channel details and search results
        self.channel = {'channel_title': u'', 'channel_link': u'http://www.mythtv.org/wiki/MythNetvision', 'channel_description': u"Mashups combines media from multiple sources to create a new work", 'channel_numresults': 0, 'channel_returned': 0, u'channel_startindex': 0}

        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/mashups.png'

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions
    # end __init__()

###########################################################################################################
#
# Start - Utility functions
#
###########################################################################################################

    def setTreeViewIcon(self, dir_icon=None):
        '''Check if there is a specific generic tree view icon. If not default to the channel icon.
        return self.tree_dir_icon
        '''
        self.tree_dir_icon = self.channel_icon
        if not dir_icon:
            if not self.feed_icons.has_key(self.tree_key):
                return self.tree_dir_icon
            if not self.feed_icons[self.tree_key].has_key(self.feed):
                return self.tree_dir_icon
            if not dir_icon:
                return self.tree_dir_icon
            dir_icon = self.feed_icons[self.tree_key][self.feed]
            if not dir_icon:
                return self.tree_dir_icon
        self.tree_dir_icon = u'%%SHAREDIR%%/mythnetvision/icons/%s.png' % (dir_icon, )
        return self.tree_dir_icon
    # end setTreeViewIcon()


    def getMashupsConfig(self):
        ''' Read the MNV Mashups grabber "mashups_config.xml" configuration file
        return nothing
        '''
        # Read the grabber mashups_config.xml configuration file
        url = u'%s/nv_python_libs/configs/XML/mashups_config.xml' % (baseProcessingDir, )
        if not os.path.isfile(url):
            raise MashupsConfigFileError(self.error_messages['MashupsConfigFileError'] % (url, ))

        if self.config['debug_enabled']:
            print url
            print
        try:
            self.mashups_config = etree.parse(url)
        except Exception, errormsg:
            raise MashupsUrlError(self.error_messages['MashupsUrlError'] % (url, errormsg))
        return
    # end getMashupsConfig()


    def getUserPreferences(self):
        '''Read the mashups_config.xml and user preference xxxxxMashup.xml file.
        If the xxxxxMashup.xml file does not exist then copy the default.
        return nothing
        '''
        # Get mashups_config.xml
        self.getMashupsConfig()

        # Check if the mashups.xml file exists
        fileName = u'%s.xml' % self.mashup_title.replace(u'treeview', u'').replace(u'search', u'')
        userPreferenceFile = u'%s/%s' % (self.mashups_config.find('userPreferenceFile').text, fileName)
        if userPreferenceFile[0] == '~':
             self.mashups_config.find('userPreferenceFile').text = u"%s%s" % (os.path.expanduser(u"~"), userPreferenceFile[1:])

        # If the user config file does not exists then copy one from the default
        create = False
        defaultConfig = u'%s/nv_python_libs/configs/XML/defaultUserPrefs/%s' % (baseProcessingDir, fileName, )
        prefDir = self.mashups_config.find('userPreferenceFile').text.replace(u'/'+fileName, u'')
        if not os.path.isfile(self.mashups_config.find('userPreferenceFile').text):
            # Make the necessary directories if they do not already exist
            if not os.path.isdir(prefDir):
                os.makedirs(prefDir)
            shutil.copy2(defaultConfig, self.mashups_config.find('userPreferenceFile').text)
            create = True

        # Read the grabber mashups_config.xml configuration file
        if self.config['debug_enabled']:
            print self.mashups_config.find('userPreferenceFile').text
            print
        try:
            self.userPrefs = etree.parse(self.mashups_config.find('userPreferenceFile').text)
        except Exception, errormsg:
            raise MashupsUrlError(self.error_messages['MashupsUrlError'] % (self.mashups_config.find('userPreferenceFile').text, errormsg))

        if create:
            return

        # Merge the existing entries with the user's current settings to get any distributed
        # additions or changes
        try:
            defaultPrefs = etree.parse(defaultConfig)
        except Exception, errormsg:
            raise MashupsUrlError(self.error_messages['MashupsUrlError'] % (defaultConfig, errormsg))
        urlFilter = etree.XPath('//sourceURL[@url=$url]', namespaces=self.common.namespaces)
        globalmaxFilter = etree.XPath('./../..', namespaces=self.common.namespaces)
        for sourceURL in self.userPrefs.xpath('//sourceURL'):
            url = sourceURL.attrib['url']
            defaultSourceURL = urlFilter(defaultPrefs, url=url)
            if len(defaultSourceURL):
                defaultSourceURL[0].attrib['enabled'] = sourceURL.attrib['enabled']
                if sourceURL.attrib.get('max'):
                    defaultSourceURL[0].attrib['max'] = sourceURL.attrib['max']
                directory = globalmaxFilter(sourceURL)[0]
                if directory.attrib.get('globalmax'):
                    defaultDir = directory.attrib.get('globalmax')
                    globalmaxFilter(defaultSourceURL[0])[0].attrib['globalmax'] = directory.attrib['globalmax']

        # Save the updated xxxxMashup.xml file
        tagName = defaultPrefs.getroot().tag

        # Get the internal documentaion
        docComment = u''
        for element in defaultPrefs.iter(tag=etree.Comment):
            docComment+=etree.tostring(element, encoding='UTF-8', pretty_print=True)[:-1]

        fd = open(self.mashups_config.find('userPreferenceFile').text, 'w')
        fd.write((u'<%s>\n' % tagName)+docComment)
        fd.write(u''.join(etree.tostring(element, encoding='UTF-8', pretty_print=True) for element in defaultPrefs.xpath(u'/%s/*' % tagName))+(u'</%s>\n'% tagName))
        fd.close()

        # Make sure that the user preferences are using the latest version
        self.userPrefs = defaultPrefs

        return
    # end getUserPreferences()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################

    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        Display the results and exit
        """
        # Get the user preferences
        try:
            self.getUserPreferences()
        except Exception, e:
            sys.stderr.write(u'%s' % e)
            sys.exit(1)

        if self.config['debug_enabled']:
            print "self.userPrefs:"
            sys.stdout.write(etree.tostring(self.userPrefs, encoding='UTF-8', pretty_print=True))
            print

        # Import the mnvsearch_api.py functions
        fullPath = u'%s/nv_python_libs/%s' % (self.common.baseProcessingDir, 'mnvsearch')
        sys.path.append(fullPath)
        try:
            exec('''import mnvsearch_api''')
        except Exception, errmsg:
            sys.stderr.write(u'! Error: Dynamic import of mnvsearch_api functions\nmessage(%s)\n' % (errmsg))
            sys.exit(1)
        mnvsearch_api.common = self.common
        mnvsearch = mnvsearch_api.Videos(None, debug=self.config['debug_enabled'], language=self.config['language'])
        mnvsearch.page_limit = self.page_limit

        # Get the dictionary of mashups functions pointers
        self.common.buildFunctionDict()

        # Massage channel icon
        self.channel_icon = self.common.ampReplace(self.channel_icon)

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+u'</rss>')

        # Add the Channel element tree
        self.channel['channel_title'] = self.grabber_title
        self.channel_icon = self.setTreeViewIcon(dir_icon=self.mashup_title.replace(u'Mashuptreeview', u''))
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)

        # Globally add all the xpath extentions to the "mythtv" namespace allowing access within the
        # XSLT stylesheets
        self.common.buildFunctionDict()
        mnvXpath = etree.FunctionNamespace('http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format')
        mnvXpath.prefix = 'mnvXpath'
        for key in self.common.functionDict.keys():
            mnvXpath[key] = self.common.functionDict[key]

        # Build Search parameter dictionary
        self.common.pagenumber = pagenumber
        self.common.page_limit = self.page_limit
        self.common.language = self.config['language']
        self.common.searchterm = title.encode("utf-8")
        searchParms = {
            'searchterm': urllib.quote_plus(title.encode("utf-8")),
            'pagemax': self.page_limit,
            'language': self.config['language'],
            }
        # Create a structure of feeds that can be concurrently downloaded
        xsltFilename = etree.XPath('./@xsltFile', namespaces=self.common.namespaces)
        sourceData = etree.XML(u'<xml></xml>')
        for source in self.userPrefs.xpath('//search//sourceURL[@enabled="true"]'):
            if source.attrib.get('mnvsearch'):
                continue
            urlName = source.attrib.get('name')
            if urlName:
                 uniqueName = u'%s;%s' % (urlName, source.attrib.get('url'))
            else:
                uniqueName = u'RSS;%s' % (source.attrib.get('url'))
            url = etree.XML(u'<url></url>')
            etree.SubElement(url, "name").text = uniqueName
            if source.attrib.get('pageFunction'):
                searchParms['pagenum'] = self.common.functionDict[source.attrib['pageFunction']]('dummy', 'dummy')
            else:
                searchParms['pagenum'] = pagenumber
            etree.SubElement(url, "href").text = source.attrib.get('url') % searchParms
            if len(xsltFilename(source)):
                for xsltName in xsltFilename(source):
                    etree.SubElement(url, "xslt").text = xsltName.strip()
            etree.SubElement(url, "parserType").text = source.attrib.get('type')
            sourceData.append(url)

        if self.config['debug_enabled']:
            print "rssData:"
            sys.stdout.write(etree.tostring(sourceData, encoding='UTF-8', pretty_print=True))
            print

        # Get the source data
        if sourceData.find('url') != None:
            # Process each directory of the user preferences that have an enabled rss feed
            try:
                resultTree = self.common.getUrlData(sourceData)
            except Exception, errormsg:
                raise MashupsUrlDownloadError(self.error_messages['MashupsUrlDownloadError'] % (errormsg))
            if self.config['debug_enabled']:
                print "resultTree:"
                sys.stdout.write(etree.tostring(resultTree, encoding='UTF-8', pretty_print=True))
                print

            # Process the results
            itemFilter = etree.XPath('.//item', namespaces=self.common.namespaces)
            # Create special directory elements
            for result in resultTree.findall('results'):
                channelTree.xpath('numresults')[0].text = self.common.numresults
                channelTree.xpath('returned')[0].text = self.common.returned
                channelTree.xpath('startindex')[0].text = self.common.startindex
                for item in itemFilter(result):
                    channelTree.append(item)

        # Process any mnvsearches
        for source in self.userPrefs.xpath('//search//sourceURL[@enabled="true" and @mnvsearch]'):
            results = mnvsearch.searchForVideos(title, pagenumber, feedtitle=source.xpath('./@mnvsearch')[0])
            if len(results[0].keys()):
                channelTree.xpath('returned')[0].text = u'%s' % (int(channelTree.xpath('returned')[0].text)+results[1])
                channelTree.xpath('startindex')[0].text = u'%s' % (int(channelTree.xpath('startindex')[0].text)+results[2])
                channelTree.xpath('numresults')[0].text = u'%s' % (int(channelTree.xpath('numresults')[0].text)+results[3])
                lastKey = None
                for key in sorted(results[0].keys()):
                    if lastKey != key:
                        channelTree.append(results[0][key])
                        lastKey = key

        # Check that there was at least some items
        if len(rssTree.xpath('//item')):
            # Output the MNV Mashup results
            sys.stdout.write(u'<?xml version="1.0" encoding="UTF-8"?>\n')
            sys.stdout.write(etree.tostring(rssTree, encoding='UTF-8', pretty_print=True))

        sys.exit(0)
    # end searchForVideos()


    def displayTreeView(self):
        '''Gather the Mashups Internet sources then get the videos meta data in each of them
        Display the results and exit
        '''
        # Get the user preferences
        try:
            self.getUserPreferences()
        except Exception, e:
            sys.stderr.write(u'%s' % e)
            sys.exit(1)

        if self.config['debug_enabled']:
            print "self.userPrefs:"
            sys.stdout.write(etree.tostring(self.userPrefs, encoding='UTF-8', pretty_print=True))
            print

        # Massage channel icon
        self.channel_icon = self.common.ampReplace(self.channel_icon)

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+u'</rss>')

        # Add the Channel element tree
        self.channel['channel_title'] = self.grabber_title
        self.channel_icon = self.setTreeViewIcon(dir_icon=self.mashup_title.replace(u'Mashuptreeview', u''))
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)
        self.common.language = self.config['language']

        # Globally add all the xpath extentions to the "mythtv" namespace allowing access within the
        # XSLT stylesheets
        self.common.buildFunctionDict()
        mnvXpath = etree.FunctionNamespace('http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format')
        mnvXpath.prefix = 'mnvXpath'
        for key in self.common.functionDict.keys():
            mnvXpath[key] = common.functionDict[key]

        # Create a structure of feeds that can be concurrently downloaded
        xsltFilename = etree.XPath('./@xsltFile', namespaces=self.common.namespaces)
        sourceData = etree.XML(u'<xml></xml>')
        for source in self.userPrefs.xpath('//directory//sourceURL[@enabled="true"]'):
            urlName = source.attrib.get('name')
            if urlName:
                 uniqueName = u'%s;%s' % (urlName, source.attrib.get('url'))
            else:
                uniqueName = u'RSS;%s' % (source.attrib.get('url'))
            url = etree.XML(u'<url></url>')
            etree.SubElement(url, "name").text = uniqueName
            etree.SubElement(url, "href").text = source.attrib.get('url')
            if source.attrib.get('parameter') != None:
                etree.SubElement(url, "parameter").text = source.attrib.get('parameter')
            if len(xsltFilename(source)):
                for xsltName in xsltFilename(source):
                    etree.SubElement(url, "xslt").text = xsltName.strip()
            etree.SubElement(url, "parserType").text = source.attrib.get('type')
            sourceData.append(url)

        if self.config['debug_enabled']:
            print "rssData:"
            sys.stdout.write(etree.tostring(sourceData, encoding='UTF-8', pretty_print=True))
            print

        # Get the source data
        if sourceData.find('url') != None:
            # Process each directory of the user preferences that have an enabled rss feed
            try:
                resultTree = self.common.getUrlData(sourceData)
            except Exception, errormsg:
                raise MashupsUrlDownloadError(self.error_messages['MashupsUrlDownloadError'] % (errormsg))
            if self.config['debug_enabled']:
                print "resultTree:"
                sys.stdout.write(etree.tostring(resultTree, encoding='UTF-8', pretty_print=True))
                print
            # Process the results
            categoryDir = None
            categoryElement = None
            xsltShowName = etree.XPath('//directory//sourceURL[@url=$url]/../@name', namespaces=self.common.namespaces)
            channelThumbnail = etree.XPath('.//directoryThumbnail', namespaces=self.common.namespaces)
            directoryFilter = etree.XPath('.//directory', namespaces=self.common.namespaces)
            itemFilter = etree.XPath('.//item', namespaces=self.common.namespaces)
            feedFilter = etree.XPath('//directory//sourceURL[@url=$url]')
            channelThumbnail = etree.XPath('.//directoryThumbnail', namespaces=self.common.namespaces)
            specialDirectoriesFilter = etree.XPath('.//specialDirectories')
            specialDirectoriesKeyFilter = etree.XPath('.//specialDirectories/*[name()=$name]/@key')
            specialDirectoriesDict = {}
            # Create special directory elements
            for result in resultTree.findall('results'):
                if len(specialDirectoriesFilter(result)):
                    for element in specialDirectoriesFilter(result)[0]:
                        if not element.tag in specialDirectoriesDict.keys():
                            specialDirectoriesElement = etree.XML(u'<directory></directory>')
                            specialDirectoriesElement.attrib['name'] = element.attrib['dirname']
                            if element.attrib.get('count'):
                                count = int(element.attrib['count'])
                            else:
                                count = 1
                            if element.attrib.get('thumbnail'):
                                specialDirectoriesElement.attrib['thumbnail'] = element.attrib['thumbnail']
                            else:
                                specialDirectoriesElement.attrib['thumbnail'] = self.channel_icon
                            specialDirectoriesDict[element.tag] = [specialDirectoriesElement, element.attrib['reverse'], count]
            for result in resultTree.findall('results'):
                names = result.find('name').text.split(u';')
                names[0] = self.common.massageText(names[0])
                if len(xsltShowName(self.userPrefs, url=names[1])):
                    names[0] =  self.common.massageText(xsltShowName(self.userPrefs, url=names[1])[0].strip())
                if names[0] == 'RSS':
                    names[0] = self.common.massageText(rssName(result.find('result'))[0].text)
                count = 0
                urlMax = None
                url = feedFilter(self.userPrefs, url=names[1])
                if len(url):
                    if url[0].attrib.get('max'):
                        try:
                            urlMax = int(url[0].attrib.get('max'))
                        except:
                            pass
                    elif url[0].getparent().getparent().attrib.get('globalmax'):
                        try:
                            urlMax = int(url[0].getparent().getparent().attrib.get('globalmax'))
                        except:
                            pass
                    if urlMax == 0:
                        urlMax = None

                # Create a new directory and/or subdirectory if required
                if names[0] != categoryDir:
                    if categoryDir != None:
                        channelTree.append(categoryElement)
                    categoryElement = etree.XML(u'<directory></directory>')
                    categoryElement.attrib['name'] = names[0]
                    if len(channelThumbnail(result)):
                        categoryElement.attrib['thumbnail'] = channelThumbnail(result)[0].text
                    else:
                        categoryElement.attrib['thumbnail'] = self.channel_icon
                    categoryDir = names[0]

                # Add the special directories videos
                for key in specialDirectoriesDict.keys():
                    sortDict = {}
                    count = 0
                    for sortData in specialDirectoriesKeyFilter(result, name=key):
                        sortDict[sortData] = count
                        count+=1
                    if len(sortDict):
                        if specialDirectoriesDict[key][1] == 'true':
                            sortedKeys = sorted(sortDict.keys(), reverse=True)
                        else:
                            sortedKeys = sorted(sortDict.keys(), reverse=False)
                        if specialDirectoriesDict[key][2] == 0:
                            number = len(sortDict)
                        else:
                            number = specialDirectoriesDict[key][2]
                        for count in range(number):
                            if count == len(sortDict):
                                break
                            specialDirectoriesDict[key][0].append(deepcopy(itemFilter(result)[sortDict[sortedKeys[count]]]))

                if len(directoryFilter(result)):
                    for directory in directoryFilter(result):
                        if not len(itemFilter(directory)):
                            continue
                        tmpDirElement = etree.XML(u'<directory></directory>')
                        tmpDirElement.attrib['name'] = directory.attrib['name']
                        if directory.attrib.get('thumbnail'):
                            tmpDirElement.attrib['thumbnail'] = directory.attrib['thumbnail']
                        else:
                            tmpDirElement.attrib['thumbnail'] = self.channel_icon
                        count = 0
                        for item in itemFilter(directory):
                            tmpDirElement.append(item)
                            if urlMax:
                                count+=1
                                if count == urlMax:
                                    break
                        categoryElement.append(tmpDirElement)
                else:
                    # Process the results through its XSLT stylesheet and append the results to the
                    # directory date and time order
                    count = 0
                    for item in itemFilter(result):
                        categoryElement.append(item)
                        if urlMax:
                            count+=1
                            if count == urlMax:
                                break

            # Add the last directory processed and the "Special" directories
            if categoryElement != None:
                if len(itemFilter(categoryElement)):
                    channelTree.append(categoryElement)
                # Add the special directories videos
                for key in specialDirectoriesDict.keys():
                    if len(itemFilter(specialDirectoriesDict[key][0])):
                        channelTree.append(specialDirectoriesDict[key][0])

        # Check that there was at least some items
        if len(rssTree.xpath('//item')):
            # Output the MNV Mashup results
            sys.stdout.write(u'<?xml version="1.0" encoding="UTF-8"?>\n')
            sys.stdout.write(etree.tostring(rssTree, encoding='UTF-8', pretty_print=True))

        sys.exit(0)
    # end displayTreeView()
# end Videos() class
