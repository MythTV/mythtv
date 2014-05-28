#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: rev3_api - Simple-to-use Python interface to the Revision3 RSS feeds (http://revision3.com/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           metadata, video and image URLs from rev3. These routines are based on the api. Specifications
#           for this api are published at http://developer.rev3nservices.com/docs
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="rev3_api - Simple-to-use Python interface to the Revision3 RSS feeds (http://revision3.com/)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from rev3. These routines process RSS feeds provided by Revision3
(http://revision3.com/). The specific Revision3 RSS feeds that are processed are controled through
a user XML preference file usually found at "~/.mythtv/MythNetvision/userGrabberPrefs/rev3.xml"
'''

__version__="v0.1.4"
# 0.1.0 Initial development
# 0.1.1 Changed the search functionality to be "Videos" only rather than the site search.
#       Added support for Revision3's Personal RSS feed
#       Changed the logger to only output to stderr rather than a file
# 0.1.2 Fixed an abort when no RSS feed data was returned
# 0.1.3 Removed the need for python MythTV bindings and added "%SHAREDIR%" to icon directory path
# 0.1.4 Fixed missing shows from the creation of the user default preference due to Web site changes
#       Fixed two incorrect variable names in debug messages

import os, struct, sys, re, time, datetime, urllib, re
import logging
from socket import gethostname, gethostbyname
from threading import Thread
from copy import deepcopy

from rev3_exceptions import (Rev3UrlError, Rev3HttpError, Rev3RssError, Rev3VideoNotFound, Rev3ConfigFileError, Rev3UrlDownloadError)

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
    """Main interface to http://www.rev3.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search methods
    The apikey is a not required to access http://www.rev3.com/
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
            pass    # Rev3 does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = u'Rev3_Grabber'
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'Rev3UrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'Rev3HttpError': u"! Error: An HTTP communications error with Rev3 was raised (%s)\n", 'Rev3RssError': u"! Error: Invalid RSS meta data\nwas received from Rev3 error (%s). Skipping item.\n", 'Rev3VideoNotFound': u"! Error: Video search with Rev3 did not return any results (%s)\n", 'Rev3ConfigFileError': u"! Error: rev3_config.xml file missing\nit should be located in and named as (%s).\n", 'Rev3UrlDownloadError': u"! Error: Downloading a RSS feed or Web page (%s).\n", }

        # Channel details and search results
        self.channel = {'channel_title': u'Revision3', 'channel_link': u'http://revision3.com/', 'channel_description': u"Revision3 is the leading television network for the internet generation.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        # Season and/or Episode detection regex patterns
        self.s_e_Patterns = [
            # Season 3, Episode 8
            re.compile(u'''^.+?Season\\ (?P<seasno>[0-9]+).*.+?Episode\\ (?P<epno>[0-9]+).*$''', re.UNICODE),
            # "Episode 1" anywhere in text
            re.compile(u'''^.+?Episode\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            # "Episode 1" at the start of the text
            re.compile(u'''Episode\\ (?P<seasno>[0-9]+).*$''', re.UNICODE),
            # "--0027--" when the episode is in the URl link
            re.compile(u'''^.+?--(?P<seasno>[0-9]+)--.*$''', re.UNICODE),
            ]

        self.FullScreen = u'http://revision3.com/show/popupPlayer?video_id=%s&quality=high&offset=0'
        self.FullScreenParser = self.common.parsers['html'].copy()
        self.FullScreenVidIDxPath = etree.XPath('//object', namespaces=self.common.namespaces )

        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/rev3.png'
    # end __init__()

