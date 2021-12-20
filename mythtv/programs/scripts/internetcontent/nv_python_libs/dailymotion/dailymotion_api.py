# -*- coding: UTF-8 -*-

# ----------------------
# Name: dailymotion_api - Simple-to-use Python interface to the dailymotion API (http://www.dailymotion.com)
# Python Script
# Author:   R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           meta data, video and image URLs from dailymotion.
#           These routines are based on the api. Specifications
#           for this api are published at http://www.dailymotion.com/ca-en/doc/api/player
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="dailymotion_api - Simple-to-use Python interface to the dailymotion API (http://www.dailymotion.com/ca-en/doc/api/player)"
__author__="R.D. Vaughan"
__purpose__='''
This python script is intended to perform a variety of utility functions to search and access text
meta data, video and image URLs from dailymotion. These routines are based on the api. Specifications
for this api are published at http://www.dailymotion.com/ca-en/doc/api/player
'''

__version__="v0.2.4"
# 0.1.0 Initial development
# 0.1.1 Added getting local directory images
# 0.1.2 Documentation update
# 0.2.0 Public release
# 0.2.1 New python bindings conversion
#       Better exception error reporting
#       Better handling of invalid unicode data from source
# 0.2.2 Completed abort exception display message improvements
#       Removed unused import of the feedparser library
# 0.2.3 Fixed an exception message output code error in two places
# 0.2.4 Removed the need for python MythTV bindings and added "%SHAREDIR%" to icon directory path


import os, struct, sys, re, time
import urllib.request, urllib.parse, urllib.error, urllib.request, urllib.error, urllib.parse
import logging
from MythTV import MythXML

try:
    import xml.etree.cElementTree as ElementTree
except ImportError:
    import xml.etree.ElementTree as ElementTree

from .dailymotion_exceptions import (DailymotionUrlError, DailymotionHttpError, DailymotionRssError, DailymotionVideoNotFound, DailymotionInvalidSearchType, DailymotionXmlError, DailymotionVideoDetailError, DailymotionCategoryNotFound)
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


class XmlHandler:
    """Deals with retrieval of XML files from API
    """
    def __init__(self, url):
        self.url = url

    def _grabUrl(self, url):
        try:
            urlhandle = urllib.request.urlopen(url)
        except IOError as errormsg:
            raise DailymotionHttpError(errormsg)
        return urlhandle.read()

    def getEt(self):
        xml = self._grabUrl(self.url)
        try:
            et = ElementTree.fromstring(xml)
        except SyntaxError as errormsg:
            raise DailymotionXmlError(errormsg)
        return et


