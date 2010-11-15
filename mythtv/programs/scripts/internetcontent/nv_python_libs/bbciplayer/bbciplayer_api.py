#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: bbciplayer_api - Simple-to-use Python interface to the BBC iPlayer RSS feeds
#                       (http://www.bbc.co.uk)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to
#           search and access text metadata, video and image URLs from BBC iPlayer Web site.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="bbciplayer_api - Simple-to-use Python interface to the BBC iPlayer RSS feeds (http://www.bbc.co.uk)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from the BBC iPlayer Web site. These routines process RSS feeds
provided by BBC (http://www.bbc.co.uk). The specific BBC iPlayer RSS feeds that are processed are controled through a user XML preference file usually found at
"~/.mythtv/MythNetvision/userGrabberPrefs/bbciplayer.xml"
'''

__version__="v0.1.3"
# 0.1.0 Initial development
# 0.1.1 Changed the logger to only output to stderr rather than a file
# 0.1.2 Fixed incorrect URL creation for RSS feed Web pages
#       Restricted custom HTML web pages to Video media only. Audio will only play from its Web page.
#       Add the "customhtml" tag to search and treeviews
#       Removed the need for python MythTV bindings and added "%SHAREDIR%" to icon directory path
# 0.1.3 Fixed search due to BBC Web site changes

import os, struct, sys, re, time, datetime, shutil, urllib, re
import logging
from socket import gethostname, gethostbyname
from threading import Thread
from copy import deepcopy
from operator import itemgetter, attrgetter

from bbciplayer_exceptions import (BBCUrlError, BBCHttpError, BBCRssError, BBCVideoNotFound, BBCConfigFileError, BBCUrlDownloadError)

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
    """Main interface to http://www.bbciplayer.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search methods
    The apikey is a not required to access http://www.bbciplayer.com/
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
            pass    # BBC does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = u'BBCiPlayer_Grabber'
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'BBCUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'BBCHttpError': u"! Error: An HTTP communications error with the BBC was raised (%s)\n", 'BBCRssError': u"! Error: Invalid RSS meta data\nwas received from the BBC error (%s). Skipping item.\n", 'BBCVideoNotFound': u"! Error: Video search with the BBC did not return any results (%s)\n", 'BBCConfigFileError': u"! Error: bbc_config.xml file missing\nit should be located in and named as (%s).\n", 'BBCUrlDownloadError': u"! Error: Downloading a RSS feed or Web page (%s).\n", }

        # Channel details and search results
        self.channel = {'channel_title': u'BBC iPlayer', 'channel_link': u'http://www.bbc.co.uk', 'channel_description': u"BBC iPlayer is our service that lets you catch up with radio and television programmes from the past week.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        # XPath parsers used to detect a video type of item
        self.countryCodeParsers = [
            etree.XPath('.//a[@class="episode-title title-link cta-video"]', namespaces=self.common.namespaces),
            etree.XPath('.//div[@class="feature video"]', namespaces=self.common.namespaces),
            etree.XPath('.//atm:category[@term="TV"]', namespaces=self.common.namespaces),
            ]

        # Season and Episode detection regex patterns
        self.s_e_Patterns = [
            # "Series 7 - Episode 4" or "Series 7 - Episode 4" or "Series 7: On Holiday: Episode 10"
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+).*.+?Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 5 - 1
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+)\\ \\-\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 1 - Warriors of Kudlak - Part 2
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+).*.+?Part\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 3: Programme 3
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+)\\:\\ Programme\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Series 3:
            re.compile(u'''^.+?Series\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            # Episode 1
            re.compile(u'''^.+?Episode\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            ]

        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/bbciplayer.jpg'

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions
    # end __init__()

###########################################################################################################
#
# Start - Utility functions
#
###########################################################################################################

    def getBBCConfig(self):
        ''' Read the MNV BBC iPlayer grabber "bbc_config.xml" configuration file
        return nothing
        '''
        # Read the grabber bbciplayer_config.xml configuration file
        url = u'file://%s/nv_python_libs/configs/XML/bbc_config.xml' % (baseProcessingDir, )
        if not os.path.isfile(url[7:]):
            raise BBCConfigFileError(self.error_messages['BBCConfigFileError'] % (url[7:], ))

        if self.config['debug_enabled']:
            print url
            print
        try:
            self.bbciplayer_config = etree.parse(url)
        except Exception, e:
            raise BBCUrlError(self.error_messages['BBCUrlError'] % (url, errormsg))
        return
    # end getBBCConfig()


    def getUserPreferences(self):
        '''Read the bbciplayer_config.xml and user preference bbciplayer.xml file.
        If the bbciplayer.xml file does not exist then copy the default.
        return nothing
        '''
        # Get bbciplayer_config.xml
        self.getBBCConfig()

        # Check if the bbciplayer.xml file exists
        userPreferenceFile = self.bbciplayer_config.find('userPreferenceFile').text
        if userPreferenceFile[0] == '~':
             self.bbciplayer_config.find('userPreferenceFile').text = u"%s%s" % (os.path.expanduser(u"~"), userPreferenceFile[1:])

        # If the user config file does not exists then copy one from the default
        if not os.path.isfile(self.bbciplayer_config.find('userPreferenceFile').text):
            # Make the necessary directories if they do not already exist
            prefDir = self.bbciplayer_config.find('userPreferenceFile').text.replace(u'/bbciplayer.xml', u'')
            if not os.path.isdir(prefDir):
                os.makedirs(prefDir)
            defaultConfig = u'%s/nv_python_libs/configs/XML/defaultUserPrefs/bbciplayer.xml' % (baseProcessingDir, )
            shutil.copy2(defaultConfig, self.bbciplayer_config.find('userPreferenceFile').text)

        # Read the grabber bbciplayer_config.xml configuration file
        url = u'file://%s' % (self.bbciplayer_config.find('userPreferenceFile').text, )
        if self.config['debug_enabled']:
            print url
            print
        try:
            self.userPrefs = etree.parse(url)
        except Exception, e:
            raise BBCUrlError(self.error_messages['BBCUrlError'] % (url, errormsg))
        return
    # end getUserPreferences()

    def setCountry(self, item):
        '''Parse the item information (HTML or RSS/XML) to identify if the content is a video or
        audio file. Set the contry code if a video is detected as it can only be played in the "UK"
        return "uk" if a video type was detected.
        return None if a video type was NOT detected.
        '''
        countryCode = None
        for xpathP in self.countryCodeParsers:
            if len(xpathP(item)):
                countryCode = u'uk'
                break
        return countryCode
    # end setCountry()


    def getSeasonEpisode(self, title):
        ''' Check is there is any season or episode number information in an item's title
        return array of season and/or episode numbers
        return array with None values
        '''
        s_e = [None, None]
        for index in range(len(self.s_e_Patterns)):
            match = self.s_e_Patterns[index].match(title)
            if not match:
                continue
            if index < 4:
                s_e[0], s_e[1] = match.groups()
                break
            elif index == 4:
                s_e[0] = match.groups()[0]
                break
            elif index == 5:
                s_e[1] = match.groups()[0]
                break
        return s_e
    # end getSeasonEpisode()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################


    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the BBC iPlayer web site
        return an array of matching item elements
        return
        '''
        # Save the origninal URL
        orgUrl = self.bbciplayer_config.find('searchURLS').xpath(".//href")[0].text

        try:
            searchVar = u'/?q=%s&page=%s' % (urllib.quote(title.encode("utf-8")), pagenumber)
        except UnicodeDecodeError:
            searchVar = u'/?q=%s&page=%s' % (urllib.quote(title), pagenumber)

        url = self.bbciplayer_config.find('searchURLS').xpath(".//href")[0].text+searchVar

        if self.config['debug_enabled']:
            print url
            print

        self.bbciplayer_config.find('searchURLS').xpath(".//href")[0].text = url

        # Perform a search
        try:
            resultTree = self.common.getUrlData(self.bbciplayer_config.find('searchURLS'), pageFilter=self.bbciplayer_config.find('searchURLS').xpath(".//pageFilter")[0].text)
        except Exception, errormsg:
            # Restore the origninal URL
            self.bbciplayer_config.find('searchURLS').xpath(".//href")[0].text = orgUrl
            raise BBCUrlDownloadError(self.error_messages['BBCUrlDownloadError'] % (errormsg))

        # Restore the origninal URL
        self.bbciplayer_config.find('searchURLS').xpath(".//href")[0].text = orgUrl

        if resultTree is None:
            raise BBCVideoNotFound(u"No BBC Video matches found for search value (%s)" % title)

        searchResults = resultTree.xpath('//result//li')
        if not len(searchResults):
            raise BBCVideoNotFound(u"No BBC Video matches found for search value (%s)" % title)

        # BBC iPlayer search results fo not have a pubDate so use the current data time
        # e.g. "Sun, 06 Jan 2008 21:44:36 GMT"
        pubDate = datetime.datetime.now().strftime(self.common.pubDateFormat)

        # Set the display type for the link (Fullscreen, Web page, Game Console)
        if self.userPrefs.find('displayURL') != None:
            urlType = self.userPrefs.find('displayURL').text
        else:
            urlType = u'fullscreen'

        # Translate the search results into MNV RSS item format
        audioFilter = etree.XPath('./@class="audio"')
        linkFilter = etree.XPath(u".//div[@class='episode-info']//a")
        titleFilter = etree.XPath(u".//div[@class='episode-info']//a")
        descFilter = etree.XPath(u".//div[@class='episode-info']//p[@class='episode-synopsis']")
        thumbnailFilter = etree.XPath(u".//div[@class='episode-image']//img")
        itemDict = {}
        for result in searchResults:
            tmpLink = linkFilter(result)
            if not len(tmpLink):   # Make sure that this result actually has a video
                continue
            bbciplayerItem = etree.XML(self.common.mnvItem)
            # Is this an Audio or Video item (true/false)
            audioTF = audioFilter(result)
            # Extract and massage data
            link = tmpLink[0].attrib['href']
            if urlType == 'bigscreen':
                link = u'http://www.bbc.co.uk/iplayer/bigscreen%s' % link.replace(u'/iplayer',u'')
            elif urlType == 'bbcweb':
                link = u'http://www.bbc.co.uk'+ link
            else:
                if not audioTF:
                    link = link.replace(u'/iplayer/episode/', u'')
                    index = link.find(u'/')
                    link = link[:index]
                    link = u'file://%s/nv_python_libs/configs/HTML/bbciplayer.html?videocode=%s' % (baseProcessingDir, link)
                    etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}customhtml").text = 'true'
                else: # Audio media will not play in the embedded HTML page
                    link = u'http://www.bbc.co.uk'+ link

            title = self.common.massageText(titleFilter(result)[0].attrib['title'].strip())
            description = self.common.massageText(etree.tostring(descFilter(result)[0], method="text", encoding=unicode).strip())
            link = self.common.ampReplace(link)

            # Insert data into a new item element
            bbciplayerItem.find('title').text = title
            bbciplayerItem.find('author').text = u'BBC'
            bbciplayerItem.find('pubDate').text = pubDate
            bbciplayerItem.find('description').text = description
            bbciplayerItem.find('link').text = link
            bbciplayerItem.xpath('.//media:thumbnail', namespaces=self.common.namespaces)[0].attrib['url'] = self.common.ampReplace(thumbnailFilter(result)[0].attrib['src'])
            bbciplayerItem.xpath('.//media:content', namespaces=self.common.namespaces)[0].attrib['url'] = link
            # Videos are only viewable in the UK so add a country indicator if this is a video
            if audioTF:
                countCode = None
            else:
                countCode = u'uk'
            if countCode:
                etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = countCode
            s_e = self.getSeasonEpisode(title)
            if s_e[0]:
                etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = s_e[0]
            if s_e[1]:
                etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = s_e[1]
            itemDict[title.lower()] = bbciplayerItem

        if not len(itemDict.keys()):
            raise BBCVideoNotFound(u"No BBC Video matches found for search value (%s)" % title)

        # Set the number of search results returned
        self.channel['channel_numresults'] = len(itemDict)

        return [itemDict, resultTree.xpath('//pageInfo')[0].text]
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
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


        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        try:
            data = self.searchTitle(title, pagenumber, self.page_limit)
        except BBCVideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            sys.exit(0)
        except BBCUrlError, msg:
            sys.stderr.write(u'%s\n' % msg)
            sys.exit(1)
        except BBCHttpError, msg:
            sys.stderr.write(self.error_messages['BBCHttpError'] % msg)
            sys.exit(1)
        except BBCRssError, msg:
            sys.stderr.write(self.error_messages['BBCRssError'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+u'</rss>')

        # Set the paging values
        itemCount = len(data[0].keys())
        if data[1] == 'true':
            self.channel['channel_returned'] = itemCount
            self.channel['channel_startindex'] = itemCount
            self.channel['channel_numresults'] = itemCount+(self.page_limit*(int(pagenumber)-1)+1)
        else:
            self.channel['channel_returned'] = itemCount+(self.page_limit*(int(pagenumber)-1))
            self.channel['channel_startindex'] = self.channel['channel_returned']
            self.channel['channel_numresults'] = self.channel['channel_returned']

        # Add the Channel element tree
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)

        lastKey = None
        for key in sorted(data[0].keys()):
            if lastKey != key:
                channelTree.append(data[0][key])
                lastKey = key

        # Output the MNV search results
        sys.stdout.write(u'<?xml version="1.0" encoding="UTF-8"?>\n')
        sys.stdout.write(etree.tostring(rssTree, encoding='UTF-8', pretty_print=True))
        sys.exit(0)
    # end searchForVideos()

    def displayTreeView(self):
        '''Gather the BBC iPlayer feeds then get a max page of videos meta data in each of them
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
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)

        # Process any user specified searches
        searchResultTree = []
        searchFilter = etree.XPath(u"//item")
        userSearchStrings = u'userSearchStrings'
        if self.userPrefs.find(userSearchStrings) != None:
            userSearch = self.userPrefs.find(userSearchStrings).xpath('./userSearch')
            if len(userSearch):
                for searchDetails in userSearch:
                    try:
                        data = self.searchTitle(searchDetails.find('searchTerm').text, 1, self.page_limit)
                    except BBCVideoNotFound, msg:
                        sys.stderr.write(u"%s\n" % msg)
                        continue
                    except BBCUrlError, msg:
                        sys.stderr.write(u'%s\n' % msg)
                        continue
                    except BBCHttpError, msg:
                        sys.stderr.write(self.error_messages['BBCHttpError'] % msg)
                        continue
                    except BBCRssError, msg:
                        sys.stderr.write(self.error_messages['BBCRssError'] % msg)
                        continue
                    except Exception, e:
                        sys.stderr.write(u"! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (title, e))
                        continue
                    dirElement = etree.XML(u'<directory></directory>')
                    dirElement.attrib['name'] = self.common.massageText(searchDetails.find('dirName').text)
                    dirElement.attrib['thumbnail'] = self.channel_icon
                    lastKey = None
                    for key in sorted(data[0].keys()):
                        if lastKey != key:
                            dirElement.append(data[0][key])
                            lastKey = key
                    channelTree.append(dirElement)
                    continue

        # Create a structure of feeds that can be concurrently downloaded
        rssData = etree.XML(u'<xml></xml>')
        for feedType in [u'treeviewURLS', u'userFeeds']:
            if self.userPrefs.find(feedType) == None:
                continue
            if not len(self.userPrefs.find(feedType).xpath('./url')):
                continue
            for rssFeed in self.userPrefs.find(feedType).xpath('./url'):
                urlEnabled = rssFeed.attrib.get('enabled')
                if urlEnabled == 'false':
                    continue
                urlName = rssFeed.attrib.get('name')
                if urlName:
                     uniqueName = u'%s;%s' % (urlName, rssFeed.text)
                else:
                    uniqueName = u'RSS;%s' % (rssFeed.text)
                url = etree.XML(u'<url></url>')
                etree.SubElement(url, "name").text = uniqueName
                etree.SubElement(url, "href").text = rssFeed.text
                etree.SubElement(url, "filter").text = u"atm:title"
                etree.SubElement(url, "filter").text = u"//atm:entry"
                etree.SubElement(url, "parserType").text = u'xml'
                rssData.append(url)

        if self.config['debug_enabled']:
            print "rssData:"
            sys.stdout.write(etree.tostring(rssData, encoding='UTF-8', pretty_print=True))
            print

        # Get the RSS Feed data
        if rssData.find('url') != None:
            try:
                resultTree = self.common.getUrlData(rssData)
            except Exception, errormsg:
                raise BBCUrlDownloadError(self.error_messages['BBCUrlDownloadError'] % (errormsg))
            if self.config['debug_enabled']:
                print "resultTree:"
                sys.stdout.write(etree.tostring(resultTree, encoding='UTF-8', pretty_print=True))
                print

             # Set the display type for the link (Fullscreen, Web page, Game Console)
            if self.userPrefs.find('displayURL') != None:
                urlType = self.userPrefs.find('displayURL').text
            else:
                urlType = u'fullscreen'

            # Process each directory of the user preferences that have an enabled rss feed
            feedFilter = etree.XPath('//url[text()=$url]')
            itemFilter = etree.XPath('.//atm:entry', namespaces=self.common.namespaces)
            titleFilter = etree.XPath('.//atm:title', namespaces=self.common.namespaces)
            mediaFilter = etree.XPath('.//atm:category[@term="TV"]', namespaces=self.common.namespaces)
            linkFilter = etree.XPath('.//atm:link', namespaces=self.common.namespaces)
            descFilter1 = etree.XPath('.//atm:content', namespaces=self.common.namespaces)
            descFilter2 = etree.XPath('.//p')
            itemThumbNail = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
            creationDate = etree.XPath('.//atm:updated', namespaces=self.common.namespaces)
            itemDwnLink = etree.XPath('.//media:content', namespaces=self.common.namespaces)
            itemLanguage = etree.XPath('.//media:content', namespaces=self.common.namespaces)
            rssName = etree.XPath('atm:title', namespaces=self.common.namespaces)
            categoryDir = None
            categoryElement = None
            itemAuthor = u'BBC'
            for result in resultTree.findall('results'):
                names = result.find('name').text.split(u';')
                names[0] = self.common.massageText(names[0])
                if names[0] == 'RSS':
                    names[0] = self.common.massageText(rssName(result.find('result'))[0].text.replace(u'BBC iPlayer - ', u''))
                count = 0
                urlMax = None
                url = feedFilter(self.userPrefs, url=names[1])
                if len(url):
                    if url[0].attrib.get('max'):
                        try:
                            urlMax = int(url[0].attrib.get('max'))
                        except:
                            pass
                    elif url[0].getparent().attrib.get('globalmax'):
                        try:
                            urlMax = int(url[0].getparent().attrib.get('globalmax'))
                        except:
                            pass
                    if urlMax == 0:
                        urlMax = None
                channelThumbnail = self.channel_icon
                channelLanguage = u'en'
                # Create a new directory and/or subdirectory if required
                if names[0] != categoryDir:
                    if categoryDir != None:
                        channelTree.append(categoryElement)
                    categoryElement = etree.XML(u'<directory></directory>')
                    categoryElement.attrib['name'] = names[0]
                    categoryElement.attrib['thumbnail'] = self.channel_icon
                    categoryDir = names[0]

                if self.config['debug_enabled']:
                    print "Results: #Items(%s) for (%s)" % (len(itemFilter(result)), names)
                    print

                # Create a list of item elements in pubDate order to that the items are processed in
                # date and time order
                itemDict = [(pd.text, pd.getparent()) for pd in creationDate(result)]
                itemList = sorted(itemDict, key=itemgetter(0), reverse=True)
                # Convert each RSS item into a MNV item
                for tupleDate in itemList:
                    itemData = tupleDate[1]
                    bbciplayerItem = etree.XML(self.common.mnvItem)
                    tmpLink = linkFilter(itemData)
                    if len(tmpLink):   # Make sure that this result actually has a video
                        link = tmpLink[0].attrib['href']
                        if urlType == 'bigscreen':
                            link = link.replace(u'/iplayer/', u'/iplayer/bigscreen/')
                        elif urlType == 'bbcweb':
                            pass    # Link does not need adjustments
                        else:
                            if len(mediaFilter(itemData)):
                                link = link.replace(u'http://www.bbc.co.uk/iplayer/episode/', u'')
                                index = link.find(u'/')
                                link = link[:index]
                                link = u'file://%s/nv_python_libs/configs/HTML/bbciplayer.html?videocode=%s' % (baseProcessingDir, link)
                                etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}customhtml").text = 'true'
                            else:
                                pass # Radio media cannot be played within the embedded weg page
                    else:
                        continue
                    link = self.common.ampReplace(link)
                    # Convert the pubDate "2010-03-22T07:56:21Z" to a MNV pubdate format
                    pubdate = creationDate(itemData)
                    if len(pubdate):
                        pubdate = pubdate[0].text
                        pubdate = time.strptime(pubdate, '%Y-%m-%dT%H:%M:%SZ')
                        pubdate = time.strftime(self.common.pubDateFormat, pubdate)
                    else:
                        pubdate = datetime.datetime.now().strftime(self.common.pubDateFormat)

                    # Extract and massage data also insert data into a new item element
                    bbciplayerItem.find('title').text = self.common.massageText(titleFilter(itemData)[0].text.strip())
                    bbciplayerItem.find('author').text = itemAuthor
                    bbciplayerItem.find('pubDate').text = pubdate
                    description = etree.HTML(etree.tostring(descFilter1(itemData)[0], method="text", encoding=unicode).strip())
                    description = etree.tostring(descFilter2(description)[1], method="text", encoding=unicode).strip()
                    bbciplayerItem.find('description').text = self.common.massageText(description)
                    bbciplayerItem.find('link').text = link
                    itemDwnLink(bbciplayerItem)[0].attrib['url'] = link
                    try:
                        itemThumbNail(bbciplayerItem)[0].attrib['url'] = self.common.ampReplace(itemThumbNail(itemData)[0].attrib['url'])
                    except IndexError:
                        pass
                    itemLanguage(bbciplayerItem)[0].attrib['lang'] = channelLanguage
                    # Videos are only viewable in the UK so add a country indicator if this is a video
                    countCode = self.setCountry(itemData)
                    if countCode:
                        etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = countCode
                    s_e = self.getSeasonEpisode(bbciplayerItem.find('title').text)
                    if s_e[0]:
                        etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = s_e[0]
                    if s_e[1]:
                        etree.SubElement(bbciplayerItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = s_e[1]
                    categoryElement.append(bbciplayerItem)
                    if urlMax: # Check of the maximum items to processes has been met
                        count+=1
                        if count > urlMax:
                            break

            # Add the last directory processed
            if categoryElement != None:
                if categoryElement.xpath('.//item') != None:
                    channelTree.append(categoryElement)

        # Check that there was at least some items
        if len(rssTree.xpath('//item')):
            # Output the MNV search results
            sys.stdout.write(u'<?xml version="1.0" encoding="UTF-8"?>\n')
            sys.stdout.write(etree.tostring(rssTree, encoding='UTF-8', pretty_print=True))

        sys.exit(0)
    # end displayTreeView()
# end Videos() class
