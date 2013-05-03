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

__version__="v0.2.5"
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

import os, struct, sys, re, time
import urllib, urllib2
import logging
from MythTV import MythXML

try:
    import xml.etree.cElementTree as ElementTree
except ImportError:
    import xml.etree.ElementTree as ElementTree

from youtube_exceptions import (YouTubeUrlError, YouTubeHttpError, YouTubeRssError, YouTubeVideoNotFound, YouTubeInvalidSearchType, YouTubeXmlError, YouTubeVideoDetailError, YouTubeCategoryNotFound)

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


class XmlHandler:
    """Deals with retrieval of XML files from API
    """
    def __init__(self, url):
        self.url = url

    def _grabUrl(self, url):
        try:
            urlhandle = urllib.urlopen(url)
        except IOError, errormsg:
            raise YouTubeHttpError(errormsg)
        return urlhandle.read()

    def getEt(self):
        xml = self._grabUrl(self.url)
        try:
            et = ElementTree.fromstring(xml)
        except SyntaxError, errormsg:
            raise YouTubeXmlError(errormsg)
        return et


class Videos(object):
    """Main interface to http://www.youtube.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search methods
    The apikey is a not required to access http://www.youtube.com/
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
        self.mythxml = MythXML()

        if apikey is not None:
            self.config['apikey'] = apikey
        else:
            pass    # YouTube does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages

        self.log_name = "youtube"
        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'YouTubeUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'YouTubeHttpError': u"! Error: An HTTP communications error with YouTube was raised (%s)\n", 'YouTubeRssError': u"! Error: Invalid RSS meta data\nwas received from YouTube error (%s). Skipping item.\n", 'YouTubeVideoNotFound': u"! Error: Video search with YouTube did not return any results (%s)\n", 'YouTubeVideoDetailError': u"! Error: Invalid Video meta data detail\nwas received from YouTube error (%s). Skipping item.\n", }

        # This is an example that must be customized for each target site
        self.key_translation = [{'channel_title': 'channel_title', 'channel_link': 'channel_link', 'channel_description': 'channel_description', 'channel_numresults': 'channel_numresults', 'channel_returned': 'channel_returned', 'channel_startindex': 'channel_startindex'}, {'title': 'item_title', 'author': 'item_author', 'published_parsed': 'item_pubdate', 'media_description': 'item_description', 'video': 'item_link', 'thumbnail': 'item_thumbnail', 'link': 'item_url', 'duration': 'item_duration', 'rating': 'item_rating', 'item_width': 'item_width', 'item_height': 'item_height', 'language': 'item_lang'}]

        # Defaulting to no language specified. The YouTube apis does support specifying a language
        if language:
            self.config['language'] = language
        else:
            self.config['language'] = u''

        self.config[u'urls'] = {}

        # v2 api calls - An example that must be customized for each target site
        self.config[u'urls'][u'video.search'] = 'http://gdata.youtube.com/feeds/api/videos?vq=%s&max-results=%s&start-index=%s&orderby=relevance&Ir=%s'


        # Functions that parse video data from RSS data
        self.config['item_parser'] = {}
        self.config['item_parser']['main'] = self.getVideosForURL

        # Tree view url and the function that parses that urls meta data
        self.config[u'urls'][u'tree.view'] = {
            'standard_feeds': {
                '__all__': ['http://gdata.youtube.com/feeds/api/standardfeeds/%s?v=2', 'main'],
                },
            'category': {
                '__all__': ['http://gdata.youtube.com/feeds/api/videos?category=%s&v=2', 'main'],
                },
            'local_feeds': {
                '__all__': ['http://gdata.youtube.com/feeds/api/standardfeeds/%s?v=2', 'main'],
                },
            'location_feeds': {
                '__all__': ['http://gdata.youtube.com/feeds/api/videos?v=2&q=%s', 'main'],
                },
            }
        self.config[u'urls'][u'categories_list'] = 'http://gdata.youtube.com/schemas/2007/categories.cat'

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions

        self.tree_order = ['standard_feeds', 'location_feeds', 'local_feeds', 'category']
        self.tree_org = {
            'category': [
                ['', ['Film']],
                ['', ['Sports']],
                ['Information', ['News', 'Tech', 'Education', 'Howto', ]],
                ['Entertainment', ['Comedy', 'Music', 'Games', 'Entertainment', ]],
                ['Other', ['Autos', 'Animals', 'Travel', 'People', 'Nonprofit']] ],
            'standard_feeds':
                [['Feeds', ['top_rated', 'top_favourites', 'most_viewed', 'most_popular', 'most_recent', 'most_discussed', 'most_responded', 'recently_featured', '']], ],
            'local_feeds':
                [['Feeds', ['top_rated', 'top_favourites', 'most_viewed', 'most_popular', 'most_recent', 'most_discussed', 'most_responded', 'recently_featured', '']], ],
            'location_feeds':
                [['', ['location']], ]
            }

        self.tree_customize = {
            'category': {
                '__default__': {'order': 'rating', 'max-results': '20', 'start-index': '1', 'Ir': self.config['language']},
                #'cat name': {'order: '', 'max-results': , 'start-index': , 'restriction: '', 'time': '', 'Ir': ''},
                'Film': {'max-results': '40', 'time': 'this_month',},
                'Music': {'max-results': '40', 'time': 'this_month',},
                'Sports': {'max-results': '40', 'time': 'this_month',},
            },
            'standard_feeds': {
                '__default__': {'order': 'rating', 'max-results': '20', 'start-index': '1', 'Ir': self.config['language'], 'time': 'this_month'},
                #'feed name": {'order: '', 'max-results': , 'start-index': , 'restriction: '', 'time': '', 'Ir': ''}
            },
            'local_feeds': {
                '__default__': {'order': 'rating', 'max-results': '20', 'start-index': '1', 'Ir': self.config['language'], 'location': '', 'location-radius':'500km'},
                #'feed name": {'order: '', 'max-results': , 'start-index': , 'restriction: '', 'time': '', 'Ir': ''}
            },
            'location_feeds': {
                '__default__': {'order': 'rating', 'max-results': '20', 'start-index': '1', 'Ir': self.config['language'], },
                #'feed name": {'order: '', 'max-results': , 'start-index': , 'restriction: '', 'time': '', 'Ir': ''}
            },
            }

        self.feed_names = {
            'standard_feeds': {'top_rated': 'Highest Rated', 'top_favourites': 'Most Subscribed', 'most_viewed': 'Most Viewed', 'most_popular': 'Most Popular', 'most_recent': 'Most Recent', 'most_discussed': 'Most Comments', 'most_responded': 'Most Responses', 'recently_featured': 'Featured'}
            }

        self.feed_icons = {
            'standard_feeds': {'top_rated': 'directories/topics/rated', 'top_favourites': 'directories/topics/most_subscribed', 'most_viewed': 'directories/topics/most_viewed', 'most_popular': None, 'most_recent': 'directories/topics/most_recent', 'most_discussed': 'directories/topics/most_comments', 'most_responded': None, 'recently_featured': 'directories/topics/featured'
                },
            'local_feeds': {'top_rated': 'directories/topics/rated', 'top_favourites': 'directories/topics/most_subscribed', 'most_viewed': 'directories/topics/most_viewed', 'most_popular': None, 'most_recent': 'directories/topics/most_recent', 'most_discussed': 'directories/topics/most_comments', 'most_responded': None, 'recently_featured': 'directories/topics/featured'
                },
            'category': {
                'Film': 'directories/topics/movies',
                'Comedy': 'directories/film_genres/comedy',
                'Sports': 'directories/topics/sports',
                'News': 'directories/topics/news', 'Tech': 'directories/topics/technology', 'Education': 'directories/topics/education', 'Howto': 'directories/topics/howto',
                'Music': 'directories/topics/music', 'Games': 'directories/topics/games', 'Entertainment': 'directories/topics/entertainment',
                'Autos': 'directories/topics/automotive', 'Animals': 'directories/topics/animals', 'Travel': 'directories/topics/travel', 'People': 'directories/topics/people', 'Nonprofit': 'directories/topics/nonprofit',
                },
            }

        self.treeview = False
        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/youtube.png'
    # end __init__()

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
        return self.ampReplace(re.sub(u"(?s)<[^>]*>|&#?\w+;", fixup, self.textUtf8(text))).replace(u'\n',u' ')
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


    def textUtf8(self, text):
        if text == None:
            return text
        try:
            return unicode(text, 'utf8')
        except UnicodeDecodeError:
            return u''
        except (UnicodeEncodeError, TypeError):
            return text
    # end textUtf8()


    def ampReplace(self, text):
        '''Replace all "&" characters with "&amp;"
        '''
        text = self.textUtf8(text)
        return text.replace(u'&amp;',u'~~~~~').replace(u'&',u'&amp;').replace(u'~~~~~', u'&amp;')
    # end ampReplace()

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
            dir_icon = self.feed_icons[self.tree_key][self.feed]
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
        url = self.config[u'urls'][u'video.search'] % (urllib.quote_plus(title.encode("utf-8")), pagelen, pagenumber, self.config['language'], )
        if self.config['debug_enabled']:
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            raise YouTubeUrlError(self.error_messages['YouTubeUrlError'] % (url, errormsg))

        if etree is None:
            raise YouTubeVideoNotFound(u"No YouTube Video matches found for search value (%s)" % title)

        data = []
        for entry in etree:
            if entry.tag.endswith('totalResults'):
                if entry.text:
                    self.channel['channel_numresults'] = int(entry.text)
                else:
                    self.channel['channel_numresults'] = 0
                continue
            if not entry.tag.endswith('entry'):
                continue
            item = {}
            cur_size = True
            flash = False
            for parts in entry:
                if parts.tag.endswith('id'):
                    item['id'] = parts.text
                    continue
                if parts.tag.endswith('title'):
                    item['title'] = parts.text
                    continue
                if parts.tag.endswith('author'):
                    for e in parts:
                        if e.tag.endswith('name'):
                            item['author'] = e.text
                            break
                    continue
                if parts.tag.endswith('published'):
                    item['published_parsed'] = parts.text
                    continue
                if parts.tag.endswith('content'):
                    item['media_description'] = parts.text
                    continue
                if parts.tag.endswith(u'rating'):
                    item['rating'] = parts.get('average')
                    continue
                if not parts.tag.endswith(u'group'):
                    continue
                for elem in parts:
                    if elem.tag.endswith(u'duration'):
                        item['duration'] =  elem.get('seconds')
                        continue
                    if elem.tag.endswith(u'thumbnail'):
                        if cur_size == False:
                            continue
                        height = elem.get('height')
                        width = elem.get('width')
                        if int(width) > cur_size:
                            item['thumbnail'] = self.ampReplace(elem.get('url'))
                            cur_size = int(width)
                        if int(width) >= 200:
                            cur_size = False
                        continue
                    if elem.tag.endswith(u'player'):
                        item['link'] = self.ampReplace(elem.get('url'))
                        continue
                    if elem.tag.endswith(u'content') and flash == False:
                        for key in elem.keys():
                            if not key.endswith(u'format'):
                                continue
                            if not elem.get(key) == '5':
                                continue
                            self.processVideoUrl(item, elem)
                            flash = True
                        continue
            if not item.has_key('video'):
                item['video'] =  item['link']
                item['duration'] =  u''
            else:
                item['link'] =  item['video']
            data.append(item)

        # Make sure there are no item elements that are None
        for item in data:
            for key in item.keys():
                if item[key] == None:
                    item[key] = u''

        # Massage each field and eliminate any item without a URL
        elements_final = []
        for item in data:
            if not 'id' in item.keys():
                continue
            item['language'] = self.config['language']
            for key in item.keys(): # 2010-01-23T08:38:39.000Z
                if key == 'published_parsed':
                    if item[key]:
                        pub_time = time.strptime(item[key].strip(), "%Y-%m-%dT%H:%M:%S.%fZ")
                        item[key] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                    continue
                if key == 'media_description' or key == 'title':
                    # Strip the HTML tags
                    if item[key]:
                        item[key] = self.massageDescription(item[key].strip())
                        item[key] = item[key].replace(u'|', u'-')
                    continue
                if type(item[key]) == type(u''):
                    if item[key]:
                        item[key] = self.ampReplace(item[key].replace('"\n',' ').strip())
            elements_final.append(item)

        if not len(elements_final):
            raise YouTubeVideoNotFound(u"No YouTube Video matches found for search value (%s)" % title)

        return elements_final
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Channel details and search results
        self.channel = {'channel_title': u'YouTube', 'channel_link': u'http://www.youtube.com/', 'channel_description': u"Share your videos with friends, family, and the world.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        startindex = (int(pagenumber) -1) * self.page_limit + 1
        try:
            data = self.searchTitle(title, startindex, self.page_limit)
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

        items = []
        for match in data:
            item_data = {}
            for key in self.key_translation[1].keys():
                if key in match.keys():
                    item_data[self.key_translation[1][key]] = match[key]
                else:
                    item_data[self.key_translation[1][key]] = u''
            items.append(item_data)

        self.channel['channel_startindex'] = self.page_limit * int(pagenumber)
        self.channel['channel_returned'] = len(items)

        if len(items):
            return [[self.channel, items]]
        return None
    # end searchForVideos()

    def getCategories(self, dir_dict, categories):
        '''Parse a dictionary made of subdictionaries and category list and extract all of the categories
        return a list of categories
        '''
        for sets in dir_dict:
            if isinstance(sets[1], str):
                continue
            for cat in sets[1]:
                categories.append(cat)
        return categories
    # end getCategories()

    def displayTreeView(self):
        '''Gather the Youtube categories/feeds/...etc then get a max page of videos meta data in each of them
        return array of directories and their video metadata
        '''
        # Channel details and search results
        self.channel = {'channel_title': u'YouTube', 'channel_link': u'http://www.youtube.com/', 'channel_description': u"Share your videos with friends, family, and the world.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        if self.config['debug_enabled']:
            print self.config[u'urls']
            print

        if self.config['debug_enabled']:
            print self.config[u'urls'][u'categories_list']
            print

        try:
            etree = XmlHandler(self.config[u'urls'][u'categories_list']).getEt()
        except Exception, errormsg:
            raise YouTubeUrlError(self.error_messages['YouTubeUrlError'] % (url, errormsg))

        if etree is None:
            raise YouTubeCategoryNotFound(u"No YouTube Categories found for Tree view")

        cats = []
        for category in etree:
            if category.tag.endswith('category'):
                cats.append({'term': category.get('term'), 'label': category.get('label')})
        if not len(cats):
            raise YouTubeCategoryNotFound(u"No YouTube Category tags found for Tree view")

        self.feed_names['category'] = {}
        for category in cats:
            self.feed_names['category'][category['term']] = self.ampReplace(category['label'])

        # Verify all categories are already in site tree map add any new ones to 'Other'
        categories = []
        categories = self.getCategories(self.tree_org['category'], categories)

        # Add any categories that are not in the preset tree map
        new_category = []
        for category in self.feed_names['category'].keys():
            if category in categories:
                continue
            new_category.append(category)
        if len(new_category):
            self.tree_org['category'].append(['New', new_category])
            self.tree_org['category'].append(['', u''])

        # Add local feed details
        # {'Latitude': '43.6667', 'Country': 'Canada', 'Longitude': '-79.4167', 'City': 'Toronto'}
        longitude_latitude = self.detectUserLocationByIP()
        if len(longitude_latitude):
            self.feed_names['local_feeds'] = dict(self.feed_names['standard_feeds'])
            self.tree_customize['local_feeds']['__default__']['location'] = u"%s,%s" % (longitude_latitude['Latitude'], longitude_latitude['Longitude'])
            self.tree_org['local_feeds'][0][0] = u'Youtube Feeds within %s of %s, %s' % (self.tree_customize['local_feeds']['__default__']['location-radius'], longitude_latitude['City'], longitude_latitude['Country'])
        else:
            self.tree_order.remove('local_feeds')
        # Set location search parameters
        if len(longitude_latitude):
            city_country = u'%s+%s' % (longitude_latitude['City'], longitude_latitude['Country'])
            self.tree_org['location_feeds'][0][1][0] = city_country
            self.feed_names['location_feeds'] = dict({u'%s' % city_country: u'Youtube Videos for %s, %s' % (longitude_latitude['City'], longitude_latitude['Country'])})
        else:
            self.tree_order.remove('location_feeds')

        # Set the default videos per page limit for all feeds/categories/... etc
        for key in self.tree_customize.keys():
            if '__default__' in self.tree_customize[key].keys():
                if 'max-results' in self.tree_customize[key]['__default__'].keys():
                    self.tree_customize[key]['__default__']['max-results'] = unicode(self.page_limit)

        # Get videos within each category
        dictionaries = []

        # Process the various video feeds/categories/... etc
        for key in self.tree_order:
            self.tree_key = key
            dictionaries = self.getVideos(self.tree_org[key], dictionaries)

        return [[self.channel, dictionaries]]
    # end displayTreeView()

    def processVideoUrl(self, item, elem):
        '''Processes elem.get('url') to either use a custom HTML page served by
        the backend, or include '&autoplay=1'
        '''
        m = re.search('/v/([^?]+)', elem.get('url'))
        if m:
            url = self.mythxml.getInternetContentUrl("nv_python_libs/configs/HTML/youtube.html", \
                                                     m.group(1))
            item['video'] = self.ampReplace(url)
        else:
            item['video'] = self.ampReplace((elem.get('url')+'&autoplay=1'))

    def makeURL(self, URL):
        '''Form a URL to search for videos
        return a URL
        '''
        additions = dict(self.tree_customize[self.tree_key]['__default__']) # Set defaults

        # Add customizations
        if self.feed in self.tree_customize[self.tree_key].keys():
            for element in self.tree_customize[self.tree_key][self.feed].keys():
                additions[element] = self.tree_customize[self.tree_key][self.feed][element]

        # Make the search extension string that is added to the URL
        addition = u''
        for ky in additions.keys():
            if ky.startswith('add_'):
                addition+=u'/%s' %  additions[ky]
            else:
                addition+=u'&%s=%s' %  (ky, additions[ky])
        index = URL.find('%')
        if index == -1:
            return (URL+addition)
        else:
            return (URL+addition) % self.feed
    # end makeURL()


    def getVideos(self, dir_dict, dictionaries):
        '''Parse a list made of category lists and retrieve video meta data
        return a dictionary of directory names and categories video metadata
        '''
        for sets in dir_dict:
            if not isinstance(sets[1], list):
                if sets[0] != '': # Add the nested dictionaries display name
                    try:
                        dictionaries.append([self.massageDescription(sets[0]), self.setTreeViewIcon(self.feed_icons[self.tree_key][sets[0]])])
                    except KeyError:
                        dictionaries.append([self.massageDescription(sets[0]), self.channel_icon])
                else:
                    dictionaries.append(['', u'']) # Add the nested dictionary indicator
                continue
            temp_dictionary = []
            for self.feed in sets[1]:
                if self.config[u'urls'][u'tree.view'][self.tree_key].has_key('__all__'):
                    URL = self.config[u'urls'][u'tree.view'][self.tree_key]['__all__']
                else:
                    URL = self.config[u'urls'][u'tree.view'][self.tree_key][self.feed]
                temp_dictionary = self.config['item_parser'][URL[1]](self.makeURL(URL[0]), temp_dictionary)
            if len(temp_dictionary):
                if len(sets[0]): # Add the nested dictionaries display name
                    try:
                        dictionaries.append([self.massageDescription(sets[0]), self.setTreeViewIcon(self.feed_icons[self.tree_key][sets[0]])])
                    except KeyError:
                        dictionaries.append([self.massageDescription(sets[0]), self.channel_icon])
                for element in temp_dictionary:
                    dictionaries.append(element)
                if len(sets[0]):
                    dictionaries.append(['', u'']) # Add the nested dictionary indicator
        return dictionaries
    # end getVideos()

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
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            sys.stderr.write(self.error_messages['YouTubeUrlError'] % (url, errormsg))
            return dictionaries

        if etree is None:
            sys.stderr.write(u'1-No Videos for (%s)\n' % self.feed)
            return dictionaries

        dictionary_first = False
        for elements in etree:
            if elements.tag.endswith(u'totalResults'):
                self.channel['channel_numresults'] += int(elements.text)
                self.channel['channel_startindex'] = self.page_limit
                self.channel['channel_returned'] = self.page_limit # False value CHANGE later
                continue

            if not elements.tag.endswith(u'entry'):
                continue

            metadata = {}
            cur_size = True
            flash = False
            for e in elements:
                metadata['language'] = self.config['language']
                if e.tag.endswith(u'published'): # '2009-02-13T04:54:28.000Z'
                    if e.text:
                        pub_time = time.strptime(e.text.strip(), "%Y-%m-%dT%H:%M:%S.000Z")
                        metadata['published_parsed'] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                    continue
                if e.tag.endswith(u'rating'):
                    if e.get('average'):
                        metadata['rating'] = e.get('average')
                    continue
                if e.tag.endswith(u'title'):
                    if e.text:
                        metadata['title'] = self.massageDescription(e.text.strip())
                    continue
                if e.tag.endswith(u'author'):
                    for a in e:
                        if a.tag.endswith(u'name'):
                            if a.text:
                                metadata['author'] = self.massageDescription(a.text.strip())
                            break
                    continue
                if not e.tag.endswith(u'group'):
                    continue
                for elem in e:
                    if elem.tag.endswith(u'description'):
                        if elem.text != None:
                            metadata['media_description'] = self.massageDescription(elem.text.strip())
                        else:
                            metadata['media_description'] = u''
                        continue
                    if elem.tag.endswith(u'duration'):
                        if elem.get('seconds'):
                            metadata['duration'] =  elem.get('seconds').strip()
                        continue
                    if elem.tag.endswith(u'thumbnail'):
                        if cur_size == False:
                            continue
                        height = elem.get('height')
                        width = elem.get('width')
                        if int(width) > cur_size:
                            if elem.get('url'):
                                metadata['thumbnail'] = self.ampReplace(elem.get('url'))
                                cur_size = int(width)
                        if int(width) >= 200:
                            cur_size = False
                        continue
                    if elem.tag.endswith(u'player'):
                        if elem.get('url'):
                            metadata['link'] = self.ampReplace(elem.get('url'))
                        continue
                    if elem.tag.endswith(u'content') and flash == False:
                        for key in elem.keys():
                            if not key.endswith(u'format'):
                                continue
                            if not elem.get(key) == '5':
                                continue
                            if elem.get('url'):
                                self.processVideoUrl(metadata, elem)
                                flash = True
                        continue

            if not metadata.has_key('video') and not metadata.has_key('link'):
                continue

            if not metadata.has_key('video'):
                metadata['video'] = metadata['link']
            else:
                metadata['link'] =  metadata['video']

            if not dictionary_first:  # Add the dictionaries display name
                dictionaries.append([self.massageDescription(self.feed_names[self.tree_key][self.feed]), self.setTreeViewIcon()])
                dictionary_first = True

            final_item = {}
            for key in self.key_translation[1].keys():
                if not metadata.has_key(key):
                    final_item[self.key_translation[1][key]] = u''
                else:
                    final_item[self.key_translation[1][key]] = metadata[key]
            dictionaries.append(final_item)

        if initial_length < len(dictionaries): # Need to check if there was any items for this Category
            dictionaries.append(['', u'']) # Add the nested dictionary indicator
        return dictionaries
    # end getVideosForURL()
# end Videos() class