###########################################################################################################
#
# Start - Utility functions
#
###########################################################################################################

    def getRev3Config(self):
        ''' Read the MNV Revision3 grabber "rev3_config.xml" configuration file
        return nothing
        '''
        # Read the grabber rev3_config.xml configuration file
        url = u'file://%s/nv_python_libs/configs/XML/rev3_config.xml' % (baseProcessingDir, )
        if not os.path.isfile(url[7:]):
            raise Rev3ConfigFileError(self.error_messages['Rev3ConfigFileError'] % (url[7:], ))

        if self.config['debug_enabled']:
            print url
            print
        try:
            self.rev3_config = etree.parse(url)
        except Exception, e:
            raise Rev3UrlError(self.error_messages['Rev3UrlError'] % (url, errormsg))
        return
    # end getRev3Config()


    def getUserPreferences(self):
        '''Read the rev3_config.xml and user preference rev3.xml file.
        If the rev3.xml file does not exist then create it.
        If the rev3.xml file is too old then update it.
        return nothing
        '''
        # Get rev3_config.xml
        self.getRev3Config()

        # Check if the rev3.xml file exists
        userPreferenceFile = self.rev3_config.find('userPreferenceFile').text
        if userPreferenceFile[0] == '~':
             self.rev3_config.find('userPreferenceFile').text = u"%s%s" % (os.path.expanduser(u"~"), userPreferenceFile[1:])
        if os.path.isfile(self.rev3_config.find('userPreferenceFile').text):
            # Read the grabber rev3_config.xml configuration file
            url = u'file://%s' % (self.rev3_config.find('userPreferenceFile').text, )
            if self.config['debug_enabled']:
                print url
                print
            try:
                self.userPrefs = etree.parse(url)
            except Exception, e:
                raise Rev3UrlError(self.error_messages['Rev3UrlError'] % (url, errormsg))
            # Check if the rev3.xml file is too old
            nextUpdateSecs = int(self.userPrefs.find('updateDuration').text)*86400 # seconds in a day
            nextUpdate = time.localtime(os.path.getmtime(self.rev3_config.find('userPreferenceFile').text)+nextUpdateSecs)
            now = time.localtime()
            if nextUpdate > now:
                return
            create = False
        else:
            create = True

        # If required create/update the rev3.xml file
        self.updateRev3(create)
        return
    # end getUserPreferences()

    def updateRev3(self, create=False):
        ''' Create or update the rev3.xml user preferences file
        return nothing
        '''
        userRev3 = etree.XML(u'''
<userRev3>
<!--
    The shows are split into three top level directories which represent how Rev3 categories
    their videos. Each top level directory has one or more shows. Each show has one or more
    MP4 formats that can be downloaded. The formats represent various video quality levels.
    Initially only three shows are enabled. You must change a show's "mp4Format" element's
    "enabled" attribute to 'true'. When you change the attribute to 'true' that show's RSS feed
    will be added to the Rev3 tree view. You could activate more than one MP4 format but that
    would result in duplicates. With the exception of "Tekzilla" which is a show that has
    both a weekly and daily video RSS feed within the same show element.
    When the Rev3 Tree view is created it will have links to the video's web page but also a
    link to the MP4 file that you can download through the MythNetvision interface.
    If a show moves from one top level directory to another your show sections will be
    preserved as long as that format is available in the new top level directory.
    Updates to the "rev3.xml" file is made every X number of days as determined by the value of
    the "updateDuration" element in this file. The default is every 3 days.
-->
<!-- Number of days between updates to the config file -->
<updateDuration>3</updateDuration>

<!--
    Personal RSS feed.
    "globalmax" (optional) Is a way to limit the number of items processed per RSS feed for all
                treeview URLs. A value of zero (0) means there are no limitions.
    "max" (optional) Is a way to limit the number of items processed for an individual RSS feed.
          This value will override any "globalmax" setting. A value of zero (0) means
          there are no limitions and would be the same if the attribute was no included at all.
    "enabled" If you want to remove a RSS feed then change the "enabled" attribute to "false".

    See details: http://revision3.com/blog/2010/03/11/pick-your-favorite-shows-create-a-personalized-feed/
    Once you sign up and create your personal RSS feed replace the url in the example below with the
    Revision3 "Your RSS Feed Address" URL and change the "enabled" element attribute to "true".
-->
<treeviewURLS globalmax="0">
    <url enabled="false">http://revision3.com/feed/user/EXAMPLE</url>
</treeviewURLS>
</userRev3>
''')

        # Get the current show links from the Rev3 web site
        linksTree = self.common.getUrlData(self.rev3_config.find('treeviewUrls'))

        if self.config['debug_enabled']:
            print "create(%s)" % create
            print "linksTree:"
            sys.stdout.write(etree.tostring(linksTree, encoding='UTF-8', pretty_print=True))
            print

        # Extract the show name and Web page links
        showData = etree.XML(u'<xml></xml>')
        complexFilter = u"//div[@class='subscribe_rss']//div//p[.='MP4']/..//a"
        for result in linksTree.xpath('//results'):
            tmpDirectory = etree.XML(u'<directory></directory>')
            dirName = result.find('name').text
            tmpDirectory.attrib['name'] = dirName

            if self.config['debug_enabled']:
                print "Results: #Items(%s) for (%s)" % (len(result.xpath('.//a')), dirName)
                print

            for anchor in result.xpath('.//a'):
                showURL = None
                if dirName == 'Shows':
                    showURL = anchor.attrib.get('href')
                    showFilter = complexFilter
                    tmpName = anchor.text
                elif dirName == 'Revision3 Beta':
                    tmpName = etree.tostring(anchor, method="text", encoding=unicode)
                    showURL = u'http://revision3beta.com%sfeed/' % anchor.attrib.get('href')
                    showFilter = None
                elif dirName == 'Archived Shows':
                    showURL = u'http://revision3.com%s' % anchor.attrib.get('href')
                    showFilter = complexFilter
                    tmpName = anchor.text
                if tmpName == u'Revision3 Beta':
                    continue
                if showURL != None:
                    url = etree.SubElement(tmpDirectory, "url")
                    etree.SubElement(url, "name").text = tmpName
                    etree.SubElement(url, "href").text = showURL
                    etree.SubElement(url, "filter").text = showFilter
                    etree.SubElement(url, "parserType").text = u'html'
            if tmpDirectory.find('url') != None:
                showData.append(tmpDirectory)

        if self.config['debug_enabled']:
            print "showData:"
            sys.stdout.write(etree.tostring(showData, encoding='UTF-8', pretty_print=True))
            print

        # Assemble the feeds and formats
        for directory in showData.findall('directory'):
            if create:
                firstEnabled = True
            else:
                firstEnabled = False
            tmpDirectory = etree.XML(u'<directory></directory>')
            tmpDirectory.attrib['name'] = directory.attrib['name']
            if directory.attrib['name'] == u'Revision3 Beta':
                for show in directory.findall('url'):
                    tmpShow = etree.XML(u'<show></show>')
                    tmpShow.attrib['name'] = show.find('name').text
                    mp4Format = etree.SubElement(tmpShow, u"mp4Format")
                    if firstEnabled:
                        mp4Format.attrib['enabled'] = u'true'
                        firstEnabled = False
                    else:
                        mp4Format.attrib['enabled'] = u'false'
                    mp4Format.attrib['name'] = u'Web Only'
                    mp4Format.attrib['rss'] = show.find('href').text
                    tmpDirectory.append(tmpShow)
            else:
                showResults = self.common.getUrlData(directory)
                for show in showResults.xpath('//results'):
                    tmpShow = etree.XML(u'<show></show>')
                    tmpShow.attrib['name'] = show.find('name').text

                    if self.config['debug_enabled']:
                        print "Results: #Items(%s) for (%s)" % (len(show.xpath('.//a')), tmpShow.attrib['name'])
                        print

                    for format in show.xpath('.//a'):
                        link = u'http://revision3.com%s' % format.attrib['href']
                        # If this is a "tekzilla" link without extra parameters that skip show
                        # This forces the Tekzilla weekly show to be separate from the daily show
                        if link.find('/tekzilla/') != -1 and link.find('?subshow=false') == -1:
                            continue
                        mp4Format = etree.SubElement(tmpShow, "mp4Format")
                        if firstEnabled:
                            mp4Format.attrib['enabled'] = u'true'
                            firstEnabled = False
                        else:
                            mp4Format.attrib['enabled'] = u'false'
                        mp4Format.attrib['name'] = format.text
                        mp4Format.attrib['rss'] = link
                    if tmpShow.find('mp4Format') != None:
                        tmpDirectory.append(tmpShow)

            # If there is any data then add to new rev3.xml element tree
            if tmpDirectory.find('show') != None:
                userRev3.append(tmpDirectory)

        if self.config['debug_enabled']:
            print "Before any merging userRev3:"
            sys.stdout.write(etree.tostring(userRev3, encoding='UTF-8', pretty_print=True))
            print

        # If there was an existing rev3.xml file then add any relevant user settings to this new rev3.xml
        if not create:
            userRev3.find('updateDuration').text = self.userPrefs.find('updateDuration').text
            for mp4Format in self.userPrefs.xpath("//mp4Format[@enabled='true']"):
                showName = mp4Format.getparent().attrib['name']
                mp4name = mp4Format.attrib['name']
                elements = userRev3.xpath("//show[@name=$showName]/mp4Format[@name=$mp4name]", showName=showName, mp4name=mp4name)
                if len(elements):
                    elements[0].attrib['enabled'] = u'true'

        if self.config['debug_enabled']:
            print "After any merging userRev3:"
            sys.stdout.write(etree.tostring(userRev3, encoding='UTF-8', pretty_print=True))
            print

        # Save the rev3.xml file
        prefDir = self.rev3_config.find('userPreferenceFile').text.replace(u'/rev3.xml', u'')
        if not os.path.isdir(prefDir):
            os.makedirs(prefDir)
        fd = open(self.rev3_config.find('userPreferenceFile').text, 'w')
        fd.write(u'<userRev3>\n'+u''.join(etree.tostring(element, encoding='UTF-8', pretty_print=True) for element in userRev3)+u'</userRev3>')
        fd.close()

        # Read the refreshed user config file
        try:
            self.userPrefs = etree.parse(self.rev3_config.find('userPreferenceFile').text)
        except Exception, e:
            raise Rev3UrlError(self.error_messages['Rev3UrlError'] % (url, errormsg))
        return
    # end updateRev3()

    def getSeasonEpisode(self, title, link=None):
        ''' Check is there is any season or episode number information in an item's title
        return array of season and/or episode numbers
        return array with None values
        '''
        s_e = [None, None]
        for index in range(len(self.s_e_Patterns)):
            match = self.s_e_Patterns[index].match(title)
            if not match:
                if link:
                    match = self.s_e_Patterns[index].match(link)
                    if not match:
                        continue
                else:
                    continue
            if index < 2:
                s_e[0], s_e[1] = match.groups()
                break
            else:
                s_e[1] = u'%s' % int(match.groups()[0])
                break
        return s_e
    # end getSeasonEpisode()

    def getVideoID(self, link):
        ''' Read the Web page to find the video ID number used for fullscreen autoplay
        return the video ID number
        return None if the number cannot be found
        '''
        videoID = None
        try:
            eTree = etree.parse(link, self.FullScreenParser)
        except Exception, errormsg:
            sys.stderr.write(u"! Error: The URL (%s) cause the exception error (%s)\n" % (link, errormsg))
            return videoID

        if self.config['debug_enabled']:
            print "Raw unfiltered URL imput:"
            sys.stdout.write(etree.tostring(eTree, encoding='UTF-8', pretty_print=True))
            print

        if not eTree:
            return videoID

        # Filter out the video id
        try:
           tmpVideoID = self.FullScreenVidIDxPath(eTree)
        except AssertionError, e:
            sys.stderr.write("No filter results for VideoID from url(%s)\n" % link)
            sys.stderr.write("! Error:(%s)\n" % e)
            return videoID

        if len(tmpVideoID):
            if tmpVideoID[0].get('id'):
                videoID = tmpVideoID[0].attrib['id'].strip().replace(u'player-', u'')

        return videoID

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################


    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the Rev3 web site
        return an array of matching item elements
        return
        '''
        try:
            searchVar = u'?q=%s' % (urllib.quote(title.encode("utf-8")).replace(u' ', u'+'))
        except UnicodeDecodeError:
            searchVar = u'?q=%s' % (urllib.quote(title).replace(u' ', u'+'))
        url = self.rev3_config.find('searchURLS').xpath(".//href")[0].text+searchVar

        if self.config['debug_enabled']:
            print url
            print

        self.rev3_config.find('searchURLS').xpath(".//href")[0].text = url

        # Perform a search
        try:
            resultTree = self.common.getUrlData(self.rev3_config.find('searchURLS'), pageFilter=self.rev3_config.find('searchURLS').xpath(".//pageFilter")[0].text)
        except Exception, errormsg:
            raise Rev3UrlDownloadError(self.error_messages['Rev3UrlDownloadError'] % (errormsg))

        if resultTree is None:
            raise Rev3VideoNotFound(u"No Rev3 Video matches found for search value (%s)" % title)

        searchResults = resultTree.xpath('//result//li[@class="video"]')
        if not len(searchResults):
            raise Rev3VideoNotFound(u"No Rev3 Video matches found for search value (%s)" % title)

        # Set the number of search results returned
        self.channel['channel_numresults'] = len(searchResults)

        # Rev3 search results fo not have a pubDate so use the current data time
        # e.g. "Sun, 06 Jan 2008 21:44:36 GMT"
        pubDate = datetime.datetime.now().strftime(self.common.pubDateFormat)

        # Translate the search results into MNV RSS item format
        thumbnailFilter = etree.XPath('.//a[@class="thumbnail"]/img/@src')
        titleFilter = etree.XPath('.//a[@class="title"]')
        descFilter = etree.XPath('.//div[@class="description"]')
        itemThumbNail = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
        itemDwnLink = etree.XPath('.//media:content', namespaces=self.common.namespaces)
        itemDict = {}
        for result in searchResults:
            if len(titleFilter(result)):   # Make sure that this result actually has a video
                rev3Item = etree.XML(self.common.mnvItem)
                # Extract and massage data
                thumbNail = self.common.ampReplace(thumbnailFilter(result)[0])
                title = self.common.massageText(titleFilter(result)[0].text.strip())
                tmpDesc = etree.tostring(descFilter(result)[0], method="text", encoding=unicode).strip()
                index = tmpDesc.find(u'¬ñ')
                if index != -1:
                    tmpDesc = tmpDesc[index+1:].strip()
                description = self.common.massageText(tmpDesc)
                link = self.common.ampReplace(titleFilter(result)[0].attrib['href'])
                author = u'Revision3'
                # Insert data into a new item element
                rev3Item.find('title').text = title
                rev3Item.find('author').text = author
                rev3Item.find('pubDate').text = pubDate
                rev3Item.find('description').text = description
                rev3Item.find('link').text = link
                itemThumbNail(rev3Item)[0].attrib['url'] = thumbNail
                itemDwnLink(rev3Item)[0].attrib['url'] = link
                s_e = self.getSeasonEpisode(title, None)
                if s_e[0]:
                    etree.SubElement(rev3Item, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = s_e[0]
                if s_e[1]:
                    etree.SubElement(rev3Item, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = s_e[1]
                itemDict[title.lower()] = rev3Item

        if not len(itemDict.keys()):
            raise Rev3VideoNotFound(u"No Rev3 Video matches found for search value (%s)" % title)

        return [itemDict, resultTree.xpath('//pageInfo')[0].text]
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Get rev3_config.xml
        self.getRev3Config()

        if self.config['debug_enabled']:
            print "self.rev3_config:"
            sys.stdout.write(etree.tostring(self.rev3_config, encoding='UTF-8', pretty_print=True))
            print

        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        try:
            data = self.searchTitle(title, pagenumber, self.page_limit)
        except Rev3VideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            sys.exit(0)
        except Rev3UrlError, msg:
            sys.stderr.write(u'%s\n' % msg)
            sys.exit(1)
        except Rev3HttpError, msg:
            sys.stderr.write(self.error_messages['Rev3HttpError'] % msg)
            sys.exit(1)
        except Rev3RssError, msg:
            sys.stderr.write(self.error_messages['Rev3RssError'] % msg)
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
        '''Gather the Revision3 feeds then get a max page of videos meta data in each of them
        Display the results and exit
        '''
        personalFeed = u"Personal Feed" # A label used to identify processing of a personal RSS feed

        # Get the user preferences that specify which shows and formats they want to be in the treeview
        try:
            self.getUserPreferences()
        except Exception, e:
            sys.stderr.write(u'%s' % e)
            sys.exit(1)

        if self.config['debug_enabled']:
            print "self.userPrefs:"
            sys.stdout.write(etree.tostring(self.userPrefs, encoding='UTF-8', pretty_print=True))
            print

        # Verify that there is at least one RSS feed that user wants to download
        rssFeeds = self.userPrefs.xpath("//mp4Format[@enabled='true']")
        personalFeeds = self.userPrefs.xpath("//treeviewURLS//url[@enabled='true']")
        if not len(rssFeeds) and not len(personalFeeds):
            sys.stderr.write(u'There are no mp4Format or personal RSS feed elements "enabled" in your "rev3.xml" user preferences\nfile (%s)\n' % self.rev3_config.find('userPreferenceFile').text)
            sys.exit(1)

        # Create a structure of feeds that can be concurrently downloaded
        showData = etree.XML(u'<xml></xml>')
        for feed in personalFeeds:
            rssFeeds.append(feed)
        count = 0
        for rssFeed in rssFeeds:
            if rssFeed.getparent().tag == 'treeviewURLS':
                uniqueName = u'%s:%s' % (personalFeed, count)
                count+=1
            else:
                uniqueName = u'%s:%s:%s' % (rssFeed.getparent().getparent().attrib['name'], rssFeed.getparent().attrib['name'], rssFeed.attrib['name'])
            url = etree.XML(u'<url></url>')
            etree.SubElement(url, "name").text = uniqueName
            if uniqueName.startswith(personalFeed):
                etree.SubElement(url, "href").text = rssFeed.text
            else:
                etree.SubElement(url, "href").text = rssFeed.attrib['rss']
            etree.SubElement(url, "filter").text = u"//channel"
            etree.SubElement(url, "parserType").text = u'xml'
            showData.append(url)

        if self.config['debug_enabled']:
            print "showData:"
            sys.stdout.write(etree.tostring(showData, encoding='UTF-8', pretty_print=True))
            print

        # Get the RSS Feed data
        try:
            resultTree = self.common.getUrlData(showData)
        except Exception, errormsg:
            raise Rev3UrlDownloadError(self.error_messages['Rev3UrlDownloadError'] % (errormsg))

        if resultTree is None:
            sys.exit(0)

        if self.config['debug_enabled']:
            print "resultTree:"
            sys.stdout.write(etree.tostring(resultTree, encoding='UTF-8', pretty_print=True))
            print

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+u'</rss>')

        # Add the Channel element tree
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)

        # Process each directory of the user preferences that have an enabled rss feed
        itemFilter = etree.XPath('.//item')
        channelFilter = etree.XPath('./result/channel')
        imageFilter = etree.XPath('.//image/url')
        itemDwnLink = './/media:content'
        itemThumbNail = './/media:thumbnail'
        itemDuration = './/media:content'
        itemLanguage = './/media:content'
        categoryDir = None
        showDir = None
        categoryElement = etree.XML(u'<directory></directory>')
        itemAuthor = u'Revision3'
        for result in resultTree.findall('results'):
            names = result.find('name').text.split(':')
            for index in range(len(names)):
                names[index] = self.common.massageText(names[index])
            channel = channelFilter(result)[0]
            if channel.find('image') != None:
                channelThumbnail = self.common.ampReplace(imageFilter(channel)[0].text)
            else:
                channelThumbnail = self.common.ampReplace(channel.find('link').text.replace(u'/watch/', u'/images/')+u'100.jpg')
            channelLanguage = u'en'
            if channel.find('language') != None:
                channelLanguage = channel.find('language').text[:2]
            # Create a new directory and/or subdirectory if required
            if names[0] != categoryDir:
                if categoryDir != None:
                    channelTree.append(categoryElement)
                categoryElement = etree.XML(u'<directory></directory>')
                if names[0] == personalFeed:
                    categoryElement.attrib['name'] = channel.find('title').text
                else:
                    categoryElement.attrib['name'] = names[0]
                categoryElement.attrib['thumbnail'] = self.channel_icon
                categoryDir = names[0]
            if names[1] != showDir:
                if names[0] == personalFeed:
                    showElement = categoryElement
                else:
                    showElement = etree.SubElement(categoryElement, u"directory")
                    if names[2] == 'Web Only':
                        showElement.attrib['name'] = u'%s' % (names[1])
                    else:
                        showElement.attrib['name'] = u'%s: %s' % (names[1], names[2])
                    showElement.attrib['thumbnail'] = channelThumbnail
                showDir = names[1]

            if self.config['debug_enabled']:
                print "Results: #Items(%s) for (%s)" % (len(itemFilter(result)), names)
                print

            # Convert each RSS item into a MNV item
            for itemData in itemFilter(result):
                rev3Item = etree.XML(self.common.mnvItem)
                # Extract and massage data also insert data into a new item element
                rev3Item.find('title').text = self.common.massageText(itemData.find('title').text.strip())
                rev3Item.find('author').text = itemAuthor
                rev3Item.find('pubDate').text = self.common.massageText(itemData.find('pubDate').text)
                rev3Item.find('description').text = self.common.massageText(itemData.find('description').text.strip())
                link = self.common.ampReplace(itemData.find('link').text)
                downLoadLink = self.common.ampReplace(itemData.xpath(itemDwnLink, namespaces=self.common.namespaces)[0].attrib['url'])

                # If this is one of the main shows or from the personal RSS feed
                # then get a full screen video id
                if names[0] == 'Shows' or names[0] == personalFeed:
                    fullScreenVideoID = self.getVideoID(itemData.find('link').text)
                    if fullScreenVideoID:
                        if link == downLoadLink:
                            downLoadLink = self.common.ampReplace(self.FullScreen % fullScreenVideoID)
                        link = self.common.ampReplace(self.FullScreen % fullScreenVideoID)

                rev3Item.find('link').text = self.common.ampReplace(link)
                rev3Item.xpath(itemDwnLink, namespaces=self.common.namespaces)[0].attrib['url'] = downLoadLink
                try:
                    rev3Item.xpath(itemThumbNail, namespaces=self.common.namespaces)[0].attrib['url'] = self.common.ampReplace(itemData.xpath(itemThumbNail, namespaces=self.common.namespaces)[0].attrib['url'].replace(u'--mini', u'--medium'))
                except IndexError:
                    pass
                try:
                    rev3Item.xpath(itemDuration, namespaces=self.common.namespaces)[0].attrib['duration'] = itemData.xpath(itemDuration, namespaces=self.common.namespaces)[0].attrib['duration']
                except KeyError:
                    pass
                rev3Item.xpath(itemLanguage, namespaces=self.common.namespaces)[0].attrib['lang'] = channelLanguage
                if rev3Item.xpath(itemThumbNail, namespaces=self.common.namespaces)[0].get('url'):
                    s_e = self.getSeasonEpisode(rev3Item.find('title').text, rev3Item.xpath(itemThumbNail, namespaces=self.common.namespaces)[0].attrib['url'])
                else:
                    s_e = self.getSeasonEpisode(rev3Item.find('title').text, rev3Item.xpath(itemDwnLink, namespaces=self.common.namespaces)[0].attrib['url'])
                if s_e[0]:
                    etree.SubElement(rev3Item, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = s_e[0]
                if s_e[1]:
                    etree.SubElement(rev3Item, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = s_e[1]
                if s_e[0] and s_e[1]:
                    rev3Item.find('title').text = u'S%02dE%02d: %s' % (int(s_e[0]), int (s_e[1]), rev3Item.find('title').text)
                elif s_e[0]:
                    rev3Item.find('title').text = u'S%02d: %s' % (int(s_e[0]), rev3Item.find('title').text)
                elif s_e[1]:
                    rev3Item.find('title').text = u'Ep%03d: %s' % (int(s_e[1]), rev3Item.find('title').text)
                showElement.append(rev3Item)

        # Add the last directory processed
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
