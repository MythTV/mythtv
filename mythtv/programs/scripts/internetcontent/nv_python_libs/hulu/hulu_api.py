#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: hulu_api - Simple-to-use Python interface to the Hulu RSS feeds
#                       (http://www.hulu.com/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to
#           search and access text metadata, video and image URLs from Hulu Web site.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="hulu_api - Simple-to-use Python interface to the Hulu RSS feeds (http://www.hulu.com/)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from the Hulu Web site. These routines process RSS feeds
provided by Hulu (http://www.hulu.com/). The specific Hulu RSS feeds that are processed are controled through a user XML preference file usually found at
"~/.mythtv/MythNetvision/userGrabberPrefs/hulu.xml"
'''

__version__="v0.1.1"
# 0.1.0 Initial development
# 0.1.1 Changed the logger to only output to stderr rather than a file

import os, struct, sys, re, time, datetime, shutil, urllib
from string import capitalize
import logging
from socket import gethostname, gethostbyname
from threading import Thread
from copy import deepcopy
from operator import itemgetter, attrgetter

from hulu_exceptions import (HuluUrlError, HuluHttpError, HuluRssError, HuluVideoNotFound, HuluConfigFileError, HuluUrlDownloadError)

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


# Find out if the MythTV python bindings can be accessed and instances can created
try:
    '''If the MythTV python interface is found, required to access Netvision icon directory settings
    '''
    from MythTV import MythDB, MythLog
    mythdb = None
    try:
        '''Create an instance of each: MythDB
        '''
        MythLog._setlevel('none') # Some non option -M cannot have any logging on stdout
        mythdb = MythDB()
    except MythError, e:
        sys.stderr.write(u'\n! Warning - %s\n' % e.args[0])
        filename = os.path.expanduser("~")+'/.mythtv/config.xml'
        if not os.path.isfile(filename):
            sys.stderr.write(u'\n! Warning - A correctly configured (%s) file must exist\n' % filename)
        else:
            sys.stderr.write(u'\n! Warning - Check that (%s) is correctly configured\n' % filename)
    except Exception, e:
        sys.stderr.write(u"\n! Warning - Creating an instance caused an error for one of: MythDB. error(%s)\n" % e)
except Exception, e:
    sys.stderr.write(u"\n! Warning - MythTV python bindings could not be imported. error(%s)\n" % e)
    mythdb = None


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
    """Main interface to http://www.hulu.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their
    site target.

    Supports search methods
    The apikey is a not required to access http://www.hulu.com/
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
            pass    # Hulu does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = u'Hulu_Grabber'
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'HuluUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'HuluHttpError': u"! Error: An HTTP communications error with the Hulu was raised (%s)\n", 'HuluRssError': u"! Error: Invalid RSS meta data\nwas received from the Hulu error (%s). Skipping item.\n", 'HuluVideoNotFound': u"! Error: Video search with the Hulu did not return any results (%s)\n", 'HuluConfigFileError': u"! Error: hulu_config.xml file missing\nit should be located in and named as (%s).\n", 'HuluUrlDownloadError': u"! Error: Downloading a RSS feed or Web page (%s).\n", }

        # Channel details and search results
        self.channel = {'channel_title': u'Hulu', 'channel_link': u'http://www.hulu.com/', 'channel_description': u"Hulu.com is a free online video service that offers hit TV shows including Family Guy, 30 Rock, and the Daily Show with Jon Stewart, etc.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        # Season and Episode detection regex patterns
        self.s_e_Patterns = [
            # Title: "s18 | e87"
            re.compile(u'''^.+?[Ss](?P<seasno>[0-9]+).*.+?[Ee](?P<epno>[0-9]+).*$''', re.UNICODE),
            # Description: "season 1, episode 5"
            re.compile(u'''^.+?season\ (?P<seasno>[0-9]+).*.+?episode\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # Thubnail: "http://media.thewb.com/thewb/images/thumbs/firefly/01/firefly_01_07.jpg"
            re.compile(u'''(?P<seriesname>[^_]+)\\_(?P<seasno>[0-9]+)\\_(?P<epno>[0-9]+).*$''', re.UNICODE),
            ]

        self.channel_icon = u'http://upload.wikimedia.org/wikipedia/commons/thumb/7/76/Hulu_logo.svg/300px-Hulu_logo.svg.png'

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions

        if mythdb:
            self.icon_dir = mythdb.settings[gethostname()]['mythnetvision.iconDir']
            if self.icon_dir:
                self.icon_dir = self.icon_dir.replace(u'//', u'/')
                self.setTreeViewIcon(dir_icon='hulu')
                self.channel_icon = self.tree_dir_icon
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
            if not self.icon_dir:
                return self.tree_dir_icon
            if not self.feed_icons.has_key(self.tree_key):
                return self.tree_dir_icon
            if not self.feed_icons[self.tree_key].has_key(self.feed):
                return self.tree_dir_icon
            dir_icon = self.feed_icons[self.tree_key][self.feed]
        for ext in self.config[u'image_extentions']:
            icon = u'%s%s.%s' % (self.icon_dir, dir_icon, ext)
            if os.path.isfile(icon):
                self.tree_dir_icon = icon
                break
        return self.tree_dir_icon
    # end setTreeViewIcon()


    def getHuluConfig(self):
        ''' Read the MNV Hulu grabber "hulu_config.xml" configuration file
        return nothing
        '''
        # Read the grabber hulu_config.xml configuration file
        url = u'file://%s/nv_python_libs/configs/XML/hulu_config.xml' % (baseProcessingDir, )
        if not os.path.isfile(url[7:]):
            raise HuluConfigFileError(self.error_messages['HuluConfigFileError'] % (url[7:], ))

        if self.config['debug_enabled']:
            print url
            print
        try:
            self.hulu_config = etree.parse(url)
        except Exception, e:
            raise HuluUrlError(self.error_messages['HuluUrlError'] % (url, errormsg))
        return
    # end getHuluConfig()


    def getUserPreferences(self):
        '''Read the hulu_config.xml and user preference hulu.xml file.
        If the hulu.xml file does not exist then copy the default.
        return nothing
        '''
        # Get hulu_config.xml
        self.getHuluConfig()

        # Check if the hulu.xml file exists
        userPreferenceFile = self.hulu_config.find('userPreferenceFile').text
        if userPreferenceFile[0] == '~':
             self.hulu_config.find('userPreferenceFile').text = u"%s%s" % (os.path.expanduser(u"~"), userPreferenceFile[1:])

        # If the user config file does not exists then copy one the default
        if not os.path.isfile(self.hulu_config.find('userPreferenceFile').text):
            # Make the necessary directories if they do not already exist
            prefDir = self.hulu_config.find('userPreferenceFile').text.replace(u'/hulu.xml', u'')
            if not os.path.isdir(prefDir):
                os.makedirs(prefDir)
            defaultConfig = u'%s/nv_python_libs/configs/XML/defaultUserPrefs/hulu.xml' % (baseProcessingDir, )
            shutil.copy2(defaultConfig, self.hulu_config.find('userPreferenceFile').text)

        # Read the grabber hulu_config.xml configuration file
        url = u'file://%s' % (self.hulu_config.find('userPreferenceFile').text, )
        if self.config['debug_enabled']:
            print url
            print
        try:
            self.userPrefs = etree.parse(url)
        except Exception, e:
            raise HuluUrlError(self.error_messages['HuluUrlError'] % (url, errormsg))
        return
    # end getUserPreferences()

    def getSeasonEpisode(self, title, desc=None, thumbnail=None):
        ''' Check is there is any season or episode number information in an item's title
        return array of season and/or episode numbers, Series name (only if title empty)
        return array with None values
        '''
        s_e = [None, None, None]
        if title:
            match = self.s_e_Patterns[0].match(title)
            if match:
                s_e[0], s_e[1] = match.groups()
        if not s_e[0] and desc:
            match = self.s_e_Patterns[1].match(desc)
            if match:
                s_e[0], s_e[1] = match.groups()
        if thumbnail and not title:
            filepath, filename = os.path.split( thumbnail.replace(u'http:/', u'') )
            match = self.s_e_Patterns[2].match(filename)
            if match:
                s_e[2], s_e[0], s_e[1] = match.groups()
                s_e[0] = u'%s' % int(s_e[0])
                s_e[1] = u'%s' % int(s_e[1])
                s_e[2] = "".join([capitalize(w) for w in re.split(re.compile("[\W_]*"), s_e[2].replace(u'_', u' ').replace(u'-', u' '))])
        return s_e
    # end getSeasonEpisode()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################


    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the Hulu web site
        return an array of matching item elements
        return
        '''
        # Save the origninal URL
        orgUrl = self.hulu_config.find('searchURLS').xpath(".//href")[0].text

        url = self.hulu_config.find('searchURLS').xpath(".//href")[0].text.replace(u'PAGENUM', unicode(pagenumber)).replace(u'SEARCHTERM', urllib.quote_plus(title.encode("utf-8")))

        if self.config['debug_enabled']:
            print url
            print

        self.hulu_config.find('searchURLS').xpath(".//href")[0].text = url

        # Perform a search
        try:
            resultTree = self.common.getUrlData(self.hulu_config.find('searchURLS'))
        except Exception, errormsg:
            # Restore the origninal URL
            self.hulu_config.find('searchURLS').xpath(".//href")[0].text = orgUrl
            raise HuluUrlDownloadError(self.error_messages['HuluUrlDownloadError'] % (errormsg))

        # Restore the origninal URL
        self.hulu_config.find('searchURLS').xpath(".//href")[0].text = orgUrl

        if resultTree is None:
            raise HuluVideoNotFound(u"No Hulu Video matches found for search value (%s)" % title)

        searchResults = resultTree.xpath('//result//a[@href!="#"]')
        if not len(searchResults):
            raise HuluVideoNotFound(u"No Hulu Video matches found for search value (%s)" % title)

        if self.config['debug_enabled']:
            print "resultTree: count(%s)" % len(searchResults)
            print

        # Hulu search results do not have a pubDate so use the current data time
        # e.g. "Sun, 06 Jan 2008 21:44:36 GMT"
        pubDate = datetime.datetime.now().strftime(self.common.pubDateFormat)

        # Translate the search results into MNV RSS item format
        titleFilter = etree.XPath(u".//img")
        thumbnailFilter = etree.XPath(u".//img")
        itemLink = etree.XPath('.//media:content', namespaces=self.common.namespaces)
        itemThumbnail = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
        itemDict = {}
        for result in searchResults:
            tmpLink = result.attrib['href']
            if not tmpLink:   # Make sure that this result actually has a video
                continue
            huluItem = etree.XML(self.common.mnvItem)
            # Extract and massage data
            link = self.common.ampReplace(tmpLink)
            tmpTitleText = titleFilter(result)[0].attrib['alt'].strip()
            tmpList = tmpTitleText.split(u':')
            title = self.common.massageText(tmpList[0].strip())
            if len(tmpList) > 1:
                description = self.common.massageText(tmpList[1].strip())
            else:
                description = u''

            # Insert data into a new item element
            huluItem.find('title').text = title
            huluItem.find('author').text = u'Hulu'
            huluItem.find('pubDate').text = pubDate
            huluItem.find('description').text = description
            huluItem.find('link').text = link
            itemThumbnail(huluItem)[0].attrib['url'] = self.common.ampReplace(thumbnailFilter(result)[0].attrib['src'])
            itemLink(huluItem)[0].attrib['url'] = link
            etree.SubElement(huluItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = u'us'
            s_e = self.getSeasonEpisode(title, description, itemThumbnail(huluItem)[0].attrib['url'])
            if s_e[0]:
                etree.SubElement(huluItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = s_e[0]
            if s_e[1]:
                etree.SubElement(huluItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = s_e[1]
            if not title and s_e[2]:
                huluItem.find('title').text = s_e[2]
            itemDict[link] = huluItem

        if not len(itemDict.keys()):
            raise HuluVideoNotFound(u"No Hulu Video matches found for search value (%s)" % title)

        # Set the number of search results returned
        self.channel['channel_numresults'] = len(itemDict)

        # Check if there are any more pages
        lastPage = resultTree.xpath('//result//a[@alt="Go to the last page"]')
        morePages = False
        if len(lastPage):
            try:
                if pagenumber < lastPage[0].text:
                    morePages = True
            except:
                pass

        return [itemDict, morePages]
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
        except HuluVideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            sys.exit(0)
        except HuluUrlError, msg:
            sys.stderr.write(u'%s\n' % msg)
            sys.exit(1)
        except HuluHttpError, msg:
            sys.stderr.write(self.error_messages['HuluHttpError'] % msg)
            sys.exit(1)
        except HuluRssError, msg:
            sys.stderr.write(self.error_messages['HuluRssError'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        if self.config['debug_enabled']:
            print "Number of items returned by the search(%s)" % len(data[0].keys())
            sys.stdout.write(etree.tostring(self.userPrefs, encoding='UTF-8', pretty_print=True))
            print

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+u'</rss>')

        # Set the paging values
        itemCount = len(data[0].keys())
        if data[1]:
            self.channel['channel_returned'] = itemCount
            self.channel['channel_startindex'] = self.page_limit*(int(pagenumber)-1)
            self.channel['channel_numresults'] = itemCount+(self.page_limit*(int(pagenumber)-1)+1)
        else:
            self.channel['channel_returned'] = itemCount
            self.channel['channel_startindex'] = itemCount+(self.page_limit*(int(pagenumber)-1))
            self.channel['channel_numresults'] = itemCount+(self.page_limit*(int(pagenumber)-1))

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
        '''Gather the Hulu feeds then get a max page of videos meta data in each of them
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
                    except HuluVideoNotFound, msg:
                        sys.stderr.write(u"%s\n" % msg)
                        continue
                    except HuluUrlError, msg:
                        sys.stderr.write(u'%s\n' % msg)
                        continue
                    except HuluHttpError, msg:
                        sys.stderr.write(self.error_messages['HuluHttpError'] % msg)
                        continue
                    except HuluRssError, msg:
                        sys.stderr.write(self.error_messages['HuluRssError'] % msg)
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
        for feedType in [u'treeviewURLS', ]:
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
                etree.SubElement(url, "filter").text = u"//channel/title"
                etree.SubElement(url, "filter").text = u"//item"
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
                raise HuluUrlDownloadError(self.error_messages['HuluUrlDownloadError'] % (errormsg))
            if self.config['debug_enabled']:
                print "resultTree:"
                sys.stdout.write(etree.tostring(resultTree, encoding='UTF-8', pretty_print=True))
                print

            # Process each directory of the user preferences that have an enabled rss feed
            itemFilter = etree.XPath('.//item', namespaces=self.common.namespaces)
            titleFilter = etree.XPath('.//title', namespaces=self.common.namespaces)
            linkFilter = etree.XPath('.//link', namespaces=self.common.namespaces)
            descriptionFilter = etree.XPath('.//description', namespaces=self.common.namespaces)
            authorFilter = etree.XPath('.//media:credit', namespaces=self.common.namespaces)
            pubDateFilter = etree.XPath('.//pubDate', namespaces=self.common.namespaces)
            feedFilter = etree.XPath('//url[text()=$url]')
            descFilter2 = etree.XPath('.//p')
            itemThumbNail = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
            itemDwnLink = etree.XPath('.//media:content', namespaces=self.common.namespaces)
            itemLanguage = etree.XPath('.//media:content', namespaces=self.common.namespaces)
            itemDuration = etree.XPath('.//media:content', namespaces=self.common.namespaces)
            rssName = etree.XPath('title', namespaces=self.common.namespaces)
            categoryDir = None
            categoryElement = None
            for result in resultTree.findall('results'):
                names = result.find('name').text.split(u';')
                names[0] = self.common.massageText(names[0])
                if names[0] == 'RSS':
                    names[0] = self.common.massageText(rssName(result.find('result'))[0].text.replace(u'Hulu - ', u''))
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

                # Convert each RSS item into a MNV item
                for itemData in itemFilter(result.find('result')):
                    huluItem = etree.XML(self.common.mnvItem)
                    link = self.common.ampReplace(linkFilter(itemData)[0].text)
                    # Convert the pubDate "Sat, 01 May 2010 08:18:05 -0000" to a MNV pubdate format
                    pubdate = pubDateFilter(itemData)[0].text[:-5]+u'GMT'

                    # Extract and massage data also insert data into a new item element
                    huluItem.find('title').text = self.common.massageText(titleFilter(itemData)[0].text.strip())
                    if authorFilter(itemData)[0].text:
                        huluItem.find('author').text = self.common.massageText(authorFilter(itemData)[0].text.strip())
                    else:
                        huluItem.find('author').text = u'Hulu'
                    huluItem.find('pubDate').text = pubdate
                    description = etree.HTML(etree.tostring(descriptionFilter(itemData)[0], method="text", encoding=unicode).strip())
                    huluItem.find('description').text = self.common.massageText(descFilter2(description)[0].text.strip())
                    for e in descFilter2(description)[1]:
                        eText = etree.tostring(e, method="text", encoding=unicode)
                        if not eText:
                            continue
                        if eText.startswith(u'Duration: '):
                            eText = eText.replace(u'Duration: ', u'').strip()
                            videoSeconds = False
                            videoDuration = eText.split(u':')
                            try:
                                if len(videoDuration) == 1:
                                    videoSeconds = int(videoDuration[0])
                                elif len(videoDuration) == 2:
                                    videoSeconds = int(videoDuration[0])*60+int(videoDuration[1])
                                elif len(videoDuration) == 3:
                                    videoSeconds = int(videoDuration[0])*3600+int(videoDuration[1])*60+int(videoDuration[2])
                                if videoSeconds:
                                    itemDwnLink(huluItem)[0].attrib['duration'] = unicode(videoSeconds)
                            except:
                                pass
                        elif eText.startswith(u'Rating: '):
                            eText = eText.replace(u'Rating: ', u'').strip()
                            videoRating = eText.split(u' ')
                            huluItem.find('rating').text = videoRating[0]
                        continue
                    huluItem.find('link').text = link
                    itemDwnLink(huluItem)[0].attrib['url'] = link
                    try:
                        itemThumbNail(huluItem)[0].attrib['url'] = self.common.ampReplace(itemThumbNail(itemData)[0].attrib['url'])
                    except IndexError:
                        pass
                    itemLanguage(huluItem)[0].attrib['lang'] = channelLanguage
                    etree.SubElement(huluItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = u'us'
                    s_e = self.getSeasonEpisode(huluItem.find('title').text, huluItem.find('description').text, itemThumbNail(huluItem)[0].attrib['url'])
                    if s_e[0]:
                        etree.SubElement(huluItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = s_e[0]
                    if s_e[1]:
                        etree.SubElement(huluItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = s_e[1]
                    if not huluItem.find('title').text and s_e[2]:
                        huluItem.find('title').text = s_e[2]
                    categoryElement.append(huluItem)
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
