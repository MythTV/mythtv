#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: thewb_api - Simple-to-use Python interface to the The WB RSS feeds (http://www.thewb.com/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and
#           access text metadata, video and image URLs from The WB.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="thewb_api - Simple-to-use Python interface to the The WB RSS feeds (http://www.thewb.com/)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from thewb. These routines process RSS feeds provided by The WB
(http://www.thewb.com/). The specific "The WB" RSS feeds that are processed are controled through
a user XML preference file usually found at "~/.mythtv/MythNetvision/userGrabberPrefs/thewb.xml"
'''

__version__="v0.1.0"
# 0.1.0 Initial development

import os, struct, sys, re, time, datetime, urllib
import logging
from socket import gethostname, gethostbyname
from threading import Thread
from copy import deepcopy

from thewb_exceptions import (TheWBUrlError, TheWBHttpError, TheWBRssError, TheWBVideoNotFound, TheWBConfigFileError, TheWBUrlDownloadError)

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


def can_int(x):
    """Takes a string, checks if it is numeric.
    >>> _can_int("2")
    True
    >>> _can_int("A test")
    False
    """
    if x == None:
        return False
    try:
        int(x)
    except ValueError:
        return False
    else:
        return True
# end _can_int


class Videos(object):
    """Main interface to http://www.thewb.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search methods
    The apikey is a not required to access http://www.thewb.com/
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
            pass    # TheWB does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = u'TheWB_Grabber'
        self.common.logger = self.common.initLogger(path=u'/tmp', log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'TheWBUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'TheWBHttpError': u"! Error: An HTTP communications error with The WB was raised (%s)\n", 'TheWBRssError': u"! Error: Invalid RSS meta data\nwas received from The WB error (%s). Skipping item.\n", 'TheWBVideoNotFound': u"! Error: Video search with The WB did not return any results (%s)\n", 'TheWBConfigFileError': u"! Error: thewb_config.xml file missing\nit should be located in and named as (%s).\n", 'TheWBUrlDownloadError': u"! Error: Downloading a RSS feed or Web page (%s).\n", }

        # Channel details and search results
        self.channel = {'channel_title': u'The WB', 'channel_link': u'http://www.thewb.com/', 'channel_description': u"Watch full episodes of your favorite shows on The WB.com, like Friends, The O.C., Veronica Mars, Pushing Daisies, Smallville, Buffy The Vampire Slayer, One Tree Hill and Gilmore Girls.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        self.channel_icon = u'http://upload.wikimedia.org/wikipedia/en/5/50/The_WB_Online_Logo.png'

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions

        if mythdb:
            self.icon_dir = mythdb.settings[gethostname()]['mythnetvision.iconDir']
            if self.icon_dir:
                self.icon_dir = self.icon_dir.replace(u'//', u'/')
                self.setTreeViewIcon(dir_icon='thewb')
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


    def getTheWBConfig(self):
        ''' Read the MNV The WB grabber "thewb_config.xml" configuration file
        return nothing
        '''
        # Read the grabber thewb_config.xml configuration file
        url = u'file://%s/nv_python_libs/configs/XML/thewb_config.xml' % (baseProcessingDir, )
        if not os.path.isfile(url[7:]):
            raise TheWBConfigFileError(self.error_messages['TheWBConfigFileError'] % (url[7:], ))

        if self.config['debug_enabled']:
            print url
            print
        try:
            self.thewb_config = etree.parse(url)
        except Exception, e:
            raise TheWBUrlError(self.error_messages['TheWBUrlError'] % (url, errormsg))
        return
    # end getTheWBConfig()


    def getUserPreferences(self):
        '''Read the thewb_config.xml and user preference thewb.xml file.
        If the thewb.xml file does not exist then create it.
        If the thewb.xml file is too old then update it.
        return nothing
        '''
        # Get thewb_config.xml
        self.getTheWBConfig()

        # Check if the thewb.xml file exists
        userPreferenceFile = self.thewb_config.find('userPreferenceFile').text
        if userPreferenceFile[0] == '~':
             self.thewb_config.find('userPreferenceFile').text = u"%s%s" % (os.path.expanduser(u"~"), userPreferenceFile[1:])
        if os.path.isfile(self.thewb_config.find('userPreferenceFile').text):
            # Read the grabber thewb_config.xml configuration file
            url = u'file://%s' % (self.thewb_config.find('userPreferenceFile').text, )
            if self.config['debug_enabled']:
                print url
                print
            try:
                self.userPrefs = etree.parse(url)
            except Exception, e:
                raise TheWBUrlError(self.error_messages['TheWBUrlError'] % (url, errormsg))
            # Check if the thewb.xml file is too old
            nextUpdateSecs = int(self.userPrefs.find('updateDuration').text)*86400 # seconds in a day
            nextUpdate = time.localtime(os.path.getmtime(self.thewb_config.find('userPreferenceFile').text)+nextUpdateSecs)
            now = time.localtime()
            if nextUpdate > now:
                return
            create = False
        else:
            create = True

        # If required create/update the thewb.xml file
        self.updateTheWB(create)
        return
    # end getUserPreferences()

    def updateTheWB(self, create=False):
        ''' Create or update the thewb.xml user preferences file
        return nothing
        '''
        # Read the default user preferences file
        url = u'file://%s/nv_python_libs/configs/XML/defaultUserPrefs/thewb.xml' % (baseProcessingDir, )
        if not os.path.isfile(url[7:]):
            raise TheWBConfigFileError(self.error_messages['TheWBConfigFileError'] % (url[7:], ))

        if self.config['debug_enabled']:
            print 'updateTheWB url(%s)' % url
            print
        try:
            userTheWB = etree.parse(url)
        except Exception, e:
            raise TheWBUrlError(self.error_messages['TheWBUrlError'] % (url, errormsg))

        # Get the current show links from the TheWB web site
        linksTree = self.common.getUrlData(self.thewb_config.find('treeviewUrls'))

        if self.config['debug_enabled']:
            print "create(%s)" % create
            print "linksTree:"
            sys.stdout.write(etree.tostring(linksTree, encoding='UTF-8', pretty_print=True))
            print

        # Check that at least several show directories were returned
        if not create:
            if not len(linksTree.xpath('//results//a')) > 10:
                return self.userPrefs

        # Assemble the feeds and formats
        root = etree.XML(u'<xml></xml>')
        for directory in linksTree.xpath('//results'):
            tmpDirectory = etree.SubElement(root, u'showDirectories')
            tmpDirectory.attrib['name'] = directory.find('name').text
            for show in directory.xpath('.//a'):
                showName = show.attrib['title']
                # Skip any DVD references as they are not on-line videos
                if showName.lower().find('dvd') != -1 or show.attrib['href'].lower().find('dvd') != -1:
                    continue
                tmpShow = etree.XML(u'<url></url>')
                tmpShow.attrib['enabled'] = u'true'
                tmpShow.attrib['name'] = self.common.massageText(showName.strip())
                tmpShow.text = self.common.ampReplace(show.attrib['href'].replace(u'/shows/', u'').replace(u'/', u'').strip())
                tmpDirectory.append(tmpShow)

        if self.config['debug_enabled']:
            print "Before any merging userTheWB:"
            sys.stdout.write(etree.tostring(userTheWB, encoding='UTF-8', pretty_print=True))
            print

        # If there was an existing thewb.xml file then add any relevant user settings to
        # this new thewb.xml
        if not create:
            userTheWB.find('updateDuration').text = self.userPrefs.find('updateDuration').text
            if self.userPrefs.find('treeviewURLS').get('globalmax'):
                userTheWB.find('treeviewURLS').attrib['globalmax'] = self.userPrefs.find('treeviewURLS').attrib['globalmax']
            if self.userPrefs.find('showDirectories').get('globalmax'):
                root.find('showDirectories').attrib['globalmax'] = self.userPrefs.find('showDirectories').attrib['globalmax']
            for rss in self.userPrefs.xpath("//url[@enabled='false']"):
                elements = root.xpath("//url[text()=$URL]", URL=rss.text.strip())
                if len(elements):
                    elements[0].attrib['enabled'] = u'false'
                    if rss.get('max'):
                        elements[0].attrib['max'] = rss.attrib['max']

        if self.config['debug_enabled']:
            print "After any merging userTheWB:"
            sys.stdout.write(etree.tostring(userTheWB, encoding='UTF-8', pretty_print=True))
            print

        # Save the thewb.xml file
        prefDir = self.thewb_config.find('userPreferenceFile').text.replace(u'/thewb.xml', u'')
        if not os.path.isdir(prefDir):
            os.makedirs(prefDir)
        fd = open(self.thewb_config.find('userPreferenceFile').text, 'w')
        fd.write(etree.tostring(userTheWB, encoding='UTF-8', pretty_print=True)[:-len(u'</userTheWB>')-1]+u''.join(etree.tostring(element, encoding='UTF-8', pretty_print=True) for element in root.xpath('/xml/*'))+u'</userTheWB>')
        fd.close()

        # Input the refreshed user preference data
        try:
            self.userPrefs = etree.parse(self.thewb_config.find('userPreferenceFile').text)
        except Exception, e:
            raise TheWBUrlError(self.error_messages['TheWBUrlError'] % (url, errormsg))
        return
    # end updateTheWB()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################


    def searchTitle(self, title, pagenumber, pagelen, ignoreError=False):
        '''Key word video search of the TheWB web site
        return an array of matching item elements
        return
        '''
        orgURL = self.thewb_config.find('searchURLS').xpath(".//href")[0].text

        try:
            searchVar = u'%s/%s' % (urllib.quote(title.encode("utf-8")).replace(u' ', u'+'), pagenumber)
        except UnicodeDecodeError:
            searchVar = u'%s/%s' % (urllib.quote(title).replace(u' ', u'+'), pagenumber)
        url = self.thewb_config.find('searchURLS').xpath(".//href")[0].text+searchVar

        if self.config['debug_enabled']:
            print "Search url(%s)" % url
            print

        self.thewb_config.find('searchURLS').xpath(".//href")[0].text = url

        # Perform a search
        try:
            resultTree = self.common.getUrlData(self.thewb_config.find('searchURLS'), pageFilter=self.thewb_config.find('searchURLS').xpath(".//pageFilter")[0].text)
        except Exception, errormsg:
            self.thewb_config.find('searchURLS').xpath(".//href")[0].text = orgURL
            raise TheWBUrlDownloadError(self.error_messages['TheWBUrlDownloadError'] % (errormsg))

        self.thewb_config.find('searchURLS').xpath(".//href")[0].text = orgURL

        if self.config['debug_enabled']:
            print "resultTree count(%s)" % len(resultTree)
            print etree.tostring(resultTree, encoding='UTF-8', pretty_print=True)
            print

        if resultTree is None:
            if ignoreError:
                return [None, None]
            raise TheWBVideoNotFound(u"No TheWB.com Video matches found for search value (%s)" % title)

        searchResults = resultTree.xpath('//result/div')
        if not len(searchResults):
            if ignoreError:
                return [None, None]
            raise TheWBVideoNotFound(u"No TheWB.com Video matches found for search value (%s)" % title)

        # Set the number of search results returned
        self.channel['channel_numresults'] = len(searchResults)

        # TheWB search results fo not have a pubDate so use the current data time
        # e.g. "Sun, 06 Jan 2008 21:44:36 GMT"
        pubDate = datetime.datetime.now().strftime(self.common.pubDateFormat)

        # Translate the search results into MNV RSS item format
        thumbNailFilter = etree.XPath('.//div[@class="episodethumb"]/img')
        descFilter = etree.XPath('.//div[@class="episodecontent"]/p')
        linkFilter = etree.XPath('.//div[@class="episodecontent"]/p//a[@href!="javascript:;"]')
        itemThumbNail = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
        itemDwnLink = etree.XPath('.//media:content', namespaces=self.common.namespaces)
        itemDict = {}
        for result in searchResults:
            if len(linkFilter(result)):   # Make sure that this result actually has a video
                thewbItem = etree.XML(self.common.mnvItem)
                # These videos are only viewable in the US so add a country indicator
                etree.SubElement(thewbItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = u'us'
                # Extract and massage data
                thumbNail = self.common.ampReplace(thumbNailFilter(result)[0].attrib['src'])
                title = linkFilter(result)[0].text
                link = self.common.ampReplace(u'http://www.thewb.com'+linkFilter(result)[0].attrib['href'])
                descriptionElement = descFilter(result)[0]
                description = u''
                tmptitle = None
                seasonNum = None
                episodeNum = None
                for e in descriptionElement.xpath('./*'):
                    try:
                        eText = unicode(e.tail, 'UTF-8').strip()
                    except:
                        continue
                    if not eText.startswith(u'Season ') and not eText.startswith(u'EP '):
                        description=eText
                        continue
                    else:
                        infoList = eText.split(u',')
                        if not len(infoList):
                            continue
                        s_e = infoList[0].replace(u'Season',u'').replace(u'EP',u'').strip().split(u' ')
                        if len(s_e) == 1 and can_int(s_e[0].strip()):
                            infoList[0] = u'Ep(%02d)' % int(s_e[0].strip())
                            episodeNum = s_e[0].strip()
                        elif len(s_e) == 3 and can_int(s_e[0].strip()) and can_int(s_e[2].strip()):
                            infoList[0] = u'S%02dE%02d' % (int(s_e[0].strip()), int(s_e[2].strip()))
                            seasonNum = s_e[0].strip()
                            episodeNum = s_e[2].strip()
                        title = title.replace(u'-', u'–')
                        index = title.find(u'–')
                        if index != -1:
                            tmptitle = u'%s: %s %s' % (title[:index].strip(), infoList[0].strip(), title[index:].strip())
                        else:
                            tmptitle = u'%s %s' % (title, infoList[0].strip())
                        if not len(infoList) > 1:
                            continue
                        videoSeconds = False
                        videoDuration = infoList[1].strip().replace(u'(', u'').replace(u')', u'').split(u':')
                        try:
                            if len(videoDuration) == 1:
                                videoSeconds = int(videoDuration[0])
                            elif len(videoDuration) == 2:
                                videoSeconds = int(videoDuration[0])*60+int(videoDuration[1])
                            elif len(videoDuration) == 3:
                                videoSeconds = int(videoDuration[0])*3600+int(videoDuration[1])*60+int(videoDuration[2])
                            if videoSeconds:
                                itemDwnLink(thewbItem)[0].attrib['duration'] = unicode(videoSeconds)
                        except:
                            pass
                if tmptitle:
                    title = tmptitle
                title = self.common.massageText(title.strip())
                description = self.common.massageText(description.strip())
                # Insert data into a new item element
                thewbItem.find('title').text = title
                thewbItem.find('author').text = "The WB.com"
                thewbItem.find('pubDate').text = pubDate
                thewbItem.find('description').text = description
                thewbItem.find('link').text = link
                itemThumbNail(thewbItem)[0].attrib['url'] = thumbNail
                itemDwnLink(thewbItem)[0].attrib['url'] = link
                if seasonNum:
                    etree.SubElement(thewbItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = seasonNum
                if episodeNum:
                    etree.SubElement(thewbItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = episodeNum
                itemDict[title.lower()] = thewbItem

        if not len(itemDict.keys()):
            if ignoreError:
                return [None, None]
            raise TheWBVideoNotFound(u"No TheWB Video matches found for search value (%s)" % title)

        return [itemDict, resultTree.xpath('//pageInfo')[0].text]
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Get thewb_config.xml
        self.getTheWBConfig()

        if self.config['debug_enabled']:
            print "self.thewb_config:"
            sys.stdout.write(etree.tostring(self.thewb_config, encoding='UTF-8', pretty_print=True))
            print

        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        try:
            data = self.searchTitle(title, pagenumber, self.page_limit)
        except TheWBVideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            sys.exit(0)
        except TheWBUrlError, msg:
            sys.stderr.write(u'%s\n' % msg)
            sys.exit(1)
        except TheWBHttpError, msg:
            sys.stderr.write(self.error_messages['TheWBHttpError'] % msg)
            sys.exit(1)
        except TheWBRssError, msg:
            sys.stderr.write(self.error_messages['TheWBRssError'] % msg)
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
            self.channel['channel_returned'] = itemCount
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
        '''Gather the The WB feeds then get a max page of videos meta data in each of them
        Display the results and exit
        '''
        # Get the user preferences that specify which shows and formats they want to be in the treeview
        try:
            self.getUserPreferences()
        except Exception, e:
            sys.stderr.write(u'%s\n' % e)
            sys.exit(1)

        # Verify that there is at least one RSS feed that user wants to download
        showFeeds = self.userPrefs.xpath("//showDirectories//url[@enabled='true']")
        totalFeeds = self.userPrefs.xpath("//url[@enabled='true']")

        if self.config['debug_enabled']:
            print "self.userPrefs show count(%s) total feed count(%s):" % (len(showFeeds), len(totalFeeds))
            sys.stdout.write(etree.tostring(self.userPrefs, encoding='UTF-8', pretty_print=True))
            print

        if not len(totalFeeds):
            sys.stderr.write(u'There are no show or treeviewURLS elements "enabled" in your "thewb.xml" user preferences\nfile (%s)\n' % self.thewb_config.find('userPreferenceFile').text)
            sys.exit(1)

        # Massage channel icon
        self.channel_icon = self.common.ampReplace(self.channel_icon)

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+u'</rss>')

        # Add the Channel element tree
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)

        # Process any user specified searches
        showItems = {}
        if len(showFeeds) != None:
            for searchDetails in showFeeds:
                try:
                    data = self.searchTitle(searchDetails.text.strip(), 1, self.page_limit, ignoreError=True)
                    if data[0] == None:
                    	continue
                except TheWBVideoNotFound, msg:
                    sys.stderr.write(u"%s\n" % msg)
                    continue
                except TheWBUrlError, msg:
                    sys.stderr.write(u'%s\n' % msg)
                    continue
                except TheWBHttpError, msg:
                    sys.stderr.write(self.error_messages['TheWBHttpError'] % msg)
                    continue
                except TheWBRssError, msg:
                    sys.stderr.write(self.error_messages['TheWBRssError'] % msg)
                    continue
                except Exception, e:
                    sys.stderr.write(u"! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (searchDetails.text.strip(), e))
                    continue
                data.append(searchDetails.attrib['name'])
                showItems[self.common.massageText(searchDetails.text.strip())] = data
                continue

        if self.config['debug_enabled']:
            print "After searches count(%s):" % len(showItems)
            for key in showItems.keys():
                print "Show(%s) name(%s) item count(%s)" % (key, showItems[key][2], len(showItems[key][0]))
            print

        # Filter out any items that are not specifically for the show
        for showNameKey in showItems.keys():
            tmpList = {}
            for key in showItems[showNameKey][0].keys():
                tmpLink = showItems[showNameKey][0][key].find('link').text.replace(self.thewb_config.find('searchURLS').xpath(".//href")[0].text, u'')
                if tmpLink.startswith(showNameKey):
                    tmpList[key] = showItems[showNameKey][0][key]
            showItems[showNameKey][0] = tmpList

        if self.config['debug_enabled']:
            print "After search filter of non-show items count(%s):" % len(showItems)
            for key in showItems.keys():
                print "Show(%s) name(%s) item count(%s)" % (key, showItems[key][2], len(showItems[key][0]))
            print

        # Create a structure of feeds that can be concurrently downloaded
        rssData = etree.XML(u'<xml></xml>')
        rssFeedsUrl = u'http://www.thewb.com/shows/feed/'
        for feedType in [u'treeviewURLS', u'showDirectories', ]:
            if self.userPrefs.find(feedType) == None:
                continue
            if not len(self.userPrefs.find(feedType).xpath('./url')):
                continue
            for rssFeed in self.userPrefs.xpath("//%s/url[@enabled='true']" % feedType):
                if feedType == u'showDirectories':
                    link = rssFeedsUrl+rssFeed.text
                else:
                    link = rssFeed.text
                urlName = rssFeed.attrib.get('name')
                if urlName:
                     uniqueName = u'%s;%s' % (urlName, link)
                else:
                    uniqueName = u'RSS;%s' % (link)
                url = etree.XML(u'<url></url>')
                etree.SubElement(url, "name").text = uniqueName
                etree.SubElement(url, "href").text = link
                etree.SubElement(url, "filter").text = u"//channel/title"
                etree.SubElement(url, "filter").text = u"//item"
                etree.SubElement(url, "parserType").text = u'xml'
                rssData.append(url)

        if self.config['debug_enabled']:
            print "rssData:"
            sys.stdout.write(etree.tostring(rssData, encoding='UTF-8', pretty_print=True))
            print

        # Get the RSS Feed data
        self.channelLanguage = u'en'
        self.itemAuthor = u'The WB.com'
        self.itemFilter = etree.XPath('.//item', namespaces=self.common.namespaces)
        self.titleFilter = etree.XPath('.//title', namespaces=self.common.namespaces)
        self.linkFilter = etree.XPath('.//link', namespaces=self.common.namespaces)
        self.descFilter1 = etree.XPath('.//description', namespaces=self.common.namespaces)
        self.descFilter2 = etree.XPath("//text()")
        self.pubdateFilter = etree.XPath('.//pubDate', namespaces=self.common.namespaces)
        self.thumbNailFilter = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
        self.itemThumbNail = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
        self.itemDwnLink = etree.XPath('.//media:content', namespaces=self.common.namespaces)
        self.rssName = etree.XPath('title', namespaces=self.common.namespaces)
        self.feedFilter = etree.XPath('//url[text()=$url]')
        self.HTMLparser = etree.HTMLParser()
        if rssData.find('url') != None:
            try:
                resultTree = self.common.getUrlData(rssData)
            except Exception, errormsg:
                raise TheWBUrlDownloadError(self.error_messages['TheWBUrlDownloadError'] % (errormsg))

            if self.config['debug_enabled']:
                print "resultTree:"
                sys.stdout.write(etree.tostring(resultTree, encoding='UTF-8', pretty_print=True))
                print

            # Process each directory of the user preferences that have an enabled rss feed
            for result in resultTree.findall('results'):
                names = result.find('name').text.split(u';')
                names[0] = self.common.massageText(names[0])
                if names[0] == 'RSS':
                    names[0] = self.common.massageText(self.rssName(result.find('result'))[0].text.strip())
                    urlName = names[0]
                else:
                    urlName = result.find('url').text.replace(rssFeedsUrl, u'').strip()

                urlMax = None
                url = self.feedFilter(self.userPrefs, url=names[1])
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
                if self.config['debug_enabled']:
                    print "Results: #Items(%s) for (%s)" % (len(self.itemFilter(result)), names)
                    print
                self.createItems(showItems, result, urlName, names[0], urlMax=urlMax)
                continue

        # Add all the shows and rss items to the channel
        for key in sorted(showItems.keys()):
            if not len(showItems[key][0]):
                continue
            # Create a new directory and/or subdirectory if required
            directoryElement = etree.SubElement(channelTree, u'directory')
            directoryElement.attrib['name'] = showItems[key][2]
            directoryElement.attrib['thumbnail'] = self.channel_icon

            if self.config['debug_enabled']:
                print "Results: #Items(%s) for (%s)" % (len(showItems[key][0]), showItems[key][2])
                print

            # Copy all the items into the MNV RSS directory
            for itemKey in sorted(showItems[key][0].keys()):
                directoryElement.append(showItems[key][0][itemKey])

        if self.config['debug_enabled']:
            print "Final results: #Items(%s)" % len(rssTree.xpath('//item'))
            print

        # Check that there was at least some items
        if len(rssTree.xpath('//item')):
            # Output the MNV search results
            sys.stdout.write(u'<?xml version="1.0" encoding="UTF-8"?>\n')
            sys.stdout.write(etree.tostring(rssTree, encoding='UTF-8', pretty_print=True))

        sys.exit(0)
    # end displayTreeView()

    def createItems(self, showItems, result, urlName, showName, urlMax=None):
        '''Create a dictionary of MNV compliant RSS items from the results of a RSS feed show search.
        Also merge with any items that were found by using the Web search. Identical items use the RSS
        feed item data over the search item as RSS provides better results.
        return nothing as the show item dictionary will have all the results
        '''
        # Initalize show if it has not already had a search result
        if not urlName in showItems.keys():
            showItems[urlName] = [{}, None, showName]

        # Convert each RSS item into a MNV item
        count = 0
        for thewbItem in self.itemFilter(result):
            newItem = etree.XML(self.common.mnvItem)
            # These videos are only viewable in the US so add a country indicator
            etree.SubElement(newItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = u'us'
            # Extract and massage data
            link = self.common.ampReplace(self.linkFilter(thewbItem)[0].text)
            # Convert the pubDate '2010-05-02T11:23:25-07:00' to a MNV pubdate format
            pubdate = self.pubdateFilter(thewbItem)
            if len(pubdate):
                pubdate = pubdate[0].text[:-6]
                pubdate = time.strptime(pubdate, '%Y-%m-%dT%H:%M:%S')
                pubdate = time.strftime(self.common.pubDateFormat, pubdate)
            else:
                pubdate = datetime.datetime.now().strftime(self.common.pubDateFormat)
            title = self.common.massageText(self.titleFilter(thewbItem)[0].text.strip())
            tmptitle = None
            descList = self.descFilter2(etree.parse(StringIO(self.descFilter1(thewbItem)[0].text), self.HTMLparser))
            description = None
            seasonNum = None
            episodeNum = None
            for eText in descList:
                if eText == '\n\t':
                    continue
                eText = eText.strip().encode('UTF-8')
                if description == None:
                    description = eText
                    continue
                if eText.startswith(u'Season: ') or eText.startswith(u'EP: '):
                    s_e = eText.replace(u'Season:',u'').replace(u', Episode:',u'').replace(u'EP:',u'').strip().split(u' ')
                    if len(s_e) == 1 and can_int(s_e[0].strip()):
                        eText = u'Ep(%02d)' % int(s_e[0].strip())
                        episodeNum = s_e[0].strip()
                    elif len(s_e) == 2 and can_int(s_e[0].strip()) and can_int(s_e[1].strip()):
                        eText = u'S%02dE%02d' % (int(s_e[0].strip()), int(s_e[1].strip()))
                        seasonNum = s_e[0].strip()
                        episodeNum = s_e[1].strip()
                    title = title.replace(u'-', u'–')
                    index = title.find(u'–')
                    if index != -1:
                        tmptitle = u'%s: %s %s' % (title[:index].strip(), eText.strip(), title[index:].strip())
                    else:
                        tmptitle = u'%s %s' % (title, eText.strip())
                    continue
                elif eText.startswith(u'Running Time: '):
                    videoDuration = eText.replace(u'Running Time: ', u'').strip().split(u':')
                    if not len(videoDuration):
                        continue
                    videoSeconds = False
                    try:
                        if len(videoDuration) == 1:
                            videoSeconds = int(videoDuration[0])
                        elif len(videoDuration) == 2:
                            videoSeconds = int(videoDuration[0])*60+int(videoDuration[1])
                        elif len(videoDuration) == 3:
                            videoSeconds = int(videoDuration[0])*3600+int(videoDuration[1])*60+int(videoDuration[2])
                        if videoSeconds:
                            self.itemDwnLink(newItem)[0].attrib['duration'] = unicode(videoSeconds)
                    except:
                        pass
            if tmptitle:
                title = tmptitle
            title = self.common.massageText(title.strip())
            description = self.common.massageText(description.strip())
            # Insert data into a new item element
            newItem.find('title').text = title
            newItem.find('author').text = self.itemAuthor
            newItem.find('pubDate').text = pubdate
            newItem.find('description').text = description
            newItem.find('link').text = link
            self.itemDwnLink(newItem)[0].attrib['url'] = link
            try:
                self.itemThumbNail(newItem)[0].attrib['url'] = self.common.ampReplace(self.itemThumbNail(thewbItem)[0].attrib['url'])
            except IndexError:
                pass
            self.itemDwnLink(newItem)[0].attrib['lang'] = self.channelLanguage
            if seasonNum:
                etree.SubElement(newItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = seasonNum
            if episodeNum:
                etree.SubElement(newItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = episodeNum
            # Merge RSS results with search results and override any duplicates with the RSS item
            showItems[urlName][0][title.lower()] = newItem
            if urlMax: # Check of the maximum items to processes has been met
                count+=1
                if count > urlMax:
                    break
        return
    # end createItems()
# end Videos() class