class Videos(object):
    """Main interface to http://www.dailymotion.com
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search and tree view methods
    The apikey is a not required to access http://www.dailymotion.com
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
            pass    # Dailymotion does not require an apikey

        self.config['debug_enabled'] = debug # show debugging messages

        self.log_name = "Dailymotion"
        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        self.error_messages = {'DailymotionUrlError': "! Error: The URL (%s) cause the exception error (%s)\n", 'DailymotionHttpError': "! Error: An HTTP communicating error with Dailymotion was raised (%s)\n", 'DailymotionRssError': "! Error: Invalid RSS meta data\nwas received from Dailymotion error (%s). Skipping item.\n", 'DailymotionVideoNotFound': "! Error: Video search with Dailymotion did not return any results (%s)\n", 'DailymotionVideoDetailError': "! Error: Invalid Video meta data detail\nwas received from Dailymotion error (%s). Skipping item.\n", }

        # This is an example that must be customized for each target site
        self.key_translation = [{'channel_title': 'channel_title', 'channel_link': 'channel_link', 'channel_description': 'channel_description', 'channel_numresults': 'channel_numresults', 'channel_returned': 'channel_returned', 'channel_startindex': 'channel_startindex'}, {'title': 'item_title', 'author': 'item_author', 'published_parsed': 'item_pubdate', 'media_description': 'item_description', 'video': 'item_link', 'thumbnail': 'item_thumbnail', 'link': 'item_url', 'duration': 'item_duration', 'rating': 'item_rating', 'item_width': 'item_width', 'item_height': 'item_height', 'language': 'item_lang'}]

        # Defaulting to English 'en'
        if language:
            self.config['language'] = language
        else:
            self.config['language'] = 'en'


        self.config['urls'] = {}

        # v2 api calls - An example that must be customized for each target site
        self.config['urls']['video.search'] = 'http://www.dailymotion.com/rss/relevance/search/%s/%s'
        self.config['urls']['group.search'] = 'http://www.dailymotion.com/rss/groups/relevance/search/%s/%s'
        self.config['urls']['user.search'] = 'http://www.dailymotion.com/rss/users/relevance/search/%s/%s'

        # Functions that parse video data from RSS data
        self.config['item_parser'] = {}
        self.config['item_parser']['main'] = self.getVideosForURL
        self.config['item_parser']['groups'] = self.getVideosForGroupURL

        # Tree view url and the function that parses that urls meta data
        self.config['urls']['tree.view'] = {
            'F_M_B_C': {
                'featured': ['http://www.dailymotion.com/rss/us/relevance/%s/search/movie/1', 'main'],
                'creative': ['http://www.dailymotion.com/rss/us/relevance/%s/search/movie/1', 'main'],
                'official': ['http://www.dailymotion.com/rss/us/%s/search/movie/1', 'main'],
                'hd': ['http://www.dailymotion.com/rss/relevance/%s/search/movie/1', 'main'],
                'commented': ['http://www.dailymotion.com/rss/us/%s/official/search/movie/1', 'main'],
                'visited': ['http://www.dailymotion.com/rss/us/%s/official/search/movie/1', 'main'],
                'rated': ['http://www.dailymotion.com/rss/us/%s/official/search/movie/1', 'main'],
                'relevance-month': ['http://www.dailymotion.com/rss/us/%s/official/search/movie/1', 'main'],
                'relevance-week': ['http://www.dailymotion.com/rss/us/%s/official/search/movie/1', 'main'],
                'relevance-today': ['http://www.dailymotion.com/rss/us/%s/official/search/movie/1', 'main'],
                },
            'categories': {
                'movie': ['http://www.dailymotion.com/rss/us/relevance/official/search/%s', 'main'],
                'comedy': ['http://www.dailymotion.com/rss/us/relevance/official/search/%s', 'main'],
                'short+film': ['http://www.dailymotion.com/rss/creative/relevance/search/%s', 'main'],
                'television': ['http://www.dailymotion.com/rss/us/relevance/official/search/%s', 'main'],
                'documentary': ['http://www.dailymotion.com/rss/us/relevance/official/search/%s', 'main'],
                'festival': ['http://www.dailymotion.com/rss/creative/relevance/search/%s', 'main'],
                },
            'channels': {
                'animals_motionmaker': ['http://www.dailymotion.com/rss/creative/lang/en/channel/animals/1', 'main'],
                'animals_most_recent': ['http://www.dailymotion.com/rss/lang/en/channel/animals/1', 'main'],
                'animals_hd': ['http://www.dailymotion.com/rss/hd/channel/animals/1', 'main'],
                'animals_official_users': ['http://www.dailymotion.com/rss/official/lang/en/channel/animals/1', 'main'],
                'animals_most_viewed': ['http://www.dailymotion.com/rss/visited/lang/en/channel/animals/1', 'main'],
                'animals_best_rated': ['http://www.dailymotion.com/rss/rated/lang/en/channel/animals/1', 'main'],

                'auto_most_recent': ['http://www.dailymotion.com/rss/lang/en/channel/auto/1', 'main'],
                'auto_motionmaker': ['http://www.dailymotion.com/rss/creative/channel/auto/1', 'main'],
                'auto_most_viewed': ['http://www.dailymotion.com/rss/visited/lang/en/channel/auto/1', 'main'],
                'auto_official_users': ['http://www.dailymotion.com/rss/official/channel/auto/1', 'main'],

                 'film_TV_movie': ['http://www.dailymotion.com/rss/us/relevance/official/search/movie', 'main'],
                 'film_TV_comedy': ['http://www.dailymotion.com/rss/us/relevance/official/search/comedy', 'main'],
                 'film_TV_short_film': ['http://www.dailymotion.com/rss/creative/relevance/search/short+film', 'main'],
                 'film_TV_television': ['http://www.dailymotion.com/rss/us/relevance/official/search/television', 'main'],
                 'film_TV_documentary': ['http://www.dailymotion.com/rss/us/relevance/official/search/documentary', 'main'],
                 'film_TV_festival': ['http://www.dailymotion.com/rss/official/relevance/search/festival', 'main'],

                 'gaming_Trailer': ['http://www.dailymotion.com/rss/official/search/videogames+trailer/1', 'main'],
                 'gaming_Lego': ['http://www.dailymotion.com/rss/relevance/search/lego', 'main'],
                 'gaming_Machinima': ['http://www.dailymotion.com/rss/relevance/creative/search/machinima/1', 'main'],
                 'gaming_Motionmaker': ['http://www.dailymotion.com/rss/us/creative/channel/videogames/1', 'main'],
                 'gaming_Review': ['http://www.dailymotion.com/rss/official/search/videogames+review/1', 'main'],
                 'gaming_News': ['http://www.dailymotion.com/rss/official/search/videogames+news/1', 'main'],
                 'gaming_recent': ['http://www.dailymotion.com/rss/us/channel/videogames/lang/en/1', 'main'],
                 'gaming_users': ['http://www.dailymotion.com/rss/us/official/channel/videogames/1', 'main'],

                 'lifestyle_best_rated': ['http://www.dailymotion.com/rss/rated/lang/en/channel/lifestyle/1', 'main'],
                 'lifestyle_most_commented': ['http://www.dailymotion.com/rss/commented/lang/en/channel/lifestyle/1', 'main'],
                 'lifestyle_most_viewed': ['http://www.dailymotion.com/rss/viewed/lang/en/channel/lifestyle/1', 'main'],
                 'lifestyle_HD': ['http://www.dailymotion.com/rss/hd/lang/en/channel/lifestyle/1', 'main'],

                 'news_politics_Politics': ['http://www.dailymotion.com/rss/official/relevance/search/politics', 'main'],
                 'news_politics_Celebrity': ['http://www.dailymotion.com/rss/official/relevance/search/celeb+news', 'main'],
                 'news_politics_official_users': ['http://www.dailymotion.com/rss/official/search/news/1', 'main'],
                 'news_politics_International': ['http://www.dailymotion.com/rss/official/search/international+news/1', 'main'],
                 'news_politics_Entertainment': ['http://www.dailymotion.com/rss/official/search/entertainment+news/1', 'main'],
                 'news_politics_Motionmakers': ['http://www.dailymotion.com/rss/relevance/creative/search/news/1', 'main'],

                 'sports_extreme_surf': ['http://www.dailymotion.com/rss/relevance/official/search/surf/1', 'main'],
                 'sports_extreme_baseball': ['http://www.dailymotion.com/rss/relevance/official/search/baseball', 'main'],
                 'sports_extreme_wrestling': ['http://www.dailymotion.com/rss/relevance/official/search/wrestling/1', 'main'],
                 'sports_extreme_BMX': ['http://www.dailymotion.com/rss/relevance/official/search/BMX/1', 'main'],

                 'webcam_vlogs_Most_viewed': ['http://www.dailymotion.com/rss/visited/channel/webcam/lang/en/1', 'main'],
                 'webcam_vlogs_Featured_videos': ['http://www.dailymotion.com/rss/us/featured/channel/webcam/1', 'main'],
                 'webcam_vlogs_Best_rated': ['http://www.dailymotion.com/rss/rated/channel/webcam/lang/en/1', 'main'],
                 'webcam_vlogs_Most_Commented': ['http://www.dailymotion.com/rss/commented/channel/webcam/lang/en/1', 'main'],

                 'arts_Animation': ['http://www.dailymotion.com/rss/relevance/creative-official/search/animation', 'main'],
                 'arts_Short_Film': ['http://www.dailymotion.com/rss/us/relevance/creative/search/shortfilm', 'main'],
                 'arts_Motionmaker': ['http://www.dailymotion.com/rss/creative-official/lang/en/1', 'main'],
                 'arts_Official_Users': ['http://www.dailymotion.com/rss/official/channel/creation/lang/en/1', 'main'],
                 'arts_Suede_swede': ['http://www.dailymotion.com/rss/relevance/creative/search/suede', 'main'],
                 'arts_Most_recent': ['http://www.dailymotion.com/rss/channel/creation/lang/en/1', 'main'],

                 'college_Most_viewed': ['http://www.dailymotion.com/rss/visited/channel/school/lang/en/1', 'main'],
                 'college_HD_videos': ['http://www.dailymotion.com/rss/hd/channel/school/1', 'main'],
                 'college_Best_rated': ['http://www.dailymotion.com/rss/rated/channel/school/lang/en/1', 'main'],
                 'college_Most_commented': ['http://www.dailymotion.com/rss/channel/school/lang/en/1', 'main'],

                 'funny_STAND UP': ['http://www.dailymotion.com/rss/official/relevance/search/standup', 'main'],
                 'funny_Hulu: NBC & Fox': ['http://www.dailymotion.com/rss/hulu/1', 'main'],
                 'funny_My Damn Channel': ['http://www.dailymotion.com/rss/mydamnchannel/1', 'main'],
                 'funny_Motionmakers': ['http://www.dailymotion.com/rss/us/creative/search/comedy/1', 'main'],
                 'funny_Sketch': ['http://www.dailymotion.com/rss/official/relevance/search/sketch', 'main'],
                 "funny_Nat'l Lampoon": ['http://www.dailymotion.com/rss/nationallampoon/1', 'main'],
                 'funny_Classic Sitcoms': ['http://www.dailymotion.com/rss/Classic_WB_TV/1', 'main'],
                 'funny_Official Users': ['http://www.dailymotion.com/rss/us/official/search/comedy/1', 'main'],

                 'latino_Featured Videos': ['http://www.dailymotion.com/rss/featured/channel/latino/1', 'main'],
                 'latino_HD content': ['http://www.dailymotion.com/rss/hd/channel/latino/1', 'main'],
                 'latino_Official Content': ['http://www.dailymotion.com/rss/official/channel/latino/1', 'main'],
                 'latino_Creative Content': ['http://www.dailymotion.com/rss/creative/channel/latino/1', 'main'],
                 'latino_Most Commented': ['http://www.dailymotion.com/rss/commented-week/featured/channel/latino/1', 'main'],
                 'latino_Most Viewed': ['http://www.dailymotion.com/rss/visited-week/featured/channel/latino/1', 'main'],
                 'latino_Best Rated': ['http://www.dailymotion.com/rss/rated-week/featured/channel/latino/1', 'main'],

                 'music_Pop': ['http://www.dailymotion.com/rss/official/relevance/search/pop', 'main'],
                 'music_Rock': ['http://www.dailymotion.com/rss/relevance/official/search/Rock+Music+Videos/1', 'main'],
                 'music_Jazz': ['http://www.dailymotion.com/rss/channel/music/official/relevance/search/jazz', 'main'],
                 'music_Covers': ['http://www.dailymotion.com/rss/channel/us/channel/music/relevance/creative/search/cover/1', 'main'],
                 'music_Rap': ['http://www.dailymotion.com/rss/official/relevance/search/rap', 'main'],
                 'music_R&B': ['http://www.dailymotion.com/rss/us/channel/music/relevance/search/rnb', 'main'],
                 'music_Metal': ['http://www.dailymotion.com/rss/channel/us/channel/music/official/relevance/search/metal', 'main'],
                 'music_Electro': ['http://www.dailymotion.com/rss/channel/music/official/relevance/search/electro', 'main'],

                 'People_Family_Featured Videos': ['http://www.dailymotion.com/rss/featured/channel/people/1', 'main'],
                 'People_Family_HD content': ['http://www.dailymotion.com/rss/hd/channel/people/1', 'main'],
                 'People_Family_Official Content': ['http://www.dailymotion.com/rss/official/channel/people/1', 'main'],
                 'People_Family_Creative Content': ['http://www.dailymotion.com/rss/creative/channel/people/1', 'main'],
                 'People_Family_Most Commented': ['http://www.dailymotion.com/rss/commented-week/featured/channel/people/1', 'main'],
                 'People_Family_Most Viewed': ['http://www.dailymotion.com/rss/visited-week/featured/channel/people/1', 'main'],
                 'People_Family_Best Rated': ['http://www.dailymotion.com/rss/rated-week/featured/channel/people/1', 'main'],

                 'Tech_Science_Most recent': ['http://www.dailymotion.com/rss/channel/tech/1', 'main'],
                 'Tech_Science_Most viewed': ['http://www.dailymotion.com/rss/visited/channel/tech/1', 'main'],
                 'Tech_Science_Most commented': ['http://www.dailymotion.com/rss/commented/channel/tech/1', 'main'],
                 'Tech_Science_Best rated': ['http://www.dailymotion.com/rss/rated/channel/tech/1', 'main'],

#                 'Travel_Most recent': ['', 'main'],
#                 'Travel_Most commented': ['', 'main'],
#                 'Travel_Best rated ': ['', 'main'],

                },

            'groups': {
                'F_M_B_C_Featured': ['http://www.dailymotion.com/rss/groups/featured', 'groups'],
                'F_M_B_C_Most Recent': ['http://www.dailymotion.com/rss/groups/1', 'groups'],
                'F_M_B_C_Most Active': ['http://www.dailymotion.com/rss/groups/active', 'groups'],
                'F_M_B_C_Month': ['http://www.dailymotion.com/rss/groups/active-month', 'groups'],
                'F_M_B_C_Week': ['http://www.dailymotion.com/rss/groups/active-week', 'groups'],
                'F_M_B_C_Today': ['http://www.dailymotion.com/rss/groups/active-today', 'groups'],
                },

            'group': {
                'group_Top rated': ['http://www.dailymotion.com/rss/rated/group', 'main'],
                'group_Most viewed': ['http://www.dailymotion.com/rss/visited/group', 'main'],
                'group_Most commented': ['http://www.dailymotion.com/rss/commented/group', 'main'],
                },

                }

        self.config['image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions

###CHANGE
        self.tree_order = ['F_M_B_C', 'categories', 'channels',]
        #self.tree_order = ['F_M_B_C', 'categories', 'channels', 'groups']

        self.tree_org = {
            'F_M_B_C': [['Featured/Most/Best/Current ...', ['featured', 'creative', 'official', 'hd', 'commented', 'visited', 'rated', 'relevance-month', 'relevance-week', 'relevance-today']],
                ],
            'categories': [
                ['Categories', ['movie', 'comedy', 'short+film', 'television', 'documentary', 'festival', ]],
                ],
            'channels': [
                ['Video Channels', ''],

                ['Animals', ['animals_motionmaker', 'animals_most_recent', 'animals_hd', 'animals_official_users', 'animals_most_viewed', 'animals_best_rated', ]],

                ['Auto-Moto', ['auto_most_recent', 'auto_motionmaker', 'auto_most_viewed', 'auto_official_users', ]],

                ['Film & TV', ['film_TV_movie', 'film_TV_comedy', 'film_TV_short_film', 'film_TV_television', 'film_TV_documentary', 'film_TV_festival', ]],

                ['Gaming', ['gaming_Trailer', 'gaming_Lego', 'gaming_Machinima', 'gaming_Motionmaker', 'gaming_Review', 'gaming_News', 'gaming_recent', 'gaming_users', ]],

                ['Life & Style', ['lifestyle_best_rated', 'lifestyle_most_commented', 'lifestyle_most_viewed', 'lifestyle_HD', ]],

                ['News & Politics', ['news_politics_Politics', 'news_politics_Celebrity', 'news_politics_official_users', 'news_politics_International', 'news_politics_Entertainment', 'news_politics_Motionmakers', ]],

                ['Sports & Extreme', ['sports_extreme_surf', 'sports_extreme_baseball', 'sports_extreme_wrestling', 'sports_extreme_BMX', ]],

                ['Webcam & Vlogs', ['webcam_vlogs_Most_viewed', 'webcam_vlogs_Featured_videos', 'webcam_vlogs_Best_rated', 'webcam_vlogs_Most_Commented', ]],

                ['Arts', ['arts_Animation', 'arts_Short_Film', 'arts_Motionmaker', 'arts_Official_Users', 'arts_Suede_swede', 'arts_Most_recent', ]],

                ['College', ['college_Most_viewed', 'college_HD_videos', 'college_Best_rated', 'college_Most_commented', ]],

                ['Funny', ['funny_STAND UP', 'funny_Hulu: NBC & Fox', 'funny_My Damn Channel', 'funny_Motionmakers', 'funny_Sketch', "funny_Nat'l Lampoon", 'funny_Classic Sitcoms', 'funny_Official Users', ]],

                ['Latino', ['latino_Featured Videos', 'latino_HD content', 'latino_Official Content', 'latino_Creative Content', 'latino_Most Commented', 'latino_Most Viewed', 'latino_Best Rated', ]],

                ['Music', ['music_Pop', 'music_Rock', 'music_Jazz', 'music_Covers', 'music_Rap', 'music_R&B', 'music_Metal', 'music_Electro', ]],

                ['People & Family', ['People_Family_Featured Videos', 'People_Family_HD content', 'People_Family_Official Content', 'People_Family_Creative Content', 'People_Family_Most Commented', 'People_Family_Most Viewed', 'People_Family_Best Rated', ]],

                ['Tech & Science', ['Tech_Science_Most recent', 'Tech_Science_Most viewed', 'Tech_Science_Most commented', 'Tech_Science_Best rated', ]],

                ['', ''],
                ],

            'groups': [
                ['Groups', ''],

                ['Featured', ['F_M_B_C_Featured']],
                ['Most Recent', ['F_M_B_C_Most Recent']],
                ['Most Active', ['F_M_B_C_Most Active']],
                ['Month', ['F_M_B_C_Month']],
                ['Week', ['F_M_B_C_Week']],
                ['Today', ['F_M_B_C_Today']],

                ['', ''],
                ],

            'group': [
                ['', ['group_Top rated', 'group_Most viewed', 'group_Most commented', ], ],
                ],

            }

        self.tree_customize = {
            'F_M_B_C': {
                '__default__': { },
                #'cat name': {},
            },
            'categories': {
                '__default__': { },
                #'cat name': {},
            },
            'channels': {
                '__default__': { },
                #'cat name': {},
            },
            'groups': {
                '__default__': { },
                #'cat name': {},
            },
            'group': {
                '__default__': {'add_': '' },
                #'cat name': {},
            },
            }

        self.feed_names = {
            'F_M_B_C': {'featured': 'Featured Videos', 'creative': 'Creative Content', 'official': 'Most Recent', 'hd': 'HD content', 'commented': 'Most Comments', 'visited': 'Most Viewed', 'rated': 'Highest Rated', 'relevance-month': 'Month', 'relevance-week': 'Week', 'relevance-today': 'Today'
            },
            'categories': {'movie': 'Trailers', 'comedy': 'Comedy', 'short+film': 'Short Films', 'television': 'TV Clips', 'documentary': 'Documentaries', 'festival': 'Festivals',
            },
            'channels': {
                'animals_motionmaker': 'Motionmaker', 'animals_most_recent': 'Most recent', 'animals_hd': 'HD videos', 'animals_official_users': 'Official users', 'animals_most_viewed': 'Most Viewed', 'animals_best_rated': 'Highest Rated',

                'auto_most_recent': 'Most recent', 'auto_motionmaker': 'Motionmaker', 'auto_most_viewed': 'Most Viewed', 'auto_official_users': 'Official users',

                'film_TV_movie': 'Trailers', 'film_TV_comedy': 'Comedy', 'film_TV_short_film': 'Short Films', 'film_TV_television': 'TV Clips', 'film_TV_documentary': 'Documentaries', 'film_TV_festival': 'Festivals',

                'gaming_Trailer': 'Trailer', 'gaming_Lego': 'Lego fan film', 'gaming_Machinima': 'Machinima', 'gaming_Motionmaker': 'Motionmaker', 'gaming_Review': 'Review', 'gaming_News': 'News', 'gaming_recent': 'Most recent', 'gaming_users': 'Official users',

                'lifestyle_best_rated': 'Highest Rated', 'lifestyle_most_commented': 'Most Comments', 'lifestyle_most_viewed': 'Most Viewed', 'lifestyle_HD': 'HD videos',

                'news_politics_Politics': 'Politics', 'news_politics_Celebrity': 'Celebrity news', 'news_politics_official_users': 'Official users', 'news_politics_International': 'International', 'news_politics_Entertainment': 'Entertainment', 'news_politics_Motionmakers': 'Motionmakers',

                'sports_extreme_surf': 'Surf', 'sports_extreme_baseball': 'Baseball', 'sports_extreme_wrestling': 'Wrestling', 'sports_extreme_BMX': 'BMX',

                'webcam_vlogs_Most_viewed': 'Most viewed', 'webcam_vlogs_Featured_videos': 'Featured videos', 'webcam_vlogs_Best_rated': 'Highest Rated', 'webcam_vlogs_Most_Commented': 'Most Comments',

                'arts_Animation': 'Animation', 'arts_Short_Film': 'Short Films', 'arts_Motionmaker': 'Motionmaker', 'arts_Official_Users': 'Official Users', 'arts_Suede_swede': 'Suede/Swede', 'arts_Most_recent': 'Most recent',

                'college_Most_viewed': 'Most viewed', 'college_HD_videos': 'HD videos', 'college_Best_rated': 'Highest Rated', 'college_Most_commented': 'Most Comments',

                'funny_STAND UP': 'STAND UP', 'funny_Hulu: NBC & Fox': 'Hulu: NBC & Fox', 'funny_My Damn Channel': 'My Damn Channel', 'funny_Motionmakers': 'Motionmakers', 'funny_Sketch': 'Sketch', "funny_Nat'l Lampoon": "Nat'l Lampoon", 'funny_Classic Sitcoms': 'Classic Sitcoms', 'funny_Official Users': 'Official Users',

                'latino_Featured Videos': 'Featured Videos', 'latino_HD content': 'HD content', 'latino_Official Content': 'Official Content', 'latino_Creative Content': 'Creative Content', 'latino_Most Commented': 'Most Comments', 'latino_Most Viewed': 'Most Viewed', 'latino_Best Rated': 'Highest Rated',

                'music_Pop': 'Pop', 'music_Rock': 'Rock', 'music_Jazz': 'Jazz', 'music_Covers': 'Covers', 'music_Rap': 'Rap', 'music_R&B': 'R&B', 'music_Metal': 'Metal', 'music_Electro': 'Electro',

                'People_Family_Featured Videos': 'Featured Videos', 'People_Family_HD content': 'HD content', 'People_Family_Official Content': 'Official Content', 'People_Family_Creative Content': 'Creative Content', 'People_Family_Most Commented': 'Most Comments', 'People_Family_Most Viewed': 'Most Viewed', 'People_Family_Best Rated': 'Highest Rated',

                'Tech_Science_Most recent': 'Most recent', 'Tech_Science_Most viewed': 'Most viewed', 'Tech_Science_Most commented': 'Most Comments', 'Tech_Science_Best rated': 'Highest Rated',

            },
        'groups': {
            'F_M_B_C_Featured': 'Featured', 'F_M_B_C_Most Recent': 'Most Recent', 'F_M_B_C_Most Active': 'Most Active', 'F_M_B_C_Month': 'Month', 'F_M_B_C_Week': 'Week', 'F_M_B_C_Today': 'Today',

            },
        'group': {
            'group_Top rated': 'Top rated', 'group_Most viewed': 'Most viewed', 'group_Most commented': 'Most Comments',

            },
            }

        self.feed_icons = {
            'F_M_B_C': {'featured': 'directories/topics/featured', 'creative': '', 'official': 'directories/topics/most_recent', 'hd': 'directories/topics/hd', 'commented': 'directories/topics/most_comments', 'visited': 'directories/topics/most_viewed', 'rated': 'directories/topics/rated', 'relevance-month': 'directories/topics/month', 'relevance-week': 'directories/topics/week', 'relevance-today': 'directories/topics/today'
            },
            'categories': {'movie': 'directories/film_genres/trailers', 'comedy': 'directories/film_genres/comedy', 'short+film': 'directories/film_genres/short_film', 'television': 'directories/topics/tv', 'documentary': 'directories/film_genres/documentaries', 'festival': 'directories/film_genres/film_festivals',
            },
            'channels': {
                'animals_motionmaker': 'directories/topics/animals', 'animals_most_recent': 'directories/topics/most_recent', 'animals_hd': 'directories/topics/hd', 'animals_official_users': 'directories/topics/animals', 'animals_most_viewed': 'directories/topics/most_viewed', 'animals_best_rated': 'directories/topics/rated',

                'auto_most_recent': 'directories/topics/most_recent', 'auto_motionmaker': 'directories/topics/automotive', 'auto_most_viewed': 'directories/topics/most_viewed', 'auto_official_users': 'directories/topics/most_subscribed',

                'film_TV_movie': 'directories/film_genres/trailers', 'film_TV_comedy': 'directories/film_genres/comedy', 'film_TV_short_film': 'directories/film_genres/short_film', 'film_TV_television': 'directories/topics/tv', 'film_TV_documentary': 'directories/film_genres/documentaries', 'film_TV_festival': 'directories/film_genres/film_festivals',

                'gaming_Trailer': 'directories/film_genres/trailers', 'gaming_Lego': 'directories/topics/games', 'gaming_Machinima': 'directories/topics/games', 'gaming_Motionmaker': 'directories/topics/games', 'gaming_Review': 'directories/topics/games', 'gaming_News': 'directories/topics/news', 'gaming_recent': 'directories/topics/most_recent', 'gaming_users': 'directories/topics/most_comments',

                'lifestyle_best_rated': 'directories/topics/rated', 'lifestyle_most_commented': 'directories/topics/most_comments', 'lifestyle_most_viewed': 'directories/topics/most_viewed', 'lifestyle_HD': 'directories/topics/hd',

                'news_politics_Politics': 'directories/topics/news', 'news_politics_Celebrity': 'directories/topics/news', 'news_politics_official_users': 'directories/topics/news', 'news_politics_International': 'directories/topics/news', 'news_politics_Entertainment': 'directories/topics/entertainment', 'news_politics_Motionmakers': 'directories/topics/news',

                'sports_extreme_surf': 'directories/topics/sports', 'sports_extreme_baseball': 'directories/topics/sports', 'sports_extreme_wrestling': 'directories/topics/sports', 'sports_extreme_BMX': 'directories/topics/sports',

                'webcam_vlogs_Most_viewed': 'directories/topics/most_viewed', 'webcam_vlogs_Featured_videos': 'directories/topics/featured', 'webcam_vlogs_Best_rated': 'directories/topics/rated', 'webcam_vlogs_Most_Commented': 'directories/topics/most_comments',

                'arts_Animation': 'directories/film_genres/animation', 'arts_Short_Film': 'directories/film_genres/short_film', 'arts_Motionmaker': '', 'arts_Official_Users': '', 'arts_Suede_swede': '', 'arts_Most_recent': 'directories/topics/most_recent',

                'college_Most_viewed': 'directories/topics/most_viewed', 'college_HD_videos': 'directories/topics/hd', 'college_Best_rated': 'directories/topics/rated', 'college_Most_commented': 'directories/topics/most_comments',

                'funny_STAND UP': 'directories/film_genres/comedy', 'funny_Hulu: NBC & Fox': 'directories/film_genres/comedy', 'funny_My Damn Channel': 'directories/film_genres/comedy', 'funny_Motionmakers': 'directories/film_genres/comedy', 'funny_Sketch': 'directories/film_genres/comedy', "funny_Nat'l Lampoon": "directories/film_genres/comedy", 'funny_Classic Sitcoms': 'directories/film_genres/comedy', 'funny_Official Users': 'directories/film_genres/comedy',

                'latino_Featured Videos': 'directories/topics/featured', 'latino_HD content': 'directories/topics/hd', 'latino_Official Content': 'directories/music_genres/latino', 'latino_Creative Content': 'directories/music_genres/latino', 'latino_Most Commented': 'directories/topics/most_comments', 'latino_Most Viewed': 'directories/topics/most_viewed', 'latino_Best Rated': 'directories/topics/rated',

                'music_Pop': 'directories/music_genres/pop', 'music_Rock': 'directories/music_genres/rock', 'music_Jazz': 'directories/music_genres/jazz', 'music_Covers': 'directories/topics/music', 'music_Rap': 'directories/music_genres/hiphop', 'music_R&B': 'directories/music_genres/rnb', 'music_Metal': 'directories/music_genres/metal', 'music_Electro': 'directories/music_genres/electronic_dance',

                'People_Family_Featured Videos': 'directories/topics/featured', 'People_Family_HD content': 'directories/topics/hd', 'People_Family_Official Content': 'directories/topics/people', 'People_Family_Creative Content': 'directories/topics/people', 'People_Family_Most Commented': 'directories/topics/most_comments', 'People_Family_Most Viewed': 'directories/topics/most_viewed', 'People_Family_Best Rated': 'directories/topics/rated',

                'Tech_Science_Most recent': 'directories/topics/recent', 'Tech_Science_Most viewed': 'directories/topics/most_viewed', 'Tech_Science_Most commented': 'directories/topics/most_comments', 'Tech_Science_Best rated': 'directories/topics/rated',

                'Animals': 'directories/topics/animals',
                'Auto-Moto': 'directories/topics/automotive',
                'Film & TV': 'directories/topics/movies',
                'Gaming': 'directories/topics/games',
                'Life & Style': 'directories/topics/????',
                'News & Politics': 'directories/topics/news',
                'Sports & Extreme': 'directories/topics/sports',
                'Webcam & Vlogs': 'directories/topics/videoblog',
                'Arts': 'directories/topics/arts',
                'College': 'directories/topics/college',
                'Funny': 'directories/film_genres/comedy',
                'Latino': 'directories/music_genres/latino',
                'Music': 'directories/topics/music',
                'People & Family': 'directories/topics/people',
                'Tech & Science': 'directories/topics/technology',
            },
            }

        # Switches specific to Group tree view
        self.group_id = ''
        self.groupview = False

        # Initialize the tree view flag so that the item parsing code can be used for multiple purposes
        self.treeview = False
        self.channel_icon = '%SHAREDIR%/mythnetvision/icons/dailymotion.png'
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
            url = urllib.request.URLopener()
            try:
                resp = url.open('http://www.whatismyip.com/automation/n09230945.asp')
                return resp.read()
            except:
                return None
            # end getExternalIP()

        ip = getExternalIP()

        if ip is None:
            return {}

        try:
            gs = urllib.request.urlopen('http://blogama.org/ip_query.php?ip=%s&output=xml' % ip)
            txt = gs.read()
        except:
            try:
                gs = urllib.request.urlopen('http://www.seomoz.org/ip2location/look.php?ip=%s' % ip)
                txt = gs.read()
            except:
                try:
                    gs = urllib.request.urlopen('http://api.hostip.info/?ip=%s' % ip)
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
                citys,countrys = re.findall(r'<br />\s*([^<]+)<br />\s*([^<]+)<',txt)[0]
                lats,lons = re.findall(r'LatLng\(([-\d\.]+),([-\d\.]+)',txt)[0]
            elif txt.find('<gml:coordinates>') > 0:
                citys = re.findall(r'<Hostip>\s*<gml:name>(\w+)</gml:name>',txt)[0]
                countrys = re.findall(r'<countryName>([\w ,\.]+)</countryName>',txt)[0]
                lats,lons = re.findall(r'gml:coordinates>([-\d\.]+),([-\d\.]+)<',txt)[0]
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
                        return chr(int(text[3:-1], 16))
                    else:
                        return chr(int(text[2:-1]))
                except ValueError:
                    pass
            elif text[:1] == "&":
                import html.entities
                entity = html.entities.entitydefs.get(text[1:-1])
                if entity:
                    if entity[:2] == "&#":
                        try:
                            return chr(int(entity[2:-1]))
                        except ValueError:
                            pass
                    else:
                        return str(entity, "iso-8859-1")
            return text # leave as is
        return self.ampReplace(re.sub(r"(?s)<[^>]*>|&#?\w+;", fixup, self.textUtf8(text))).replace('\n',' ')
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
        if text is None:
            return text
        try:
            return str(text, 'utf8')
        except UnicodeDecodeError:
            return ''
        except (UnicodeEncodeError, TypeError):
            return text
    # end textUtf8()


    def ampReplace(self, text):
        '''Replace all "&" characters with "&amp;"
        '''
        text = self.textUtf8(text)
        return text.replace('&amp;','~~~~~').replace('&','&amp;').replace('~~~~~', '&amp;')
    # end ampReplace()


    def setTreeViewIcon(self, dir_icon=None):
        '''Check if there is a specific generic tree view icon. If not default to the channel icon.
        return self.tree_dir_icon
        '''
        self.tree_dir_icon = self.channel_icon
        if not dir_icon:
            if self.tree_key not in self.feed_icons:
                return self.tree_dir_icon
            if self.feed not in self.feed_icons[self.tree_key]:
                return self.tree_dir_icon
            dir_icon = self.feed_icons[self.tree_key][self.feed]
            if not dir_icon:
                return self.tree_dir_icon
        self.tree_dir_icon = '%%SHAREDIR%%/mythnetvision/icons/%s.png' % (dir_icon, )
        return self.tree_dir_icon
    # end setTreeViewIcon()
###########################################################################################################
#
# End of Utility functions
#
###########################################################################################################

    def processVideoUrl(self, url):
        playerUrl = self.mythxml.getInternetContentUrl("nv_python_libs/configs/HTML/dailymotion.html", \
                                                       url.replace('http://www.dailymotion.com/swf/video/', ''))
        return self.ampReplace(playerUrl)

    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the Dailymotion web site
        return an array of matching item dictionaries
        return
        '''
        url = self.config['urls']['video.search'] % (urllib.parse.quote_plus(title.encode("utf-8")), pagenumber)
        if self.config['debug_enabled']:
            print(url)
            print()

        return self.config['item_parser']['main'](url, [])
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Channel details and search results
        self.channel = {'channel_title': 'Dailymotion', 'channel_link': 'http://www.dailymotion.com', 'channel_description': "Dailymotion is about finding new ways to see, share and engage your world through the power of online video.", 'channel_numresults': 0, 'channel_returned': 1, 'channel_startindex': 0}

        # Easier for debugging
#        print self.searchTitle(title, pagenumber, self.page_limit)
#        print
#        sys.exit()

        try:
            data = self.searchTitle(title, int(pagenumber), self.page_limit)
        except DailymotionVideoNotFound as msg:
            sys.stderr.write("%s\n" % msg)
            return None
        except DailymotionUrlError as msg:
            sys.stderr.write('%s\n' % msg)
            sys.exit(1)
        except DailymotionHttpError as msg:
            sys.stderr.write(self.error_messages['DailymotionHttpError'] % msg)
            sys.exit(1)
        except DailymotionRssError as msg:
            sys.stderr.write(self.error_messages['DailymotionRssError'] % msg)
            sys.exit(1)
        except Exception as e:
            sys.stderr.write("! Error: Unknown error during a Video search (%s)\nError(%s)\n" % (title, e))
            sys.exit(1)

        if data is None:
            return None
        if not len(data):
            return None

        if self.next_page:
            self.channel['channel_numresults'] = len(data) * int(pagenumber) + 1
        else:
            self.channel['channel_numresults'] = int(pagenumber) * len(data)

        self.channel['channel_startindex'] = int(pagenumber) * len(data)
        self.channel['channel_returned'] = len(data)

        return [[self.channel, data]]
    # end searchForVideos()

    def displayTreeView(self):
        '''Gather the Dailymotion categories/feeds/...etc then get a max page of videos meta data in
        each of them.
        return array of directories and their video meta data
        '''
        # Channel details and search results
        self.channel = {'channel_title': 'Dailymotion', 'channel_link': 'http://www.dailymotion.com', 'channel_description': "Dailymotion is about finding new ways to see, share and engage your world through the power of online video.", 'channel_numresults': 0, 'channel_returned': 1, 'channel_startindex': 0}

        if self.config['debug_enabled']:
            print(self.config['urls'])
            print()

        # Get videos within each category
        dictionaries = []

        self.treeview = True

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
        if self.feed in list(self.tree_customize[self.tree_key].keys()):
            for element in list(self.tree_customize[self.tree_key][self.feed].keys()):
                additions[element] = self.tree_customize[self.tree_key][self.feed][element]

        # Make the search extension string that is added to the URL
        addition = ''
        for ky in list(additions.keys()):
            if ky.startswith('add_'):
                addition+='/%s' %  additions[ky]
            else:
                addition+='&%s=%s' %  (ky, additions[ky])
        index = URL.find('%')
        if index == -1:
            return (URL+addition)
        else:
            return (URL+addition) % self.feed
    # end makeURL()


    def getVideos(self, dir_dict, dictionaries):
        '''Parse a list made of category lists and retrieve video meta data
        return a dictionary of directory names and category's video meta data
        '''
        for sets in dir_dict:
            if not isinstance(sets[1], list):
                if sets[0] != '': # Add the nested dictionaries display name
                    try:
                        dictionaries.append([self.massageDescription(sets[0]), self.setTreeViewIcon(self.feed_icons[self.tree_key][sets[0]])])
                    except KeyError:
                        dictionaries.append([self.massageDescription(sets[0]), self.channel_icon])
                else:
                    dictionaries.append(['', '']) # Add the nested dictionary indicator
                continue
            temp_dictionary = []
            for self.feed in sets[1]:
                if '__all__' in self.config['urls']['tree.view'][self.tree_key]:
                    URL = self.config['urls']['tree.view'][self.tree_key]['__all__']
                else:
                    URL = self.config['urls']['tree.view'][self.tree_key][self.feed]
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
                    dictionaries.append(['', '']) # Add the nested dictionary indicator
        return dictionaries
    # end getVideos()

    def getVideosForURL(self, url, dictionaries):
        '''Get the video meta data for url search
        return the video dictionary of directories and their video mata data
        '''
        initial_length = len(dictionaries)

        if self.config['debug_enabled']:
            print("Video URL:")
            print(url)
            print()

        try:
            etree = XmlHandler(url).getEt()
        except Exception as errormsg:
            sys.stderr.write(self.error_messages['DailymotionUrlError'] % (url, errormsg))
            return dictionaries

        if etree is None:
            sys.stderr.write('1-No Videos for (%s)\n' % self.feed)
            return dictionaries

        dictionary_first = False
        directory_image = ''
        self.next_page = False
        language = self.config['language']
        for elements in etree.find('channel'):
            if elements.tag.endswith('language'):
                if elements.text:
                    language = elements.text[:2]
                continue

            if elements.tag.endswith('link'):
                if elements.get('rel') == "next":
                    self.next_page = True
                continue

            if elements.tag.endswith('image'):
                if elements.get('href'):
                   directory_image = self.ampReplace(elements.get('href').strip())
                continue

            if not elements.tag.endswith('item'):
                continue

            meta_data = {}
            cur_size = True
            flash = False
            meta_data['language'] = language
            for e in elements:
                if e.tag.endswith('title'):
                    if e.text:
                        meta_data['title'] = self.massageDescription(e.text.strip())
                    continue
                if e.tag.endswith('author'):
                    if e.text:
                        meta_data['author'] = self.massageDescription(e.text.strip())
                    continue
                if e.tag.endswith('pubDate'): # Wed, 16 Dec 2009 21:15:57 +0100
                    if e.text:
                        meta_data['published_parsed'] = e.text.strip()
                    continue
                if e.tag.endswith('description'):
                    if e.text:
                        index1 = e.text.find('<p>')
                        index2 = e.text.find('</p>')
                        if index1 != -1 and index2 != -1:
                            meta_data['media_description'] = self.massageDescription(e.text[index1+3:index2].strip())
                    continue
                if e.tag.endswith('thumbnail'):
                    if e.get('url'):
                        meta_data['thumbnail'] =  self.ampReplace(e.get('url').strip())
                    continue
                if e.tag.endswith('player'):
                    if e.text:
                        meta_data['link'] =  self.ampReplace(e.get('url').strip())
                    continue
                if e.tag.endswith('videorating'):
                    if e.text:
                        meta_data['rating'] =  e.text.strip()
                    continue
                if not e.tag.endswith('group'):
                    continue
                for elem in e:
                    if elem.tag.endswith('content') and elem.get('type') == 'application/x-shockwave-flash':
                        if elem.get('url'):
                            meta_data['video'] = self.processVideoUrl(elem.get('url'))
                        if elem.get('duration'):
                            meta_data['duration'] = elem.get('duration').strip()
                        if elem.get('width'):
                            meta_data['item_width'] = elem.get('width').strip()
                        if elem.get('height'):
                            meta_data['item_height'] = elem.get('height').strip()
                        break
                continue

            if 'video' not in meta_data and 'link' not in meta_data:
                continue

            if 'video' not in meta_data:
                meta_data['video'] = meta_data['link']
            else:
                meta_data['link'] =  meta_data['video']

            if self.treeview:
                if not dictionary_first:  # Add the dictionaries display name
                    if not self.groupview:
                        dictionaries.append([self.massageDescription(self.feed_names[self.tree_key][self.feed]), self.setTreeViewIcon()])
                    else:
                        dictionaries.append([self.massageDescription(self.feed_names[self.tree_key][self.feed]), self.groupview_image])
                    dictionary_first = True

            final_item = {}
            for key in list(self.key_translation[1].keys()):
                if key not in meta_data:
                    final_item[self.key_translation[1][key]] = ''
                else:
                    final_item[self.key_translation[1][key]] = meta_data[key]
            dictionaries.append(final_item)

        if self.treeview:
            if initial_length < len(dictionaries): # Need to check if there was any items for this Category
                dictionaries.append(['', '']) # Add the nested dictionary indicator
        return dictionaries
    # end getVideosForURL()


    def getVideosForGroupURL(self, url, dictionaries):
        '''Get the video meta data for a group url search
        return the video dictionary of directories and their video mata data
        '''
        self.groupview = True
        initial_length = len(dictionaries)
        save_tree_key = self.tree_key

        if self.config['debug_enabled']:
            print("Groups URL:")
            print(url)
            print()

        try:
            etree = XmlHandler(url).getEt()
        except Exception as errormsg:
            sys.stderr.write(self.error_messages['DailymotionUrlError'] % (url, errormsg))
            return dictionaries

        if etree is None:
            sys.stderr.write('1-No Groups for (%s)\n' % self.feed)
            return dictionaries

        for elements in etree.find('channel'):
            if not elements.tag.endswith('item'):
                continue
            self.group_id = ''
            group_name = ''
            group_image = ''
            for group in elements:
                if group.tag == 'title':
                    if group.text:
                        group_name = self.massageDescription(group.text.strip())
                if group.tag == 'link':
                    if group.text:
                        self.group_id = group.text.strip().replace('http://www.dailymotion.com/group/', '')
                if group.tag.endswith('thumbnail'):
                    if group.get('url'):
                        group_image = self.ampReplace(group.get('url').strip())
                if group_name != '' and self.group_id != '' and group_image != '':

                    temp_dictionary = []
                    self.tree_key = 'group'
                    self.groupview_image = group_image
                    self.tree_customize[self.tree_key]['__default__']['add_'] = self.group_id
                    temp_dictionary = self.getVideos(self.tree_org[self.tree_key], temp_dictionary)
                    if len(temp_dictionary):
                        if self.treeview:
                            dictionaries.append([group_name, group_image])

                        for element in temp_dictionary:
                            dictionaries.append(element)

                        if self.treeview:
                            dictionaries.append(['',''])
                    break

        self.tree_key = save_tree_key

        return dictionaries
    # end getVideosForGroupURL()
# end Videos() class
