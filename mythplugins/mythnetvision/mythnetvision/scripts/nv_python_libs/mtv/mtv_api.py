#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: mtv_api - Simple-to-use Python interface to the MTV API (http://www.mtv.com/)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           metadata, video and image URLs from MTV. These routines are based on the api. Specifications
#           for this api are published at http://developer.mtvnservices.com/docs
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="mtv_api - Simple-to-use Python interface to the MTV API (http://developer.mtvnservices.com/docs)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
metadata, video and image URLs from MTV. These routines are based on the api. Specifications
for this api are published at http://developer.mtvnservices.com/docs
'''

__version__="v0.2.3"
# 0.1.0 Initial development
# 0.1.1 Added Tree View Processing
# 0.1.2 Modified Reee view code and structure to be standandized across all grabbers
# 0.1.3 Added directory image access and display
# 0.1.4 Documentation review
# 0.2.0 Public release
# 0.2.1 New python bindings conversion
#       Better exception error reporting
#       Better handling of invalid unicode data from source
# 0.2.2 Complete abort error message display improvements
#       Removed the import and use of the feedparser library
# 0.2.3 Fixed an exception message output code error in two places

import os, struct, sys, re, time
from datetime import datetime, timedelta
import urllib, urllib2
import logging

try:
    import xml.etree.cElementTree as ElementTree
except ImportError:
    import xml.etree.ElementTree as ElementTree

from mtv_exceptions import (MtvUrlError, MtvHttpError, MtvRssError, MtvVideoNotFound, MtvInvalidSearchType, MtvXmlError, MtvVideoDetailError)

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

from socket import gethostname, gethostbyname


class XmlHandler:
    """Deals with retrieval of XML files from API
    """
    def __init__(self, url):
        self.url = url

    def _grabUrl(self, url):
        try:
            urlhandle = urllib.urlopen(url)
        except IOError, errormsg:
            raise MtvHttpError(errormsg)
        return urlhandle.read()

    def getEt(self):
        xml = self._grabUrl(self.url)
        try:
            et = ElementTree.fromstring(xml)
        except SyntaxError, errormsg:
            raise MtvXmlError(errormsg)
        return et


class Videos(object):
    """Main interface to http://www.mtv.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search methods
    The apikey is a not required to access http://www.mtv.com/
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
            pass    # MTV does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages

        self.log_name = "MTV"
        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        # Defaulting to ENGISH but the MTV apis do not support specifying a language
        self.config['language'] = "en"

        self.error_messages = {'MtvUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'MtvHttpError': u"! Error: An HTTP communications error with MTV was raised (%s)\n", 'MtvRssError': u"! Error: Invalid RSS metadata\nwas received from MTV error (%s). Skipping item.\n", 'MtvVideoNotFound': u"! Error: Video search with MTV did not return any results (%s)\n", 'MtvVideoDetailError': u"! Error: Invalid Video metadata detail\nwas received from MTV error (%s). Skipping item.\n", }

        # This is an example that must be customized for each target site
        self.key_translation = [{'channel_title': 'channel_title', 'channel_link': 'channel_link', 'channel_description': 'channel_description', 'channel_numresults': 'channel_numresults', 'channel_returned': 'channel_returned', 'channel_startindex': 'channel_startindex'}, {'title': 'item_title', 'media_credit': 'item_author', 'published_parsed': 'item_pubdate', 'media_description': 'item_description', 'video': 'item_link', 'thumbnail': 'item_thumbnail', 'link': 'item_url', 'duration': 'item_duration', 'item_rating': 'item_rating', 'item_width': 'item_width', 'item_height': 'item_height', 'language': 'item_lang'}]

        self.config[u'urls'] = {}

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions
        self.config[u'urls'] = {}

        # Functions that parse video data from RSS data
        self.config['item_parser'] = {}
        self.config['item_parser']['main'] = self.getVideosForURL

        # v2 api calls - An example that must be customized for each target site
        self.config[u'urls'][u'tree.view'] = {
            'new_genres': {
                '__all__': ['http://api.mtvnservices.com/1/genre/%s/videos/?', 'main'],
                },
            'genres': {
                '__all__': ['http://api.mtvnservices.com/1/genre/%s/videos/?', 'main'],
                },
            }

        self.tree_order = ['new_genres', 'genres', ]
        self.tree_org = {
            'new_genres': [['New over the last 3 months', ['pop', 'rock', 'metal', 'randb', 'jazz', 'blues_folk', 'country', 'latin', 'hip_hop', 'world_reggae', 'electronic_dance', 'easy_listening', 'classical', 'soundtracks_musicals', 'alternative', 'environmental', ]],
            ],
            'genres': [['All Genres', ['pop', 'rock', 'metal', 'randb', 'jazz', 'blues_folk', 'country', 'latin', 'hip_hop', 'world_reggae', 'electronic_dance', 'easy_listening', 'classical', 'soundtracks_musicals', 'alternative', 'environmental', ]],
            ],
            }

        # Time periods of videos e.g, &date=01011980-12311989 or MMDDYYYY-MMDDYYYY
        d1 = datetime.now()
        yr = d1 - timedelta(weeks=52)
        mts = d1 - timedelta(days=93)
        last_3_months = u'%s-%s' % (mts.strftime('%m%d%Y'), d1.strftime('%m%d%Y'))
        last_year = u'%s-%s' % (yr.strftime('%m%d%Y'), d1.strftime('%m%d%Y'))

        # http://api.mtvnservices.com/1/genre/rock/videos/?&max-results=20&start-index=1
        self.tree_customize = {
            'new_genres': { # ?term=%s&start-index=%s&max-results=%s
                '__default__': {'max-results': '20', 'start-index': '1', 'date': last_3_months, 'sort': 'date_descending'},
                #'cat name': {'max-results': '', 'start-index': '', 'date': '', 'sort': ''},
            },
            'genres': { # ?term=%s&start-index=%s&max-results=%s
                '__default__': {'max-results': '20', 'start-index': '1', 'sort': 'date_descending'},
                #'cat name': {'max-results': '', 'start-index': '', 'date': '', 'sort': ''},
                'rock': {'date': last_year, },
                'R&B': {'date': last_year, },
                'country': {'date': last_year, },
                'hip_hop': {'date': last_year, },
            },
            }

        self.feed_names = {
            'new_genres': {'world_reggae': 'World/Reggae', 'pop': 'Pop', 'metal': 'Metal', 'environmental': 'Environmental', 'latin': 'Latin', 'randb': 'R&B', 'rock': 'Rock', 'easy_listening': 'Easy Listening', 'jazz': 'Jazz', 'country': 'Country', 'hip_hop': 'Hip-Hop', 'classical': 'Classical', 'electronic_dance': 'Electro / Dance', 'blues_folk': 'Blues / Folk', 'alternative': 'Alternative', 'soundtracks_musicals': 'Soundtracks / Musicals', 'New over the last 3 months': 'directories/topics/month'
            },
            'genres': {'world_reggae': 'World/Reggae', 'pop': 'Pop', 'metal': 'Metal', 'environmental': 'Environmental', 'latin': 'Latin', 'randb': 'R&B', 'rock': 'Rock', 'easy_listening': 'Easy Listening', 'jazz': 'Jazz', 'country': 'Country', 'hip_hop': 'Hip-Hop', 'classical': 'Classical', 'electronic_dance': 'Electro / Dance', 'blues_folk': 'Blues / Folk', 'alternative': 'Alternative', 'soundtracks_musicals': 'Soundtracks / Musicals',
            },
            }

        self.feed_icons = {
            'new_genres': {'New over the last 3 months': 'directories/topics/recent', 'world_reggae': 'directories/music_genres/world_reggae', 'pop': 'directories/music_genres/pop', 'metal': 'directories/music_genres/metal', 'environmental': 'directories/music_genres/environmental', 'latin': 'directories/music_genres/latino', 'randb': 'directories/music_genres/rnb', 'rock': 'directories/music_genres/rock', 'easy_listening': 'directories/music_genres/easy_listening', 'jazz': 'directories/music_genres/jazz', 'country': 'directories/music_genres/country', 'hip_hop': 'directories/music_genres/hiphop', 'classical': 'directories/music_genres/classical', 'electronic_dance': 'directories/music_genres/electronic_dance', 'blues_folk': 'directories/music_genres/blues_folk', 'alternative': 'directories/music_genres/alternative', 'soundtracks_musicals': 'directories/music_genres/soundtracks_musicals',
            },
            'genres': {'Genres': 'directories/topics/music','world_reggae': 'directories/music_genres/world_reggae', 'pop': 'directories/music_genres/pop', 'metal': 'directories/music_genres/metal', 'environmental': 'directories/music_genres/environmental', 'latin': 'directories/music_genres/latino', 'randb': 'directories/music_genres/rnb', 'rock': 'directories/music_genres/rock', 'easy_listening': 'directories/music_genres/easy_listening', 'jazz': 'directories/music_genres/jazz', 'country': 'directories/music_genres/country', 'hip_hop': 'directories/music_genres/hiphop', 'classical': 'directories/music_genres/classical', 'electronic_dance': 'directories/music_genres/electronic_dance', 'blues_folk': 'directories/music_genres/blues_folk', 'alternative': 'directories/music_genres/alternative', 'soundtracks_musicals': 'directories/music_genres/soundtracks_musicals',
            },
            }

        # Initialize the tree view flag so that the item parsing code can be used for multiple purposes
        self.treeview = False
        self.channel_icon = u'http://nail.cc/brain/wp-content/uploads/2009/02/mtv_logo.jpg'

        if mythdb:
            self.icon_dir = mythdb.settings[gethostname()]['mythnetvision.iconDir']
            if self.icon_dir:
                self.icon_dir = self.icon_dir.replace(u'//', u'/')
                self.setTreeViewIcon(dir_icon='mtv')
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


    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the MTV web site
        return an array of matching item dictionaries
        return
        '''
        url = self.config[u'urls'][u'video.search'] % (urllib.quote_plus(title.encode("utf-8")), pagenumber , pagelen,)
        if self.config['debug_enabled']:
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            raise MtvUrlError(self.error_messages['MtvUrlError'] % (url, errormsg))

        if etree is None:
            raise MtvVideoNotFound(u"No MTV Video matches found for search value (%s)" % title)

        data = []
        for entry in etree:
            if not entry.tag.endswith('entry'):
                continue
            item = {}
            for parts in entry:
                if parts.tag.endswith('id'):
                    item['id'] = parts.text
                if parts.tag.endswith('title'):
                    item['title'] = parts.text
                if parts.tag.endswith('author'):
                    for e in parts:
                        if e.tag.endswith('name'):
                            item['media_credit'] = e.text
                            break
                if parts.tag.endswith('published'):
                    item['published_parsed'] = parts.text
                if parts.tag.endswith('description'):
                    item['media_description'] = parts.text
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

            video_details = None
            try:
                video_details = self.videoDetails(item['id'])
            except MtvUrlError, msg:
                sys.stderr.write(self.error_messages['MtvUrlError'] % msg)
            except MtvVideoDetailError, msg:
                sys.stderr.write(self.error_messages['MtvVideoDetailError'] % msg)
            except Exception, e:
                sys.stderr.write(u"! Error: Unknown error while retrieving a Video's meta data. Skipping video.' (%s)\nError(%s)\n" % (title, e))

            if video_details:
                for key in video_details.keys():
                    item[key] = video_details[key]

            item['language'] = u''
            for key in item.keys():
                if key == 'content':
                    if len(item[key]):
                        if item[key][0].has_key('language'):
                            if item[key][0]['language'] != None:
                                item['language'] = item[key][0]['language']
                if key == 'published_parsed': # '2009-12-21T00:00:00Z'
                    if item[key]:
                        pub_time = time.strptime(item[key].strip(), "%Y-%m-%dT%H:%M:%SZ")
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
                        item[key] = item[key].replace('"\n',' ').strip()
            elements_final.append(item)

        if not len(elements_final):
            raise MtvVideoNotFound(u"No MTV Video matches found for search value (%s)" % title)

        return elements_final
        # end searchTitle()


    def videoDetails(self, url):
        '''Using the passed URL retrieve the video meta data details
        return a dictionary of video metadata details
        return
        '''
        if self.config['debug_enabled']:
            print url
            print

        try:
            etree = XmlHandler(url).getEt()
        except Exception, errormsg:
            raise MtvUrlError(self.error_messages['MtvUrlError'] % (url, errormsg))

        if etree is None:
            raise MtvVideoDetailError(u'1-No Video meta data for (%s)' % url)

        metadata = {}
        cur_size = True
        for e in etree:
            if e.tag.endswith(u'content') and e.text == None:
                metadata['video'] =  e.get('url')
                metadata['duration'] =  e.get('duration')
            if e.tag.endswith(u'player'):
                metadata['link'] = e.get('url')
            if e.tag.endswith(u'thumbnail'):
                if cur_size == False:
                    continue
                height = e.get('height')
                width = e.get('width')
                if int(width) > cur_size:
                    metadata['thumbnail'] = e.get('url')
                    cur_size = int(width)
                if int(width) >= 200:
                    cur_size = False
                    break

        if not len(metadata):
            raise MtvVideoDetailError(u'2-No Video meta data for (%s)' % url)

        if not metadata.has_key('video'):
            metadata['video'] =  metadata['link']
            metadata['duration'] =  u''
        else:
            metadata['link'] =  metadata['video']

        return metadata
        # end videoDetails()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # v2 api calls - An example that must be customized for each target site
        if self.grabber_title == 'MTV':
            self.config[u'urls'][u'video.search'] = "http://api.mtvnservices.com/1/video/search/?term=%s&start-index=%s&max-results=%s"
        elif self.grabber_title == 'MTV Artists': # This search type is not currently implemented
            self.config[u'urls'][u'video.search'] = "http://api.mtvnservices.com/1/artist/search/?term=%s&start-index=%s&max-results=%s"
        else:
            sys.stderr.write(u"! Error: MtvInvalidSearchType - The grabber name (%s) is invalid \n" % self.grabber_title)
            sys.exit(1)


        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()


        startindex = (int(pagenumber) -1) * self.page_limit + 1
        try:
            data = self.searchTitle(title, startindex, self.page_limit)
        except MtvVideoNotFound, msg:
            sys.stderr.write(u"%s\n" % msg)
            return None
        except MtvUrlError, msg:
            sys.stderr.write(u'%s\n' % msg)
            sys.exit(1)
        except MtvHttpError, msg:
            sys.stderr.write(self.error_messages['MtvHttpError'] % msg)
            sys.exit(1)
        except MtvRssError, msg:
            sys.stderr.write(self.error_messages['MtvRssError'] % msg)
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
        channel = {'channel_title': u'MTV', 'channel_link': u'http://www.mtv.com', 'channel_description': u"Visit MTV (Music Television) for TV shows, music videos, celebrity photos, news.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        if len(items) == self.page_limit:
            channel['channel_numresults'] = self.page_limit * int(pagenumber) + 1
        elif len(items) < self.page_limit:
            channel['channel_numresults'] = self.page_limit * (int(pagenumber)-1) + len(items)
        else:
            channel['channel_numresults'] = self.page_limit * int(pagenumber)
        channel['channel_startindex'] = self.page_limit * int(pagenumber)
        channel['channel_returned'] = len(items)

        if len(items):
            return [[channel, items]]
        return None
    # end searchForVideos()


    def displayTreeView(self):
        '''Gather the MTV Genres/Artists/...etc then get a max page of videos meta data in each of them
        return array of directories and their video metadata
        '''
        # Channel details and search results
        self.channel = {'channel_title': u'MTV', 'channel_link': u'http://www.mtv.com', 'channel_description': u"Visit MTV (Music Television) for TV shows, music videos, celebrity photos, news.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        if self.config['debug_enabled']:
            print self.config[u'urls']
            print

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
            sys.stderr.write(self.error_messages['MtvUrlError'] % (url, errormsg))
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
            metadata['language'] = self.config['language']
            for e in elements:
                if e.tag.endswith(u'title'):
                    if e.text != None:
                        metadata['title'] = self.massageDescription(e.text.strip())
                    else:
                        metadata['title'] = u''
                    continue
                if e.tag == u'content':
                    if e.text != None:
                        metadata['media_description'] = self.massageDescription(e.text.strip())
                    else:
                        metadata['media_description'] = u''
                    continue
                if e.tag.endswith(u'published'): # '2007-03-06T00:00:00Z'
                    if e.text != None:
                        pub_time = time.strptime(e.text.strip(), "%Y-%m-%dT%H:%M:%SZ")
                        metadata['published_parsed'] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                    else:
                        metadata['published_parsed'] = u''
                    continue
                if e.tag.endswith(u'content') and e.text == None:
                    metadata['video'] =  self.ampReplace(e.get('url'))
                    metadata['duration'] =  e.get('duration')
                    continue
                if e.tag.endswith(u'player'):
                    metadata['link'] = self.ampReplace(e.get('url'))
                    continue
                if e.tag.endswith(u'thumbnail'):
                    if cur_size == False:
                        continue
                    height = e.get('height')
                    width = e.get('width')
                    if int(width) > cur_size:
                        metadata['thumbnail'] = self.ampReplace(e.get('url'))
                        cur_size = int(width)
                    if int(width) >= 200:
                        cur_size = False
                    continue
                if e.tag.endswith(u'author'):
                    for a in e:
                        if a.tag.endswith(u'name'):
                            if a.text:
                                metadata['media_credit'] = self.massageDescription(a.text.strip())
                            else:
                                metadata['media_credit'] = u''
                            break
                    continue

            if not len(metadata):
                raise MtvVideoDetailError(u'2-No Video meta data for (%s)' % url)

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
