# -*- coding: UTF-8 -*-

# ----------------------
# Name: mnvsearch_api - Simple-to-use Python interface to search the MythNetvision data base tables
#
# Python Script
# Author:   R.D. Vaughan
#   This python script is intended to perform a data base search of MythNetvision data base tables for
#   videos based on a command line search term.
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mnvsearch_api - Simple-to-use Python interface to search the MythNetvision data base tables"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a data base search of MythNetvision data base tables for
videos based on a command line search term.
'''

__version__="0.1.4"
# 0.1.0 Initial development
# 0.1.1 Changed the logger to only output to stderr rather than a file
# 0.1.2 Changed the SQL query to the new "internetcontentarticles" table format and new fields
#       Added "%SHAREDIR%" to icon directory path
# 0.1.3 Video duration value was being erroneously multiplied by 60.
# 0.1.4 Add the ability to search within a specific "feedtitle". Used mainly for searching large mashups
#       Fixed a paging bug

import os, struct, sys, re, time, datetime, shutil, urllib.request, urllib.parse, urllib.error
import logging
from socket import gethostname, gethostbyname
from threading import Thread
from copy import deepcopy
from operator import itemgetter, attrgetter

from .mnvsearch_exceptions import (MNVSQLError, MNVVideoNotFound, )
import io

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
        if isinstance(obj, str):
            obj = obj.encode(self.encoding)
        self.out.buffer.write(obj)

    def __getattr__(self, attr):
        """Delegate everything but write to the stream"""
        return getattr(self.out, attr)

if isinstance(sys.stdout, io.TextIOWrapper):
    sys.stdout = OutStreamEncoder(sys.stdout, 'utf8')
    sys.stderr = OutStreamEncoder(sys.stderr, 'utf8')


# Find out if the MythTV python bindings can be accessed and instances can created
try:
    '''If the MythTV python interface is found, required to access Netvision icon directory settings
    '''
    from MythTV import MythDB, MythLog
    try:
        '''Create an instance of each: MythDB
        '''
        MythLog._setlevel('none') # Some non option -M cannot have any logging on stdout
        mythdb = MythDB()
    except MythError as e:
        sys.stderr.write('\n! Error - %s\n' % e.args[0])
        filename = os.path.expanduser("~")+'/.mythtv/config.xml'
        if not os.path.isfile(filename):
            sys.stderr.write('\n! Error - A correctly configured (%s) file must exist\n' % filename)
        else:
            sys.stderr.write('\n! Error - Check that (%s) is correctly configured\n' % filename)
        sys.exit(1)
    except Exception as e:
        sys.stderr.write("\n! Error - Creating an instance caused an error for one of: MythDB. error(%s)\n" % e)
        sys.exit(1)
except Exception as e:
    sys.stderr.write("\n! Error - MythTV python bindings could not be imported. error(%s)\n" % e)
    sys.exit(1)


try:
    from io import StringIO
    from lxml import etree
except Exception as e:
    sys.stderr.write('\n! Error - Importing the "lxml" and "StringIO" python libraries failed on error(%s)\n' % e)
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
    sys.stderr.write('''
! Error - The installed version of the "lxml" python library "libxml" version is too old.
          At least "libxml" version 2.7.2 must be installed. Your version is (%s).
