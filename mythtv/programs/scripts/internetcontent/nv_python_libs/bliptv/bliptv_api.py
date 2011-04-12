#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: bliptv_api - Simple-to-use Python interface to the bliptv API (http://blip.tv/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           meta data, video and image URLs from blip.tv.
#           These routines are based on the v2.0 api. Specifications
#           for this api are published at http://blip.tv/about/api/
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="bliptv_api - Simple-to-use Python interface to the bliptv API (http://blip.tv/about/api/)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from blip.tv. These routines are based on the v2.0 api. Specifications
for this api are published at  http://blip.tv/about/api/
'''

__version__="v0.2.5"
# 0.1.0 Initial development
# 0.1.1 Changed to use bliptv's rss data rather than JSON as JSON ad a number of error
# 0.1.2 Changes Search to parse XML and added Tree view
# 0.1.3 Added directory image logic
# 0.1.4 Documentation updates
# 0.2.0 Public release
# 0.2.1 New python bindings conversion
#       Better exception error reporting
#       Better handling of invalid unicode data from source
# 0.2.2 Completed exception message improvements
#       Removed the unused import of the feedparser library
# 0.2.3 Fixed an exception message output code error in two places
# 0.2.4 Removed the need for python MythTV bindings and added "%SHAREDIR%" to icon directory path
# 0.2.5 Changed link URL to full screen when a "blip:embedUrl" exists for an item
#       Removed a subdirectory level as the "Featured" RSS feed has been discontinued

import os, struct, sys, re, time
import urllib, urllib2
import logging

try:
    import xml.etree.cElementTree as ElementTree
except ImportError:
    import xml.etree.ElementTree as ElementTree

from bliptv_exceptions import (BliptvUrlError, BliptvHttpError, BliptvRssError, BliptvVideoNotFound, BliptvXmlError)

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
            raise BliptvHttpError(errormsg)
        return urlhandle.read()

    def getEt(self):
        xml = self._grabUrl(self.url)
        try:
            et = ElementTree.fromstring(xml)
        except SyntaxError, errormsg:
            raise BliptvXmlError(errormsg)
        return et


class Videos(object):
    """Main interface to http://blip.tv/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search and tree view methods
    The apikey is a not required to access http://blip.tv/
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
            pass    # blip.tv does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages

        self.log_name = "Bliptv"
        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        # Defaulting to ENGISH but the blip.tv apis do not support specifying a language
        self.config['language'] = "en"

        self.error_messages = {'BliptvUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'BliptvHttpError': u"! Error: An HTTP communicating error with blip.tv was raised (%s)\n", 'BliptvRssError': u"! Error: Invalid RSS meta data\nwas received from blip.tv error (%s). Skipping item.\n", 'BliptvVideoNotFound': u"! Error: Video search with blip.tv did not return any results (%s)\n", }

        # This is an example that must be customized for each target site
        self.key_translation = [{'channel_title': 'channel_title', 'channel_link': 'channel_link', 'channel_description': 'channel_description', 'channel_numresults': 'channel_numresults', 'channel_returned': 'channel_returned', 'channel_startindex': 'channel_startindex'}, {'title': 'item_title', 'blip_safeusername': 'item_author', 'updated': 'item_pubdate', 'blip_puredescription': 'item_description', 'link': 'item_link', 'blip_picture': 'item_thumbnail', 'video': 'item_url', 'blip_runtime': 'item_duration', 'blip_rating': 'item_rating', 'width': 'item_width', 'height': 'item_height', 'language': 'item_lang'}]

        # The following url_ configs are based of the
        # http://blip.tv/about/api/
        self.config['base_url'] = "http://www.blip.tv%s"
        self.config['thumb_url'] = u"http://a.images.blip.tv%s"

        self.config[u'urls'] = {}

        # v2 api calls - An example that must be customized for each target site
        self.config[u'urls'][u'video.search'] = "http://www.blip.tv/search?q=%s;&page=%s;&pagelen=%s;&language_code=%s;&skin=rss"
        self.config[u'urls'][u'categories'] = "http://www.blip.tv/?section=categories&cmd=view&skin=api"

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions

        # Functions that parse video data from RSS data
        self.config['item_parser'] = {}
        self.config['item_parser']['main'] = self.getVideosForURL

        # Tree view url and the function that parses that urls meta data
        self.config[u'urls'][u'tree.view'] = {
            'P_R_R_F': {
                '__all__': ['http://www.blip.tv/%s/?skin=rss', 'main'],
                },
            'categories': {
                '__all__': ['http://www.blip.tv/rss/', 'main'],
                },
            }

        # Tree view categories are disabled until their results can be made more meaningful
        #self.tree_order = ['P_R_R_F', 'categories', ]
        self.tree_order = ['P_R_R_F']

        self.tree_org = {
#            'P_R_R_F': [['Popular/Recent/Features/Random ...', ['popular', 'recent', 'random', 'featured',]],
            'P_R_R_F': [['', ['popular', 'recent', 'random', 'featured',]],
                ],
            # categories are dynamically filled in from a list retrieved from the blip.tv site
            'categories': [
                ['Categories', u''],
                ],
            }

        self.tree_customize = {
            'P_R_R_F': {
                '__default__': { },
                #'cat name': {},
            },
            'categories': {
                '__default__': {'categories_id': u'', 'sort': u'', },
                #'cat name': {},
            },
            }

        self.feed_names = {
            'P_R_R_F': {'popular': 'Most Comments', 'recent': 'Most Recent', 'random': 'Random selection',
            },
            'categories': {'featured': 'Featured Videos', 'popular': 'Most Comments', 'recent': 'Most Recent', 'random': 'Random selection',
            },
            }

        self.feed_icons = {
            'P_R_R_F': {'popular': 'directories/topics/most_comments', 'recent': 'directories/topics/most_recent', 'random': 'directories/topics/random',
            },
            'categories': {'featured': 'directories/topics/featured', 'popular': 'directories/topics/most_comments', 'recent': 'directories/topics/most_recent', 'random': 'directories/topics/random',
            },
            }

        # Initialize the tree view flag so that the item parsing code can be used for multiple purposes
        self.categories = False
        self.treeview = False
        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/bliptv.png'
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
        '''Key word video search of the blip.tv web site
        return an array of matching item dictionaries
        return
        '''
        url = self.config[u'urls'][u'video.search'] % (urllib.quote_plus(title.encode("utf-8")), pagenumber, pagelen, self.config['language'])

        if self.config['debug_enabled']:
            print "Search URL:"
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            raise BliptvUrlError(self.error_messages['BliptvUrlError'] % (url, errormsg))

        if etree is None:
            raise BliptvVideoNotFound(u"1-No blip.tv Video matches found for search value (%s)" % title)

        # Massage each field and eliminate any item without a URL
        elements_final = []
        dictionary_first = False
        directory_image = u''
        self.next_page = False
        language = self.config['language']
        for elements in etree.find('channel'):
            if elements.tag == 'language':
                if elements.text:
                    language = elements.text[:2]
                continue
            if not elements.tag == 'item':
                continue
            item = {}
            item['language'] = language
            embedURL = u''
            for elem in elements:
                if elem.tag == 'title':
                    if elem.text:
                        item['title'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('safeusername'):
                    if elem.text:
                        item['blip_safeusername'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('pubDate'):
                    if elem.text:
                        item['updated'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('puredescription'):
                    if elem.text:
                        item['blip_puredescription'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('link'):
                    if elem.text:
                        item['link'] = self.ampReplace(elem.text.strip())
                    continue
                if elem.tag.endswith('embedUrl'):
                    if elem.text:
                        embedURL = self.ampReplace(elem.text.strip())
                    continue
                if elem.tag.endswith('thumbnail'):
                    if elem.get('url'):
                        item['blip_picture'] = self.ampReplace(elem.get('url').strip())
                    continue
                if elem.tag.endswith('group'):
                    file_size = 0
                    for e in elem:
                        if e.tag.endswith('content'):
                            if e.get('fileSize'):
                                try:
                                    if int(e.get('fileSize')) > file_size:
                                        item['video'] = self.ampReplace(e.get('url').strip())
                                        file_size = int(e.get('fileSize'))
                                except:
                                    pass
                            continue
                    continue
                if elem.tag.endswith('runtime'):
                    if elem.text:
                        item['blip_runtime'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('rating'):
                    if elem.text:
                        item['blip_rating'] = self.massageDescription(elem.text.strip())
                    continue
            if not item.has_key('video') and not item.has_key('link') and not embedURL:
                continue
            if embedURL:
                item['link'] = embedURL
            if item.has_key('link') and not item.has_key('video'):
                continue
            if item.has_key('video') and not item.has_key('link'):
                item['link'] = item['video']
            elements_final.append(item)

        if not len(elements_final):
            raise BliptvVideoNotFound(u"2-No blip.tv Video matches found for search value (%s)" % title)

        return elements_final
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        try:
            data = self.searchTitle(title, pagenumber, self.page_limit)
        except BliptvVideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            return None
        except BliptvUrlError, msg:
            sys.stderr.write(u'%s' % msg)
            sys.exit(1)
        except BliptvHttpError, msg:
            sys.stderr.write(self.error_messages['BliptvHttpError'] % msg)
            sys.exit(1)
        except BliptvRssError, msg:
            sys.stderr.write(self.error_messages['BliptvRssError'] % msg)
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

        # Channel details and search results
        channel = {'channel_title': u'blip.tv', 'channel_link': u'http://blip.tv', 'channel_description': u"We're the next generation television network", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        if len(items) == self.page_limit:
            channel['channel_numresults'] = self.page_limit * int(pagenumber) + 1
        else:
            channel['channel_numresults'] = self.page_limit * int(pagenumber)
        channel['channel_startindex'] = self.page_limit * int(pagenumber)
        channel['channel_returned'] = len(items)

        if len(items):
            return [[channel, items]]
        return None
    # end searchForVideos()


    def getCategories(self):
        '''Get the list of valid category ids and their name and update the proper dictionaries
        return nothing
        '''
        url = self.config[u'urls'][u'categories']
        if self.config['debug_enabled']:
            print "Category list URL:"
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            sys.stderr.write(self.error_messages['BliptvUrlError'] % (url, errormsg))
            self.tree_order.remove('categories')
            return

        if etree is None:
            sys.stderr.write(u'1-No Categories found at (%s)\n' % url)
            self.tree_order.remove('categories')
            return

        if not etree.find('payload'):
            sys.stderr.write(u'2-No Categories found at (%s)\n' % url)
            self.tree_order.remove('categories')
            return

        category = False
        for element in etree.find('payload'):
            if element.tag == 'category':
                tmp_name = u''
                tmp_id = u''
                for e in element:
                    if e.tag == 'id':
                        if e.text == '-1':
                            break
                        if e.text:
                            tmp_id = self.massageDescription(e.text.strip())
                    if e.tag == 'name':
                        if e.text:
                            tmp_name = self.massageDescription(e.text.strip())
                if tmp_id and tmp_name:
                    category = True
                    self.tree_org['categories'].append([tmp_name, ['popular', 'recent', 'random', 'featured',]])
                    self.feed_names['categories'][tmp_name] = tmp_id

        if not category:
            sys.stderr.write(u'3-No Categories found at (%s)\n' % url)
            self.tree_order.remove('categories')
            return

        self.tree_org['categories'].append([u'', u'']) # Adds a end of the Categories directory indicator

        return
    # end getCategories()

    def displayTreeView(self):
        '''Gather the categories/feeds/...etc then retrieve a max page of videos meta data in each of them
        return array of directories and their video meta data
        '''
        # Channel details and search results
        self.channel = {'channel_title': u'blip.tv', 'channel_link': u'http://blip.tv', 'channel_description': u"We're the next generation television network", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        if self.config['debug_enabled']:
            print self.config[u'urls']
            print

        # Get category ids
        self.getCategories()

        # Process the various video feeds/categories/... etc
        self.treeview = True
        dictionaries = []
        for key in self.tree_order:
            if key == 'categories':
                self.categories = True
            else:
                self.categories = False
            self.tree_key = key
            dictionaries = self.getVideos(self.tree_org[key], dictionaries)

        return [[self.channel, dictionaries]]
    # end displayTreeView()

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
                addition+=u'?%s=%s' %  (ky, additions[ky])
        index = URL.find('%')
        if index == -1:
            return (URL+addition)
        else:
            return (URL+addition) % self.feed
    # end makeURL()


    def getVideos(self, dir_dict, dictionaries):
        '''Parse a list made of categories and retrieve video meta data
        return a dictionary of directory names and categories video meta data
        '''
        for sets in dir_dict:
            if not isinstance(sets[1], list):
                if sets[0] != '': # Add the nested dictionaries display name
                    dictionaries.append([self.massageDescription(sets[0]), self.channel_icon])
                else:
                    dictionaries.append(['', u'']) # Add the nested dictionary indicator
                continue
            temp_dictionary = []
            for self.feed in sets[1]:
                if self.categories:
                    self.tree_customize[self.tree_key]['__default__']['categories_id'] = self.feed_names['categories'][sets[0]]
                    self.tree_customize[self.tree_key]['__default__']['sort'] = self.feed
                if self.config[u'urls'][u'tree.view'][self.tree_key].has_key('__all__'):
                    URL = self.config[u'urls'][u'tree.view'][self.tree_key]['__all__']
                else:
                    URL = self.config[u'urls'][u'tree.view'][self.tree_key][self.feed]
                temp_dictionary = self.config['item_parser'][URL[1]](self.makeURL(URL[0]), temp_dictionary)
            if len(temp_dictionary):
                if len(sets[0]): # Add the nested dictionaries display name
                    dictionaries.append([self.massageDescription(sets[0]), self.channel_icon])
                for element in temp_dictionary:
                    dictionaries.append(element)
                if len(sets[0]):
                    dictionaries.append(['', u'']) # Add the nested dictionary indicator
        return dictionaries
    # end getVideos()

    def getVideosForURL(self, url, dictionaries):
        '''Get the video meta data for url search
        return the video dictionary of directories and their video mata data
        '''
        initial_length = len(dictionaries)

        if self.config['debug_enabled']:
            print "Video URL:"
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            sys.stderr.write(self.error_messages['BliptvUrlError'] % (url, errormsg))
            return dictionaries

        if etree is None:
            sys.stderr.write(u'1-No Videos for (%s)\n' % self.feed)
            return dictionaries

        dictionary_first = False
        self.next_page = False
        language = self.config['language']
        for elements in etree.find('channel'):
            if elements.tag.endswith(u'language'):
                if elements.text:
                    language = elements.text[:2]
                continue

            if not elements.tag.endswith(u'item'):
                continue

            item = {}
            item['language'] = language
            embedURL = u''
            for elem in elements:
                if elem.tag == 'title':
                    if elem.text:
                        item['title'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('safeusername'):
                    if elem.text:
                        item['blip_safeusername'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('pubDate'):
                    if elem.text:
                        item['updated'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('puredescription'):
                    if elem.text:
                        item['blip_puredescription'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag == 'link':
                    if elem.text:
                        item['link'] = self.ampReplace(elem.text.strip())
                    continue
                if elem.tag.endswith('embedUrl'):
                    if elem.text:
                        embedURL = self.ampReplace(elem.text.strip())
                    continue
                if elem.tag.endswith('thumbnail'):
                    if elem.get('url'):
                        item['blip_picture'] = self.ampReplace(elem.get('url').strip())
                    continue
                if elem.tag.endswith('group'):
                    file_size = 0
                    for e in elem:
                        if e.tag.endswith('content'):
                            for key in e.keys():
                                if key.endswith('vcodec'):
                                    break
                            else:
                                continue
                            if e.get('fileSize'):
                                try:
                                    if int(e.get('fileSize')) > file_size:
                                        item['video'] = self.ampReplace(e.get('url').strip())
                                        file_size = int(e.get('fileSize'))
                                except:
                                    pass
                                if e.get('height'):
                                    item['height'] = e.get('height').strip()
                                if e.get('width'):
                                    item['width'] = e.get('width').strip()
                            continue
                    continue
                if elem.tag.endswith('runtime'):
                    if elem.text:
                        item['blip_runtime'] = self.massageDescription(elem.text.strip())
                    continue
                if elem.tag.endswith('rating'):
                    if elem.text:
                        item['blip_rating'] = self.massageDescription(elem.text.strip())
                    continue
            if not item.has_key('video') and not item.has_key('link'):
                continue
            if embedURL:
                item['link'] = embedURL
            if item.has_key('link') and not item.has_key('video'):
                continue
            if item.has_key('video') and not item.has_key('link'):
                item['link'] = item['video']

            if self.treeview:
                if not dictionary_first:  # Add the dictionaries display name
                    dictionaries.append([self.massageDescription(self.feed_names[self.tree_key][self.feed]), self.setTreeViewIcon()])
                    dictionary_first = True

            final_item = {}
            for key in self.key_translation[1].keys():
                if not item.has_key(key):
                    final_item[self.key_translation[1][key]] = u''
                else:
                    final_item[self.key_translation[1][key]] = item[key]
            dictionaries.append(final_item)

        if self.treeview:
            if initial_length < len(dictionaries): # Need to check if there was any items for this Category
                dictionaries.append(['', u'']) # Add the nested dictionary indicator
        return dictionaries
    # end getVideosForURL()
# end Videos() class
