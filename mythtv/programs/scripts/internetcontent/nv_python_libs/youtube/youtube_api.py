#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: youtube_api - Simple-to-use Python interface to the youtube API (http://www.youtube.com/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           metadata, video and image URLs from youtube. These routines are based on the api. Specifications
#           for this api are published at http://developer.youtubenservices.com/docs
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="youtube_api - Simple-to-use Python interface to the youtube API (http://developer.youtubenservices.com/docs)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from youtube. These routines are based on the api. Specifications
for this api are published at http://developer.youtubenservices.com/docs
'''

__version__="v0.3.0"
# 0.1.0 Initial development
# 0.1.1 Added Tree view display option
# 0.1.2 Modified Tree view internals to be consistent in approach and structure.
# 0.1.3 Added images for directories
# 0.1.4 Documentation review
# 0.2.0 Public release
# 0.2.1 New python bindings conversion
#       Better exception error reporting
#       Better handling of invalid unicode data from source
# 0.2.2 Completed exception error reporting improvements
#       Removed the use of the feedparser library
# 0.2.3 Fixed an exception message output code error in two places
# 0.2.4 Removed the need for python MythTV bindings and added "%SHAREDIR%" to icon directory path
# 0.2.5 Fixed the Foreign Film icon file name
# 0.3.0 Adapted to the v3 API

import os, struct, sys, re, time, shutil
import urllib, urllib2
import json
import logging
from MythTV import MythXML
from ..common import common_api

from youtube_exceptions import (YouTubeUrlError, YouTubeHttpError, YouTubeRssError, YouTubeVideoNotFound, YouTubeInvalidSearchType, YouTubeXmlError, YouTubeVideoDetailError, YouTubeCategoryNotFound)
from youtube_data import getData

try:
    import aniso8601
except:
    sys.stderr.write("The module aniso8601 could not be imported, duration "
                     "parsing will be disabled\n")
    pass

class JsonHandler:
    """Deals with retrieval of JSON data from API
    """
    def __init__(self, url):
        self.url = url

    def getJson(self):
        try:
            urlhandle = urllib.urlopen(self.url)
            return json.load(urlhandle)
        except IOError, errormsg:
            raise YouTubeHttpError(errormsg)


class Videos(object):
    """Main interface to http://www.youtube.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search methods
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
        self.common = common_api.Common()
        self.mythxml = MythXML()

        self.config['debug_enabled'] = debug # show debugging messages

        self.log_name = "youtube"
        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = \
                {'YouTubeUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n",
                'YouTubeHttpError': u"! Error: An HTTP communications error with YouTube was raised (%s)\n",
                'YouTubeRssError': u"! Error: Invalid RSS meta data\nwas received from YouTube error (%s). Skipping item.\n",
                'YouTubeVideoNotFound': u"! Error: Video search with YouTube did not return any results (%s)\n",
                'YouTubeVideoDetailError': u"! Error: Invalid Video meta data detail\nwas received from YouTube error (%s). Skipping item.\n", }

        # This is an example that must be customized for each target site
        self.key_translation = \
                [{'channel_title': 'channel_title',
                    'channel_link': 'channel_link',
                    'channel_description': 'channel_description',
                    'channel_numresults': 'channel_numresults',
                    'channel_returned': 'channel_returned',
                    'channel_startindex': 'channel_startindex'},
                 {'title': 'item_title',
                    'author': 'item_author',
                    'published_parsed': 'item_pubdate',
                    'media_description': 'item_description',
                    'video': 'item_link',
                    'thumbnail': 'item_thumbnail',
                    'link': 'item_url',
                    'duration': 'item_duration',
                    'rating': 'item_rating',
                    'item_width': 'item_width',
                    'item_height': 'item_height',
                    'language': 'item_lang'}]

        # Defaulting to no language specified. The YouTube apis does support specifying a language
        if language:
            self.config['language'] = language
        else:
            self.config['language'] = u''

        self.getUserPreferences("~/.mythtv/MythNetvision/userGrabberPrefs/youtube.xml")

        # Read region code from user preferences, used by tree view
        region = self.userPrefs.find("region")
        if region != None and region.text:
            self.config['region'] = region.text
        else:
            self.config['region'] = u'us'

        self.apikey = getData().update(getData().a)

        apikey = self.userPrefs.find("apikey")
        if apikey != None and apikey.text:
            self.apikey = apikey.text

        self.feed_icons = {
                'Film & Animation': 'directories/topics/movies',
                'Movies': 'directories/topics/movies',
                'Trailers': 'directories/topics/movies',
                'Sports': 'directories/topics/sports',
                'News & Politics': 'directories/topics/news',
                'Science & Technology': 'directories/topics/technology',
                'Education': 'directories/topics/education',
                'Howto & Style': 'directories/topics/howto',
                'Music': 'directories/topics/music',
                'Gaming': 'directories/topics/games',
                'Entertainment': 'directories/topics/entertainment',
                'Autos & Vehicles': 'directories/topics/automotive',
                'Pets & Animals': 'directories/topics/animals',
                'Travel & Events': 'directories/topics/travel',
                'People & Blogs': 'directories/topics/people',
            }

        self.treeview = False
        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/youtube.png'
    # end __init__()

    def getUserPreferences(self, userPreferenceFilePath):
        userPreferenceFilePath = os.path.expanduser(userPreferenceFilePath)

        # If the user config file does not exists then copy one the default
        if not os.path.isfile(userPreferenceFilePath):
            # Make the necessary directories if they do not already exist
            prefDir = os.path.dirname(userPreferenceFilePath)
            if not os.path.isdir(prefDir):
                os.makedirs(prefDir)

            fileName = os.path.basename(userPreferenceFilePath)
            defaultConfig = u'%s/nv_python_libs/configs/XML/defaultUserPrefs/%s' \
                    % (baseProcessingDir, fileName)
            shutil.copy2(defaultConfig, userPreferenceFilePath)

        # Read the grabber hulu_config.xml configuration file
        url = u'file://%s' % userPreferenceFilePath
        if self.config['debug_enabled']:
            print(url)
            print
        try:
            self.userPrefs = self.common.etree.parse(url)
        except Exception as e:
            raise Exception(url, e)

###########################################################################################################
#
# Start - Utility functions
#
###########################################################################################################

    def detectUserLocationByIP(self):
        '''Get longitude and latitiude to find videos relative to your location. Up to three different
        servers will be tried before giving up.
        return a dictionary e.g.
        {'Latitude': '43.6667', 'Country': 'Canada', 'Longitude': '-79.4167', 'City': 'Toronto'}
        return an empty dictionary if there were any errors
        Code found at: http://blog.suinova.com/2009/04/from-ip-to-geolocation-country-city.html
        '''
        def getExternalIP():
            '''Find the external IP address of this computer.
            '''
            url = urllib.URLopener()
            try:
                resp = url.open('http://www.whatismyip.com/automation/n09230945.asp')
                return resp.read()
            except:
                return None
            # end getExternalIP()

        ip = getExternalIP()

        if ip == None:
            return {}

        try:
            gs = urllib.urlopen('http://blogama.org/ip_query.php?ip=%s&output=xml' % ip)
            txt = gs.read()
        except:
            try:
                gs = urllib.urlopen('http://www.seomoz.org/ip2location/look.php?ip=%s' % ip)
                txt = gs.read()
            except:
                try:
                    gs = urllib.urlopen('http://api.hostip.info/?ip=%s' % ip)
                    txt = gs.read()
                except:
                    logging.error('GeoIP servers not available')
                    return {}
        try:
            if txt.find('<Response>') > 0:
                countrys = re.findall(r'<CountryName>([\w ]+)<',txt)[0]
                citys = re.findall(r'<City>([\w ]+)<',txt)[0]
                lats,lons = re.findall(r'<Latitude>([\d\-\.]+)</Latitude>\s*<Longitude>([\d\-\.]+)<',txt)[0]
            elif txt.find('GLatLng') > 0:
                citys,countrys = re.findall('<br />\s*([^<]+)<br />\s*([^<]+)<',txt)[0]
                lats,lons = re.findall('LatLng\(([-\d\.]+),([-\d\.]+)',txt)[0]
            elif txt.find('<gml:coordinates>') > 0:
                citys = re.findall('<Hostip>\s*<gml:name>(\w+)</gml:name>',txt)[0]
                countrys = re.findall('<countryName>([\w ,\.]+)</countryName>',txt)[0]
                lats,lons = re.findall('gml:coordinates>([-\d\.]+),([-\d\.]+)<',txt)[0]
            else:
                logging.error('error parsing IP result %s'%txt)
                return {}
            return {'Country':countrys,'City':citys,'Latitude':lats,'Longitude':lons}
        except:
            logging.error('Error parsing IP result %s'%txt)
            return {}
    # end detectUserLocationByIP()


    def massageDescription(self, text):
        '''Removes HTML markup from a text string.
        @param text The HTML source.
        @return The plain text.  If the HTML source contains non-ASCII
        entities or character references, this is a Unicode string.
        '''
        def fixup(m):
            text = m.group(0)
            if text[:1] == "<":
                return "" # ignore tags
            if text[:2] == "&#":
                try:
                    if text[:3] == "&#x":
                        return unichr(int(text[3:-1], 16))
                    else:
                        return unichr(int(text[2:-1]))
                except ValueError:
                    pass
            elif text[:1] == "&":
                import htmlentitydefs
                entity = htmlentitydefs.entitydefs.get(text[1:-1])
                if entity:
                    if entity[:2] == "&#":
                        try:
                            return unichr(int(entity[2:-1]))
                        except ValueError:
                            pass
                    else:
                        return unicode(entity, "iso-8859-1")
            return text # leave as is
        return self.common.ampReplace(re.sub(u"(?s)<[^>]*>|&#?\w+;", fixup, self.common.textUtf8(text)))
    # end massageDescription()

    def _initLogger(self):
        """Setups a logger using the logging module, returns a log object
        """
        logger = logging.getLogger(self.log_name)
        formatter = logging.Formatter('%(asctime)s) %(levelname)s %(message)s')

        hdlr = logging.StreamHandler(sys.stdout)

        hdlr.setFormatter(formatter)
        logger.addHandler(hdlr)

        if self.config['debug_enabled']:
            logger.setLevel(logging.DEBUG)
        else:
            logger.setLevel(logging.WARNING)
        return logger
    #end initLogger

    def setTreeViewIcon(self, dir_icon=None):
        '''Check if there is a specific generic tree view icon. If not default to the channel icon.
        return self.tree_dir_icon
        '''
        self.tree_dir_icon = self.channel_icon
        if not dir_icon:
            if not self.feed_icons.has_key(self.tree_key):
                return self.tree_dir_icon
            dir_icon = self.feed_icons[self.tree_key]
            if not dir_icon:
                return self.tree_dir_icon
        self.tree_dir_icon = u'%%SHAREDIR%%/mythnetvision/icons/%s.png' % (dir_icon, )
        return self.tree_dir_icon
    # end setTreeViewIcon()

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################


    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the YouTube web site
        return an array of matching item dictionaries
        return
        '''
        # Special case where the grabber has been executed without any page
        # argument
        if 1 == pagenumber:
            pagenumber = ""

        result = self.getSearchResults(title, pagenumber, pagelen)
        if not result:
            raise YouTubeVideoNotFound(u"No YouTube Video matches found for search value (%s)" % title)

        self.channel['channel_numresults'] = int(result['pageInfo']['totalResults'])
        if 'nextPageToken' in result:
            self.channel['nextpagetoken'] = result['nextPageToken']
        if 'prevPageToken' in result:
            self.channel['prevpagetoken'] = result['prevPageToken']

        ids = map(lambda entry: entry['id']['videoId'], result['items'])

        result = self.getVideoDetails(ids)
        data = map(lambda entry: self.parseDetails(entry), result['items'])

        if not len(data):
            raise YouTubeVideoNotFound(u"No YouTube Video matches found for search value (%s)" % title)

        return data
        # end searchTitle()

    def getSearchResults(self, title, pagenumber, pagelen):
        url = ('https://www.googleapis.com/youtube/v3/search?part=snippet&' + \
                'type=video&q=%s&maxResults=%s&order=relevance&' + \
                'videoEmbeddable=true&key=%s&pageToken=%s') % \
                (urllib.quote_plus(title.encode("utf-8")), pagelen, self.apikey,
                        pagenumber)
        if self.config['debug_enabled']:
            print url
            print

        try:
            return JsonHandler(url).getJson()
        except Exception, errormsg:
            raise YouTubeUrlError(self.error_messages['YouTubeUrlError'] % (url, errormsg))

    def getVideoDetails(self, ids):
        url = 'https://www.googleapis.com/youtube/v3/videos?part=id,snippet,' + \
                'contentDetails&key=%s&id=%s' % (self.apikey, ",".join(ids))
        try:
            return JsonHandler(url).getJson()
        except Exception as errormsg:
            raise YouTubeUrlError(self.error_messages['YouTubeUrlError'] % (url, errormsg))

    def parseDetails(self, entry):
        item = {}
        try:
            item['id'] = entry['id']
            item['video'] = \
                self.mythxml.getInternetContentUrl("nv_python_libs/configs/HTML/youtube.html", \
                                                   item['id'])
            item['link'] = item['video']
            snippet = entry['snippet']
            item['title'] = snippet['title']
            item['media_description'] = snippet['description']
            item['thumbnail'] = snippet['thumbnails']['high']['url']
            item['author'] = snippet['channelTitle']
            item['published_parsed'] = snippet['publishedAt']

            try:
                duration = aniso8601.parse_duration(entry['contentDetails']['duration'])
                item['duration'] = duration.days * 24 * 3600 + duration.seconds
            except Exception:
                pass

            for key in item.keys():
                # Make sure there are no item elements that are None
                if item[key] == None:
                    item[key] = u''
                elif key == 'published_parsed': # 2010-01-23T08:38:39.000Z
                    if item[key]:
                        pub_time = time.strptime(item[key].strip(), "%Y-%m-%dT%H:%M:%S.%fZ")
                        item[key] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                elif key == 'media_description' or key == 'title':
                    # Strip the HTML tags
                    if item[key]:
                        item[key] = self.massageDescription(item[key].strip())
                        item[key] = item[key].replace(u'|', u'-')
                elif type(item[key]) == type(u''):
                    if item[key]:
                        item[key] = self.common.ampReplace(item[key].replace('"\n',' ').strip())
        except KeyError:
            pass

        return item

    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Channel details and search results
        self.channel = {
            'channel_title': u'YouTube',
            'channel_link': u'http://www.youtube.com/',
            'channel_description': u"Share your videos with friends, family, and the world.",
            'channel_numresults': 0,
            'channel_returned': 1,
            'channel_startindex': 0}

        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        try:
            data = self.searchTitle(title, pagenumber, self.page_limit)
        except YouTubeVideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            return None
        except YouTubeUrlError, msg:
            sys.stderr.write(u'%s\n' % msg)
            sys.exit(1)
        except YouTubeHttpError, msg:
            sys.stderr.write(self.error_messages['YouTubeHttpError'] % msg)
            sys.exit(1)
        except YouTubeRssError, msg:
            sys.stderr.write(self.error_messages['YouTubeRssError'] % msg)
            sys.exit(1)
        except Exception, e:
            sys.stderr.write(u"! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        if data == None:
            return None
        if not len(data):
            return None

        items = map(lambda match: self.translateItem(match), data)
        self.channel['channel_returned'] = len(items)

        if len(items):
            return [[self.channel, items]]
        return None
    # end searchForVideos()

    def translateItem(self, item):
        item_data = {}
        for key in self.key_translation[1].keys():
            if key in item.keys():
                item_data[self.key_translation[1][key]] = item[key]
            else:
                item_data[self.key_translation[1][key]] = u''
        return item_data

    def displayTreeView(self):
        '''Gather the Youtube categories/feeds/...etc then get a max page of videos meta data in each of them
        return array of directories and their video metadata
        '''
        # Channel details and search results
        self.channel = {
            'channel_title': u'YouTube',
            'channel_link': u'http://www.youtube.com/',
            'channel_description': u"Share your videos with friends, family, and the world.",
            'channel_numresults': 0,
            'channel_returned': 1,
            'channel_startindex': 0}

        etree = self.getVideoCategories()
        if etree is None:
            raise YouTubeCategoryNotFound(u"No YouTube Categories found for Tree view")

        feed_names = {}
        for category in etree['items']:
            snippet = category['snippet']
            feed_names[snippet['title']] = self.common.ampReplace(category['id'])

        # Get videos within each category
        dictionaries = []

        # Process the various video feeds/categories/... etc
        for category in feed_names:
            self.tree_key = category
            dictionaries = self.getVideosForCategory(feed_names[category], dictionaries)

        return [[self.channel, dictionaries]]
    # end displayTreeView()

    def getVideoCategories(self):
        try:
            url = 'https://www.googleapis.com/youtube/v3/videoCategories?' + \
                    'part=snippet&regionCode=%s&key=%s' % \
                    (self.config['region'], self.apikey)
            return JsonHandler(url).getJson()
        except Exception as errormsg:
            raise YouTubeUrlError(self.error_messages['YouTubeUrlError'] % (url, errormsg))

    def getVideosForCategory(self, categoryId, dictionaries):
        '''Parse a list made of category lists and retrieve video meta data
        return a dictionary of directory names and categories video metadata
        '''
        url = 'https://www.googleapis.com/youtube/v3/videos?part=snippet&' + \
                'chart=mostPopular&videoCategoryId=%s&maxResults=%s&key=%s' %  \
                (categoryId, self.page_limit, self.apikey)
        temp_dictionary = []
        temp_dictionary = self.getVideosForURL(url, temp_dictionary)
        for element in temp_dictionary:
            dictionaries.append(element)
        return dictionaries
    # end getVideosForCategory()

    def getVideosForURL(self, url, dictionaries):
        '''Get the video metadata for url search
        return the video dictionary of directories and their video mata data
        '''
        initial_length = len(dictionaries)

        if self.config['debug_enabled']:
            print "Category URL:"
            print url
            print

        try:
            result = JsonHandler(url).getJson()
        except Exception, errormsg:
            sys.stderr.write(self.error_messages['YouTubeUrlError'] % (url, errormsg))
            return dictionaries

        if result is None:
            sys.stderr.write(u'1-No Videos for (%s)\n' % self.feed)
            return dictionaries

        if 'pageInfo' not in result or 'items' not in result:
            return dictionaries

        dictionary_first = False
        self.channel['channel_numresults'] += int(result['pageInfo']['totalResults'])
        self.channel['channel_startindex'] = self.page_limit
        self.channel['channel_returned'] = len(result['items'])
        for entry in result['items']:
            item = self.parseDetails(entry)

            if not dictionary_first:  # Add the dictionaries display name
                dictionaries.append([self.massageDescription(self.tree_key),
                    self.setTreeViewIcon()])
                dictionary_first = True

            dictionaries.append(self.translateItem(item))

        if initial_length < len(dictionaries): # Need to check if there was any items for this Category
            dictionaries.append(['', u'']) # Add the nested dictionary indicator
        return dictionaries
    # end getVideosForURL()
# end Videos() class