''' % version)
    sys.exit(1)


class Videos(object):
    """Main interface to the MNV treeview table search
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search methods
    The apikey is a not required for this grabber
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
            pass    # MNV search does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages
        self.common = common
        self.common.debug = debug   # Set the common function debug level

        self.log_name = 'MNVsearch_Grabber'
        self.common.logger = self.common.initLogger(path=sys.stderr, log_name=self.log_name)
        self.logger = self.common.logger # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'MNVSQLError': "! Error: A SQL call cause the exception error (%s)\n", 'MNVVideoNotFound': "! Error: Video search did not return any results (%s)\n", }
        # Channel details and search results
        self.channel = {'channel_title': 'Search all tree views', 'channel_link': 'http://www.mythtv.org/wiki/MythNetvision', 'channel_description': "MythNetvision treeview data base search", 'channel_numresults': 0, 'channel_returned': 1, 'channel_startindex': 0}

        self.channel_icon = '%SHAREDIR%/mythnetvision/icons/mnvsearch.png'
    # end __init__()


    def searchTitle(self, title, pagenumber, pagelen, feedtitle=False):
        '''Key word video search of the MNV treeview tables
        return an array of matching item elements
        return
        '''

        # Usually commented out - Easier for debugging
#        resultList = self.getTreeviewData(title, pagenumber, pagelen)
#        print resultList
#        sys.exit(1)

        # Perform a search
        try:
            resultList = self.getTreeviewData(title, pagenumber, pagelen, feedtitle=feedtitle)
        except Exception as errormsg:
            raise MNVSQLError(self.error_messages['MNVSQLError'] % (errormsg))

        if self.config['debug_enabled']:
            print("resultList: count(%s)" % len(resultList))
            print(resultList)
            print()

        if not len(resultList):
            raise MNVVideoNotFound("No treeview Video matches found for search value (%s)" % title)

        # Check to see if there are more items available to display
        morePages = False
        if len(resultList) > pagelen:
            morePages = True
            resultList.pop() # Remove the extra item as it was only used detect if there are more pages

        # Translate the data base search results into MNV RSS item format
        itemDict = {}
        itemThumbnail = etree.XPath('.//media:thumbnail', namespaces=self.common.namespaces)
        itemContent = etree.XPath('.//media:content', namespaces=self.common.namespaces)
        for result in resultList:
            if not result['url']:
                continue
            mnvsearchItem = etree.XML(self.common.mnvItem)
            # Insert data into a new item element
            mnvsearchItem.find('link').text = result['url']
            if result['title']:
                mnvsearchItem.find('title').text = result['title']
            if result['subtitle']:
                etree.SubElement(mnvsearchItem, "subtitle").text = result['subtitle']
            if result['description']:
                mnvsearchItem.find('description').text = result['description']
            if result['author']:
                mnvsearchItem.find('author').text = result['author']
            if result['date']:
                mnvsearchItem.find('pubDate').text = result['date'].strftime(self.common.pubDateFormat)
            if result['rating'] != '32576' and result['rating']:
                mnvsearchItem.find('rating').text = result['rating']
            if result['thumbnail']:
                itemThumbnail(mnvsearchItem)[0].attrib['url'] = result['thumbnail']
            if result['mediaURL']:
                itemContent(mnvsearchItem)[0].attrib['url'] = result['mediaURL']
            if result['filesize']:
                itemContent(mnvsearchItem)[0].attrib['length'] = str(result['filesize'])
            if result['time']:
                itemContent(mnvsearchItem)[0].attrib['duration'] = str(result['time'])
            if result['width']:
                itemContent(mnvsearchItem)[0].attrib['width'] = str(result['width'])
            if result['height']:
                itemContent(mnvsearchItem)[0].attrib['height'] = str(result['height'])
            if result['language']:
                itemContent(mnvsearchItem)[0].attrib['lang'] = result['language']
            if not result['season'] == 0 and not result['episode'] == 0:
                if result['season']:
                    etree.SubElement(mnvsearchItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}season").text = str(result['season'])
                if result['episode']:
                    etree.SubElement(mnvsearchItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}episode").text = str(result['episode'])
            if result['customhtml'] == 1:
                etree.SubElement(mnvsearchItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}customhtml").text = 'true'
            if result['countries']:
                countries = result['countries'].split(' ')
                for country in countries:
                    etree.SubElement(mnvsearchItem, "{http://www.mythtv.org/wiki/MythNetvision_Grabber_Script_Format}country").text = country
            itemDict[result['title'].lower()] = mnvsearchItem

        if not len(list(itemDict.keys())):
            raise MNVVideoNotFound("No MNV Video matches found for search value (%s)" % title)

        # Set the number of search results returned
        if morePages:
            self.channel['channel_numresults'] = pagelen
        else:
            self.channel['channel_numresults'] = len(itemDict)

        return [itemDict, morePages]
        # end searchTitle()


    def searchForVideos(self, title, pagenumber, feedtitle=False):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Usually commented out - Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        try:
            data = self.searchTitle(title, pagenumber, self.page_limit, feedtitle=feedtitle)
        except MNVVideoNotFound as msg:
            if feedtitle:
                return [{}, '0', '0', '0']
            sys.stderr.write("%s\n" % msg)
            sys.exit(0)
        except MNVSQLError as msg:
            sys.stderr.write('%s\n' % msg)
            sys.exit(1)
        except Exception as e:
            sys.stderr.write("! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        if self.config['debug_enabled']:
            print("data: count(%s)" % len(data[0]))
            print(data)
            print()

        # Create RSS element tree
        rssTree = etree.XML(self.common.mnvRSS+'</rss>')

        # Set the paging values
        itemCount = len(list(data[0].keys()))
        if data[1] == True:
            self.channel['channel_returned'] = itemCount
            self.channel['channel_startindex'] = itemCount+(self.page_limit*(int(pagenumber)-1))
            self.channel['channel_numresults'] = itemCount+(self.page_limit*(int(pagenumber)-1)+1)
        else:
            self.channel['channel_returned'] = itemCount+(self.page_limit*(int(pagenumber)-1))
            self.channel['channel_startindex'] = self.channel['channel_returned']
            self.channel['channel_numresults'] = self.channel['channel_returned']

        # If this was a Mashup search request then just return the elements dictionary a paging info
        if feedtitle:
            return [data[0], self.channel['channel_returned'], self.channel['channel_startindex'], self.channel['channel_numresults']]

        # Add the Channel element tree
        channelTree = self.common.mnvChannelElement(self.channel)
        rssTree.append(channelTree)

        lastKey = None
        for key in sorted(data[0].keys()):
            if lastKey != key:
                channelTree.append(data[0][key])
                lastKey = key

        # Output the MNV search results
        sys.stdout.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        sys.stdout.write(etree.tostring(rssTree, encoding='UTF-8', pretty_print=True))
        sys.exit(0)
    # end searchForVideos()

    def getTreeviewData(self, searchTerms, pagenumber, pagelen, feedtitle=False):
        ''' Use a SQL call to get any matching data base entries from the "netvisiontreeitems" and
        "netvisionrssitems" tables. The search term can contain multiple search words separated
        by a ";" character.
        return a list of items found in the search or an empty dictionary if none were found
        '''
        if feedtitle:
            sqlStatement = "(SELECT title, description, subtitle, season, episode, url, type, thumbnail, mediaURL, author, date, rating, filesize, player, playerargs, download, downloadargs, time, width, height, language, customhtml, countries FROM `internetcontentarticles` WHERE `feedtitle` LIKE '%%%%FEEDTITLE%%%%' AND (%s)) ORDER BY title ASC LIMIT %s , %s"
        else:
            sqlStatement = '(SELECT title, description, subtitle, season, episode, url, type, thumbnail, mediaURL, author, date, rating, filesize, player, playerargs, download, downloadargs, time, width, height, language, customhtml, countries FROM `internetcontentarticles` WHERE %s) ORDER BY title ASC LIMIT %s , %s'
        searchTerm = "`title` LIKE '%%SEARCHTERM%%' OR `description` LIKE '%%SEARCHTERM%%'"

        # Create the query variables search terms and the from/to paging values
        searchList = searchTerms.split(';')
        if not len(searchList):
            return {}

        dbSearchStatements = ''
        for aSearch in searchList:
            tmpTerms = searchTerm.replace('SEARCHTERM', aSearch)
            if not len(dbSearchStatements):
                dbSearchStatements+=tmpTerms
            else:
                dbSearchStatements+=' OR ' + tmpTerms

        if pagenumber == 1:
            fromResults = 0
            pageLimit = pagelen+1
        else:
            fromResults = (int(pagenumber)-1)*int(pagelen)
            pageLimit = pagelen+1

        if feedtitle:
            sqlStatement = sqlStatement.replace('FEEDTITLE', feedtitle)

        query = sqlStatement % (dbSearchStatements, fromResults, pageLimit,)
        if self.config['debug_enabled']:
            print("FromRow(%s) pageLimit(%s)" % (fromResults, pageLimit))
            print("query:")
            sys.stdout.write(query)
            print()

        # Make the data base call and parse the returned data to extract the matching video item data
        items = []
        c = mythdb.cursor()
        host = gethostname()
        c.execute(query)
        for title, description, subtitle, season, episode, url, media_type, thumbnail, mediaURL, author, date, rating, filesize, player, playerargs, download, downloadargs, time, width, height, language, customhtml, countries in c.fetchall():
            items.append({'title': title, 'description': description, 'subtitle': subtitle, 'season': season, 'episode': episode, 'url': url, 'media_type': media_type, 'thumbnail': thumbnail, 'mediaURL': mediaURL, 'author': author, 'date': date, 'rating': rating, 'filesize': filesize, 'player': player, 'playerargs': playerargs, 'download': download, 'downloadargs': downloadargs, 'time': time, 'width': width, 'height': height, 'language': language, 'customhtml': customhtml, 'countries': countries})
        c.close()

        return items
    # end getTreeviewData()
# end Videos() class
