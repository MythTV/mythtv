#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: joost_api - Simple-to-use Python interface to the Joost API (http://www.joost.com/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           metadata, video and image URLs from Joost. These routines are based on the api. Specifications
#           for this api are published at http://www.joost.com/test/doc/api/
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="joost_api - Simple-to-use Python interface to the Joost API (http://www.joost.com/test/doc/api/)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
metadata, video and image URLs from Joost. These routines are based on the api. Specifications
for this api are published at http://www.joost.com/test/doc/api/
'''

__version__="v0.2.2"
# 0.1.0 Initial development - Tree View Processing
# 0.1.1 Updated code and data structures to conform to the standard Tree view
# 0.1.2 Documentation updates
# 0.2.0 Public release
# 0.2.1 New python bindings conversion
#       Better exception error reporting
#       Better handling of invalid unicode data from source
# 0.2.1 Removed and unused import of the feedparser library

import os, struct, sys, re, time
from datetime import datetime, timedelta
import urllib, urllib2
import logging

try:
    import xml.etree.cElementTree as ElementTree
except ImportError:
    import xml.etree.ElementTree as ElementTree

from joost_exceptions import (JoostUrlError, JoostHttpError, JoostRssError, JoostVideoNotFound, JoostInvalidSearchType, JoostXmlError, JoostVideoDetailError)

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
        except IOError:
            raise JoostHttpError(errormsg)
        return urlhandle.read()

    def getEt(self):
        xml = self._grabUrl(self.url)
        try:
            et = ElementTree.fromstring(xml)
        except SyntaxError, errormsg:
            raise JoostXmlError(errormsg)
        return et


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
		sys.stderr(u'\n! Warning - %s\n' % e.args[0])
		filename = os.path.expanduser("~")+'/.mythtv/config.xml'
		if not os.path.isfile(filename):
			sys.stderr(u'\n! Warning - A correctly configured (%s) file must exist\n' % filename)
		else:
			sys.stderr(u'\n! Warning - Check that (%s) is correctly configured\n' % filename)
	except Exception, e:
		sys.stderr(u"\n! Warning - Creating an instance caused an error for one of: MythDB. error(%s)\n" % e)
except Exception, e:
	sys.stderr(u"\n! Warning - MythTV python bindings could not be imported. error(%s)\n" % e)
	mythdb = None

from socket import gethostname, gethostbyname


class Videos(object):
    """Main interface to http://www.joost.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports Tree view method
    The apikey is a not required to access http://www.joost.com/
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
            pass    # Joost does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages

        self.log_name = "joost"
        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        # Defaulting to ENGISH but the Joost apis do not support specifying a language
        self.config['language'] = "en"

        self.error_messages = {'JoostUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'JoostHttpError': u"! Error: An HTTP communications error with Joost was raised (%s)\n", 'JoostRssError': u"! Error: Invalid RSS meta data\nwas received from Joost error (%s). Skipping item.\n", 'JoostVideoNotFound': u"! Error: Video search with Joost did not return any results (%s)\n", 'JoostVideoDetailError': u"! Error: Invalid Video metadata detail\nwas received from Joost error (%s). Skipping item.\n", }

        # This is an example that must be customized for each target site
        self.key_translation = [{'channel_title': 'channel_title', 'channel_link': 'channel_link', 'channel_description': 'channel_description', 'channel_numresults': 'channel_numresults', 'channel_returned': 'channel_returned', 'channel_startindex': 'channel_startindex'}, {'title': 'item_title', 'media_credit': 'item_author', 'published_parsed': 'item_pubdate', 'media_description': 'item_description', 'video': 'item_link', 'thumbnail': 'item_thumbnail', 'link': 'item_url', 'duration': 'item_duration', 'item_rating': 'item_rating', 'item_width': 'item_width', 'item_height': 'item_height', 'language': 'item_lang'}]

        self.config[u'urls'] = {}

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions
        self.config[u'urls'] = {}

        # Functions that parse video data from RSS data
        self.config['item_parser'] = {}
        self.config['item_parser']['first'] = self.getVideosForURL_1
        self.config['item_parser']['second'] = self.getVideosForURL_2

        # v2 api calls - An example that must be customized for each target site
        self.config[u'urls'][u'tree.view'] = {
            'recent': {
                '__all__': ['http://www.joost.com/api/%s', 'first'],
                },
            'featured': {
                '__all__': ['http://www.joost.com/api/%s', 'second'],
                },
            'popular': {
                '__all__': ['http://www.joost.com/api/%s', 'first'],
                },
            'shared': {
                '__all__': ['http://www.joost.com/api/%s', 'second'],
                },
            'commented': {
                '__all__': ['http://www.joost.com/api/%s', 'second'],
                },
            }

        self.tree_order = ['recent', 'featured', 'popular', 'shared', 'commented', ]
        self.tree_org = {
            'recent': [['', ['recent/videos']], ],
            'featured': [['', ['featured/fav/get']], ],
            'popular': [['', ['popular/videos']], ],
            'shared': [['', ['popular/shared/videos']], ],
            'commented': [['', ['popular/commented/videos']], ],
            }

        self.tree_customize = {
            'topten': {
                '__default__': {'fmt': 'atom', },
                #'cat name': {'fmt': '', },
            },
            'recent':  {
                '__default__': {'fmt': 'atom', },
                #'cat name': {'fmt': '', },
            },
            'featured':  {
                '__default__': {'fmt': 'atom', },
                #'cat name': {'fmt': '', },
            },
            'popular':  {
                '__default__': {'fmt': 'atom', },
                #'cat name': {'fmt': '', },
            },
            'shared':  {
                '__default__': {'fmt': 'atom', },
                #'cat name': {'fmt': '', },
            },
            'commented':  {
                '__default__': {'fmt': 'atom', },
                #'cat name': {'fmt': '', },
            },
            }

        self.feed_names = {
            'recent': {'recent/videos': 'Most Recent'},
            'featured': {'featured/fav/get': 'Featured'},
            'popular': {'popular/videos': 'Most Viewed'},
            'shared': {'popular/shared/videos': 'Most Subscribed'},
            'commented': {'popular/commented/videos': 'Most Comments'},
            }

        self.feed_icons = {
            'recent': {'recent/videos': 'directories/topics/most_recent'},
            'featured': {'featured/fav/get': 'directories/topics/featured'},
            'popular': {'popular/videos': 'directories/topics/most_viewed'},
            'shared': {'popular/shared/videos': 'directories/topics/most_subscribed'},
            'commented': {'popular/commented/videos': 'directories/topics/most_comments'},
            }

        # Initialize the tree view flag so that the item parsing code can be used for multiple purposes
        self.treeview = False
        self.channel_icon = u'http://static.joost.com/banners/joost_007_en_125x125_mate.png'

        if mythdb:
            self.icon_dir = mythdb.settings[gethostname()]['mythnetvision.iconDir']
            if self.icon_dir:
                self.icon_dir = self.icon_dir.replace(u'//', u'/')
                self.setTreeViewIcon(dir_icon='joost')
                self.channel_icon = self.tree_dir_icon
    # end __init__()

###########################################################################################################
#
# Start - Utility functions
#
###########################################################################################################

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

###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################


    def displayTreeView(self):
        '''Gather the Joost Genres/Artists/...etc then get a max page of videos meta data in each of them
        return array of directories and their video metadata
        '''
        # Channel details and search results
        self.channel = {'channel_title': u'Joost', 'channel_link': u'http://www.joost.com', 'channel_description': u"What's Joost? It's a way to watch videos – music, TV, movies and more – over the Internet. We could just call it a website ... with videos ... but that's not the whole story.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        if self.config['debug_enabled']:
            print self.config[u'urls']
            print

        # Get videos within each category
        dictionaries = []

        # Process the various video feeds/categories/... etc
        for key in self.tree_order:
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
        '''Parse a list made of genres/artists ... etc lists and retrieve video meta data
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
    # end categoriesVideos()

    def getVideosForURL_1(self, url, dictionaries):
        '''Get the video metadata for url search
        Handles "recent" and "popular"
        return the video dictionary of directories and their video mata data
        '''
        initial_length = len(dictionaries)

        if self.config['debug_enabled']:
            print "URL:"
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            sys.stderr.write(self.error_messages['JoostUrlError'] % (url, errormsg))
            return dictionaries

        if etree is None:
            sys.stderr.write(u'1-No Videos for (%s)\n' % self.feed)
            return dictionaries

        dictionary_first = False
        self.channel['channel_startindex'] = self.page_limit
        self.channel['channel_returned'] = self.page_limit # False value CHANGE later
        for elements in etree:
            if not elements.tag.endswith(u'entry'):
                continue

            self.channel['channel_numresults']+=1

            metadata = {}
            cur_size = True
            flash = False
            metadata['language'] = self.config['language']
            for e in elements:
                if e.tag.endswith(u'title'):
                    metadata['title'] = self.massageDescription(e.text.strip())
                    continue
                if e.tag.endswith(u'author'):
                    for a in e:
                        if a.tag.endswith(u'name'):
                            metadata['author'] = self.massageDescription(a.text.strip())
                            break
                    continue
                if e.tag.endswith(u'updated'): # '2009-06-24T15:21:01Z'
                    if e.text != None:
                        pub_time = time.strptime(e.text.strip(), "%Y-%m-%dT%H:%M:%SZ")
                        metadata['published_parsed'] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                    continue
                if e.tag.endswith(u'id'):
                    metadata['video'] = self.ampReplace(e.text.replace(u'joost.com/', u'joost.com/embed/')+'?autoplay=1')
                    metadata['link'] = self.ampReplace(e.text.replace(u'joost.com/', u'joost.com/embed/')+'?autoplay=1')
                    continue
                if e.tag.endswith(u'thumbnail'):
                    metadata['thumbnail'] = self.ampReplace(e.text)
                    continue
                if e.tag.endswith(u'duration'):
                    metadata['duration'] = e.text
                    continue

            if not len(metadata):
                raise JoostVideoDetailError(u'2-No Video meta data for (%s)' % url)

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
    # end getVideosForURL_1()

    def getVideosForURL_2(self, url, dictionaries):
        '''Get the video metadata for url search
        Handles "favorite", "shared" and "commented"
        return the video dictionary of directories and their video mata data
        '''
        initial_length = len(dictionaries)

        if self.config['debug_enabled']:
            print "URL:"
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            sys.stderr.write(self.error_messages['JoostUrlError'] % (url, errormsg))
            return dictionaries

        if etree is None:
            sys.stderr.write(u'3-No Videos for (%s)\n' % self.feed)
            return dictionaries

        dictionary_first = False
        self.channel['channel_startindex'] = self.page_limit
        self.channel['channel_returned'] = self.page_limit # False value CHANGE later
        for elements in etree:
            if not elements.tag.endswith(u'entry'):
                continue

            self.channel['channel_numresults']+=1

            metadata = {}
            cur_size = True
            flash = False
            metadata['language'] = self.config['language']
            for e in elements:
                if e.tag.endswith(u'title'):
                    metadata['title'] = self.massageDescription(e.text.strip())
                    continue
                if e.tag.endswith(u'updated'): # '2009-06-24T15:21:01Z'
                    if e.text != None:
                        pub_time = time.strptime(e.text.strip(), "%Y-%m-%dT%H:%M:%SZ")
                        metadata['published_parsed'] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                    continue
                if e.tag.endswith(u'id'):
                    metadata['video'] = self.ampReplace(e.text.replace(u'joost.com/', u'joost.com/embed/')+'?autoplay=1')
                    metadata['link'] = self.ampReplace(e.text.replace(u'joost.com/', u'joost.com/embed/')+'?autoplay=1')
                    continue
                if e.tag.endswith(u'content'):
                    index = e.text.find(u'src="')
                    if index != -1:
                        index2 = e.text[index+5:].find(u'"')
                        metadata['thumbnail'] = self.ampReplace(e.text[index+5:index+5+index2].strip())
                    index = e.text.find(u'</a>')
                    if index != -1:
                        metadata['media_description'] = self.massageDescription(e.text[index+4:].strip())
                    continue

            if not len(metadata):
                raise JoostVideoDetailError(u'4-No Video meta data for (%s)' % url)

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
    # end getVideosForURL_2()

# end Videos() class
