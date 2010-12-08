#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (C) 2009 Marc Poulhiès
#
# Python module for Vimeo
# originaly part of 'plopifier'
#
# Plopifier is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# Plopifier is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with Plopifier.  If not, see <http://www.gnu.org/licenses/>.
# ------------------------------------------------------------------
# Name: vimeo_api - Simple-to-use Python interface to the vimeo API (http://vimeo.com)
# Python Script
# Author:   Marc Poulhiès and modified by R.D. Vaughan
# Purpose:  This python script is intended to perform a variety of utility functions to search and access text
#           metadata and video/image URLs from vimeo. These routines are based on the v2 api. Specifications
#           for this api are published at http://vimeo.com/api/docs/advanced-api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="vimeo_api - Simple-to-use Python interface to the vimeo API (http://vimeo.com)"
__author__="Marc Poulhiès and modified by R.D. Vaughan"
__purpose__='''
 This python script is intended to perform a variety of utility functions to search and access text
metadata and video/image URLs from vimeo. These routines are based on the v2 api. Specifications
for this api are published at http://vimeo.com/api/docs/advanced-api
'''

__version__="v0.2.5"
# 0.1.0 Initial development
# 0.1.1 Added Tree view processing
# 0.1.2 Documentation review
# 0.2.0 Public release
# 0.2.1 Fixed bug where some videos cannot be embedded (played fullscreen automatically)
# 0.2.2 New python bindings conversion
#       Better exception error reporting
#       Better handling of invalid unicode data from source
# 0.2.3 Completed the exception error reporting improvements
#       Fixed an exception message error when vimeo returns poorly formed XML.
#       For example search term "flute" returns bad XML while "Flute music" returns proper XML
# 0.2.4 Fixed an exception message output code error
# 0.2.5 Removed the need for python MythTV bindings and added "%SHAREDIR%" to icon directory path

"""
Python module to interact with Vimeo through its API (version 2)
"""
import os, struct, sys, re, time, datetime
import urllib, urllib2
import logging
import pycurl
import xml.etree.ElementTree as ET
import inspect
import oauth.oauth_api as oauth
from MythTV import MythXML

from vimeo_exceptions import (VimeoUrlError, VimeoHttpError, VimeoResponseError, VimeoVideoNotFound, VimeoRequestTokenError, VimeoAuthorizeTokenError, VimeoVideosSearchError, VimeoAllChannelError, __errmsgs__)

from vimeo_data import getData


REQUEST_TOKEN_URL = 'http://vimeo.com/oauth/request_token'
ACCESS_TOKEN_URL = 'http://vimeo.com/oauth/access_token'
AUTHORIZATION_URL = 'http://vimeo.com/oauth/authorize'

API_REST_URL = 'http://vimeo.com/api/rest/v2/'
API_V2_CALL_URL = 'http://vimeo.com/api/v2/'

USER_AGENT = 'python-vimeo http://github.com/dkm/python-vimeo'

PORT=80

HMAC_SHA1 = oauth.OAuthSignatureMethod_HMAC_SHA1()


class VimeoException(Exception):
    def __init__(self, msg):
        Exception.__init__(self)
        self.msg = msg

    def __str__(self):
        return self.msg

class CurlyRestException(Exception):
    def __init__(self, code, msg, full):
        Exception.__init__(self)
        self.code = code
        self.msg = msg
        self.full = full

    def __str__(self):
        return "Error code: %s, message: %s\nFull message: %s" % (self.code,
                                                                  self.msg,
                                                                  self.full)


class CurlyRequest:
    """
    A CurlyRequest object is used to send HTTP requests.
    It's a simple wrapper around basic curl methods.
    In particular, it can upload files and display a progress bar.
    """
    def __init__(self, pbarsize=19):
        self.buf = None
        self.pbar_size = pbarsize
        self.pidx = 0
        self.debug = False

    def do_rest_call(self, url):
        """
        Send a simple GET request and interpret the answer as a REST reply.
        """

        res = self.do_request(url)
        try:
            t = ET.fromstring(res)

            if t.attrib['stat'] == 'fail':
                err_code = t.find('err').attrib['code']
                err_msg = t.find('err').attrib['msg']
                raise Exception(err_code, err_msg, ET.tostring(t))
            return t
        except Exception,e:
            raise Exception(u'%s' % (e))

    def _body_callback(self, buf):
        self.buf += buf

    def do_request(self, url):
        """
        Send a simple GET request
        """
        if self.debug:
            print "Request URL:"
            print url
            print

        self.buf = ""
        curl = pycurl.Curl()
        curl.setopt(pycurl.USERAGENT, USER_AGENT)
        curl.setopt(curl.URL, url)
        curl.setopt(curl.WRITEFUNCTION, self._body_callback)
        curl.perform()
        curl.close()
        p = self.buf
        self.buf = ""

        if self.debug:
            print "Raw response:"
            print p
            print

        return p

    def _upload_progress(self, download_t, download_d, upload_t, upload_d):
        # this is only for upload progress bar
	if upload_t == 0:
            return 0

        self.pidx = (self.pidx + 1) % len(TURNING_BAR)

        done = int(self.pbar_size * upload_d / upload_t)

        if done != self.pbar_size:
            pstr = '#'*done  +'>' + ' '*(self.pbar_size - done - 1)
        else:
            pstr = '#'*done

        print "\r%s[%s]  " %(TURNING_BAR[self.pidx], pstr),
        return 0

    def do_post_call(self, url, args, use_progress=False):
        """
        Send a simple POST request
        """
        c = pycurl.Curl()
        c.setopt(c.POST, 1)
        c.setopt(c.URL, url)
        c.setopt(c.HTTPPOST, args)
        c.setopt(c.WRITEFUNCTION, self.body_callback)
        #c.setopt(c.VERBOSE, 1)
        self.buf = ""

        c.setopt(c.NOPROGRESS, 0)

        if use_progress:
            c.setopt(c.PROGRESSFUNCTION, self._upload_progress)

        c.perform()
        c.close()
        res = self.buf
        self.buf = ""
        return res

class SimpleOAuthClient(oauth.OAuthClient):
    """
    Class used for handling authenticated call to the API.
    """

    def __init__(self, key, secret,
                 server="vimeo.com", port=PORT,
                 request_token_url=REQUEST_TOKEN_URL,
                 access_token_url=ACCESS_TOKEN_URL,
                 authorization_url=AUTHORIZATION_URL,
                 token=None,
                 token_secret=None):
        """
        You need to give both key (consumer key) and secret (consumer secret).
        If you already have an access token (token+secret), you can use it
        by giving it through token and token_secret parameters.
        If not, then you need to call both get_request_token(), get_authorize_token_url() and
        finally get_access_token().
        """

        self.curly = CurlyRequest()
        self.key = key
        self.secret = secret
        self.server = server
        self.port = PORT
        self.request_token_url = request_token_url
        self.access_token_url = access_token_url
        self.authorization_url = authorization_url
        self.consumer = oauth.OAuthConsumer(self.key, self.secret)

        if token != None and token_secret != None:
            self.token = oauth.OAuthToken(token, token_secret)
        else:
            self.token = None

    def get_request_token(self):
        """
        Requests a request token and return it on success.
        """
        oauth_request = oauth.OAuthRequest.from_consumer_and_token(self.consumer,
                                                                 http_url=self.request_token_url)
        oauth_request.sign_request(HMAC_SHA1, self.consumer, None)
        self.token = self._fetch_token(oauth_request)


    def get_authorize_token_url(self):
        """
        Returns a URL used to verify and authorize the application to access
        user's account. The pointed page should contain a simple 'password' that
        acts as the 'verifier' in oauth.
        """

        oauth_request = oauth.OAuthRequest.from_token_and_callback(token=self.token,
                                                                 http_url=self.authorization_url)
        return oauth_request.to_url()


    def get_access_token(self, verifier):
        """
        Should be called after having received the 'verifier' from the authorization page.
        See 'get_authorize_token_url()' method.
        """

        self.token.set_verifier(verifier)
        oauth_request = oauth.OAuthRequest.from_consumer_and_token(self.consumer,
                                                                 token=self.token,
                                                                 verifier=verifier,
                                                                 http_url=self.access_token_url)
        oauth_request.sign_request(HMAC_SHA1, self.consumer, self.token)
        self.token = self._fetch_token(oauth_request)

    def _fetch_token(self, oauth_request):
        """
        Sends a requests and interprets the result as a token string.
        """
        ans = self.curly.do_request(oauth_request.to_url())
        return oauth.OAuthToken.from_string(ans)

    def vimeo_oauth_checkAccessToken(self, auth_token):
        pass


    def _do_vimeo_authenticated_call(self, method, parameters={}):
        """
        Wrapper to send an authenticated call to vimeo. You first need to have
        an access token.
        """

        parameters['method'] = method
        oauth_request = oauth.OAuthRequest.from_consumer_and_token(self.consumer,
                                                                 token=self.token,
                                                                 http_method='GET',
                                                                 http_url=API_REST_URL,
                                                                 parameters=parameters)
        oauth_request.sign_request(HMAC_SHA1, self.consumer, self.token)
        return self.curly.do_rest_call(oauth_request.to_url())

    def _do_vimeo_unauthenticated_call(self, method, parameters={}):
        """
        Wrapper to send an unauthenticated call to vimeo. You don't need to have
        an access token.
        """
        parameters['method'] = method
        oauth_request = oauth.OAuthRequest.from_consumer_and_token(self.consumer,
                                                                 http_method='GET',
                                                                 http_url=API_REST_URL,
                                                                 parameters=parameters)
        oauth_request.sign_request(HMAC_SHA1, self.consumer, None)
        return self.curly.do_rest_call(oauth_request.to_url())

###
### Album section
###
    def  vimeo_albums_getAll(self, user_id, sort=None,
                             per_page=None,
                             page=None):
        """
        Get a list of a user's albums.
        This method does not require authentication.
        """
        params = {'user_id': user_id}
        if sort in ('newest', 'oldest', 'alphabetical'):
            params['sort'] = sort
        if per_page != None:
            params['per_page'] = per_page
        if page != None:
            params['page'] = page
        return self._do_vimeo_unauthenticated_call(inspect.stack()[0][3].replace('_', '.'),
                                                   parameters=params)

###
### Video section
###
    def  vimeo_videos_search(self, query, sort=None,
                             per_page=None,
                             page=None):
        """
        Search for matching Videos.
        This method does not require authentication.
        """
        params = {}
        if sort in ('newest', 'most_played', 'relevant', 'most_liked', 'oldest'):
            params['sort'] = sort
        else:
            params['sort'] = 'most_liked'
        if per_page != None:
            params['per_page'] = per_page
        if page != None:
            params['page'] = page
        params['full_response'] = '1'
        #params['query'] = query.replace(u' ', u'_')
        params['query'] = query
        return self._do_vimeo_unauthenticated_call(inspect.stack()[0][3].replace('_', '.'),
                                                   parameters=params)

###
### Channel section
###
    def vimeo_channels_getAll(self, sort=None,
                              per_page=None,
                              page=None):
        """
        Get a list of all public channels.
        This method does not require authentication.
        """
        params = {}
        if sort in ('newest', 'oldest', 'alphabetical',
                    'most_videos', 'most_subscribed', 'most_recently_updated'):
            params['sort'] = sort
        if per_page != None:
            params['per_page'] = per_page
        if page != None:
            params['page'] = page

        return self._do_vimeo_unauthenticated_call(inspect.stack()[0][3].replace('_', '.'),
                                                   parameters=params)

    def vimeo_channels_getVideos(self, channel_id=None, full_response=None,
                              per_page=None,
                              page=None):
        """
        Get a list of Videos for a specific channels.
        This method does not require authentication.
        """
        # full_response channel_id
        params = {}
        if channel_id != None:
            params['channel_id'] = channel_id
        if full_response != None:
            params['full_response'] = 1
        if per_page != None:
            params['per_page'] = per_page
        if page != None:
            params['page'] = page

        return self._do_vimeo_unauthenticated_call(inspect.stack()[0][3].replace('_', '.'),
                                                   parameters=params)


###
### Contacts section
###


###
### Groups section
###

###
### Groups Events section
###

###
### Groups forums section
###

###
### OAuth section
###

###
### People section
###

###
### Test section
###
    def vimeo_test_echo(self, params={}):
        """
        This will just repeat back any parameters that you send.
        No auth required
        """
        ## for simplicity, I'm using a signed call, but it's
        ## useless. Tokens & stuff will simply get echoed as the
        ## others parameters are.
        return self._do_vimeo_unauthenticated_call(inspect.stack()[0][3].replace('_', '.'),
                                                   parameters=params)


    def vimeo_test_login(self):
        """
        Is the user logged in?
        """
        return self._do_vimeo_authenticated_call(inspect.stack()[0][3].replace('_', '.'))


    def vimeo_test_null(self):
        """
        This is just a simple null/ping test.

        You can use this method to make sure that you are properly
        contacting to the Vimeo API.
        """
        return self._do_vimeo_authenticated_call(inspect.stack()[0][3].replace('_', '.'))


###
### Videos section
###

###
### Videos comments section
###

###
### Videos embed section
###


###
### Videos Upload section
###

    def vimeo_videos_upload_getQuota(self):
        """
        (from vimeo API documentation)
        Get the space and number of HD uploads left for a user.

        Numbers are provided in bytes. It's a good idea to check this
        method before you upload a video to let the user know if their
        video will be converted to HD. hd_quota will have a value of 0
        if the user reached the max number of uploads, 1
        otherwise. Resets is the number of the day of the week,
        starting with Sunday.
        """
        return self._do_vimeo_authenticated_call(inspect.stack()[0][3].replace('_', '.'))




def _simple_request(url, format):
    if format != 'xml':
        raise VimeoException("Sorry, only 'xml' supported. '%s' was requested." %format)

    curly = CurlyRequest()
    url = url %(format)
    ans = curly.do_request(url)

    if format == 'xml':
        return ET.fromstring(ans)

##
## User related call from the "Simple API".
## See : http://vimeo.com/api/docs/simple-api
##

def _user_request(user, info, format):
    url = API_V2_CALL_URL + '%s/%s.%%s' %(user,info)
    return _simple_request(url, format)

def user_info(user, format="xml"):
    """
    User info for the specified user
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)


def user_videos(user, format="xml"):
    """
    Videos created by user
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_likes(user, format="xml"):
    """
    Videos the user likes
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_appears_in(user, format="xml"):
    """
    Videos that the user appears in
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_all_videos(user, format="xml"):
    """
    Videos that the user appears in and created
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_subscriptions(user, format="xml"):
    """
    Videos the user is subscribed to
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_albums(user, format="xml"):
    """
    Albums the user has created
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_channels(user, format="xml"):
    """
    Channels the user has created and subscribed to
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_groups(user, format="xml"):
    """
    Groups the user has created and joined
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_contacts_videos(user, format="xml"):
    """
    Videos that the user's contacts created
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)

def user_contacts_like(user, format="xml"):
    """
    Videos that the user's contacts like
    """
    return _user_request(user, inspect.stack()[0][3][5:], format)


##
## get a specific video
##
def video_request(video, format):
    url = API_V2_CALL_URL + 'video/%s.%%s' %(video)
    return _simple_request(url)

#################################################################################################
# MythTV Netvideo specific classes start here
#################################################################################################

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


class Videos(object):
    """Main interface to http://vimeo.com/
    This is done to support a common naming framework for all python Netvision plugins no matter their site
    target.

    Supports search and tree view methods
    The apikey is a not passed but is created as a session token to access http://vimeo.com/
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

        self.config['debug_enabled'] = debug # show debugging messages

        self.log_name = "vimeo"
        self.log = self._initLogger() # Setups the logger (self.log.debug() etc)

        self.config['custom_ui'] = custom_ui

        self.config['interactive'] = interactive # prompt for correct series?

        self.config['select_first'] = select_first

        self.config['search_all_languages'] = search_all_languages

        # Defaulting to ENGISH but the vimeo.com apis do not support specifying a language
        self.config['language'] = "en"

        self.error_messages = {'VimeoUrlError': u"! Error: The URL (%s) cause the exception error (%s)\n", 'VimeoHttpError': u"! Error: An HTTP communications error with vimeo.com was raised (%s)\n", 'VimeoResponseError': u"! Error: Invalid XML metadata\nwas received from vimeo.com error (%s). Skipping item.\n", 'VimeoVideoNotFound': u"! Error: Video search with vimeo.com did not return any results for (%s)\n", 'VimeoRequestTokenError': u"! Error: Vimeo get request token failed (%s)\n", 'VimeoAuthorizeTokenError': u"! Error: Video get authorise token failed (%s)\n", 'VimeoVideosSearchError': u"! Error: Video search failed (%s)\n", 'VimeoException': u"! Error: VimeoException (%s)\n", 'VimeoAllChannelError': u"! Error: Access Video Channel information failed (%s)\n", }

        # This is an example that must be customized for each target site
        self.key_translation = [{'channel_title': 'channel_title', 'channel_link': 'channel_link', 'channel_description': 'channel_description', 'channel_numresults': 'channel_numresults', 'channel_returned': 'channel_returned', 'channel_startindex': 'channel_startindex'}, {'title': 'item_title', 'display_name': 'item_author', 'upload_date': 'item_pubdate', 'description': 'item_description', 'url': 'item_link', 'thumbnail': 'item_thumbnail', 'url': 'item_url', 'duration': 'item_duration', 'number_of_likes': 'item_rating', 'width': 'item_width', 'height': 'item_height', 'language': 'item_lang'}]

        # The following url_ configs are based of the
        # http://vimeo.com/api/docs/advanced-api
        self.config[u'methods'] = {}
        # Methods are actually set in initializeVimeo()
        self.config[u'methods']['channels'] = None
        self.config[u'methods']['channels_videos'] = None

        # Functions that parse video data from RSS data
        self.config['item_parser'] = {}
        self.config['item_parser']['channels'] = self.getVideosForChannels

        # Tree view url and the function that parses that urls meta data
        self.config[u'methods'][u'tree.view'] = {
            'N_R_S': {
                '__all__': ['channels_videos', 'channels'],
                },
            'Everything HD': {
                '__all__': ['channels_videos', 'channels'],
                },
            }

        self.config[u'image_extentions'] = ["png", "jpg", "bmp"] # Acceptable image extentions

        self.tree_key_list = ['newest', 'most_recently_updated', 'most_subscribed']

        self.tree_order = ['N_R_S', 'Everything HD', ]

        self.tree_org = {
            'N_R_S': [
                ['Newest Channels/Most ...', u''],
                ['', self.tree_key_list],
                [u'',u'']
                ],
            'Everything HD': [
                ['Everything HD: Newest Channels/Most ...', self.tree_key_list ]
                ],
            }

        self.tree_customize = {
            'N_R_S': {
                '__default__': { },
                #'cat name': {},
            },
            'Everything HD': {
                '__default__': { },
                #'cat name': {},
            },
            }

        self.feed_names = {
            'N_R_S': {'newest': 'Most Recent Channels', 'most_recently_updated': "This Month's Channels'", 'most_subscribed': 'Most Subscribed Channels',
            },
            'Everything HD': {'newest': 'Most Recent Channels', 'most_recently_updated': "This Month's Channels'", 'most_subscribed': 'Most Subscribed Channels',
            },

            }

        self.feed_icons = {
            'N_R_S': {'newest': 'directories/topics/most_recent', 'most_recently_updated': 'directories/topics/month', 'most_subscribed': 'directories/topics/most_subscribed',
            },
            'Everything HD': {'newest': 'directories/topics/most_recent', 'most_recently_updated': 'directories/topics/month', 'most_subscribed': 'directories/topics/most_subscribed',  'Everything HD': 'directories/topics/hd'
            },

            }

        # Initialize the tree view flag so that the item parsing code can be used for multiple purposes
        self.treeview = False
        self.channel_icon = u'%SHAREDIR%/mythnetvision/icons/vimeo.jpg'
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

    def initializeVimeo(self):
        '''Initialize acccess methods for all Vimeo API calls
        raise errors if there are issues during the initalization steps
        return nothing
        '''
        #
        self.client = SimpleOAuthClient(getData().update(getData().a), getData().update(getData().s))
        if self.config['debug_enabled']:
            self.client.curly.debug = True
        try:
            self.client.get_request_token()
        except Exception, msg:
            raise VimeoRequestTokenError(u'%s', msg)
        try:
            self.client.get_authorize_token_url()
        except Exception, msg:
            raise VimeoAuthorizeTokenError(u'%s', msg)

        self.config[u'methods']['channels'] = self.client.vimeo_channels_getAll
        self.config[u'methods']['channels_videos'] = self.client.vimeo_channels_getVideos

    # end initializeVimeo()

    def processVideoUrl(self, url):
        playerUrl = self.mythxml.getInternetContentUrl("nv_python_libs/configs/HTML/vimeo.html", \
                                                       url.replace(u'http://vimeo.com/', ''))
        return self.ampReplace(playerUrl)

    def searchTitle(self, title, pagenumber, pagelen):
        '''Key word video search of the vimeo.com web site
        return an array of matching item dictionaries
        return
        '''
        self.initializeVimeo()

        # Used for debugging usually commented out
#        xml_data = self.client.vimeo_videos_search(title, sort='most_liked',
#                         per_page=pagelen,
#                         page=pagenumber)
#        print xml_data

        try:
            xml_data = self.client.vimeo_videos_search(urllib.quote_plus(title.encode("utf-8")),
                             sort='most_liked',
                             per_page=pagelen,
                             page=pagenumber)
        except Exception, msg:
            raise VimeoVideosSearchError(u'%s' % msg)

        if xml_data == None:
            raise VimeoVideoNotFound(self.error_messages['VimeoVideoNotFound'] % title)

        if not len(xml_data.keys()):
            raise VimeoVideoNotFound(self.error_messages['VimeoVideoNotFound'] % title)

        if xml_data.tag == 'rsp':
            if not xml_data.get('stat') == 'ok':
                if __errmsgs__.has_key(xml_data.get('stat')):
                    errmsg = __errmsg__[xml_data.get('stat')]
                    raise VimeoResponseError(u"Error %s - %s" % (xml_data.get('stat'), errmsg))
                else:
                    errmsg = u'Unknown error'
                    raise VimeoResponseError(u"Error %s - %s" % (xml_data.get('stat'), errmsg))

        elements_final = []
        videos = xml_data.find(u"videos")
        if videos:
            if videos.get('total'):
                if not int(videos.get('total')):
                    raise VimeoVideoNotFound(self.error_messages['VimeoVideoNotFound'] % title)
                self.channel['channel_numresults'] = int(videos.get('total'))

        # Collect video meta data that matched the search value
        for video in xml_data.find(u"videos").getchildren():
            hd_flag = False
            embed_flag = False
            if video.tag == 'video':
                if video.get('embed_privacy') == "anywhere":
                    embed_flag = True
                if video.get('is_hd') == "1":
                    hd_flag = True
            v_details = {}
            for details in video.getchildren():
                if details.tag in ['tags', 'cast']:
                    continue
                if details.tag == 'width':
                    if details.text:
                        v_details['width'] = details.text.strip()
                    continue
                if details.tag == 'height':
                    if details.text:
                        v_details['height'] = details.text.strip()
                    continue
                if details.tag == 'duration':
                    if details.text:
                        v_details['duration'] = details.text.strip()
                    continue
                if details.tag == 'owner':
                    if details.text:
                        v_details['display_name'] = self.massageDescription(details.get('display_name').strip())
                    else:
                        v_details['display_name'] = u''
                    continue
                if details.tag == 'description':
                    if details.text:
                        v_details[details.tag] = self.massageDescription(details.text.strip())
                    else:
                        v_details[details.tag] = u''
                    continue
                if details.tag == 'upload_date':
                    if details.text:
                        pub_time = time.strptime(details.text.strip(), "%Y-%m-%d %H:%M:%S")
                        v_details[details.tag] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                    else:
                        v_details[details.tag] = u''
                    continue
                if details.tag == 'urls':
                    for url in details.getchildren():
                        if url.get('type') == 'video':
                            if url.text: # Make the link fullscreen and auto play
                                if embed_flag:
                                    v_details[url.tag] = self.processVideoUrl(url.text.strip())
                                else:
                                    v_details[url.tag] = self.ampReplace(url.text.strip())
                            else:
                                v_details[url.tag] = u''
                    continue
                if details.tag == 'thumbnails':
                    largest = 0
                    for image in details.getchildren():
                        if image.tag == 'thumbnail':
                            height = int(image.get('height'))
                            width = int(image.get('width'))
                            default = image.text.find('default')
                            if largest < height * width and not default != -1:
                                if image.text:
                                    v_details[image.tag] = self.ampReplace(image.text.strip())
                                    if width >= 200:
                                        break
                    continue
                if details.text:
                    v_details[details.tag] = self.massageDescription(details.text.strip())
                else:
                    v_details[details.tag] = u''
            if hd_flag and not v_details.has_key('width'):
                v_details['width'] = 1280
                v_details['height'] = 720
            elements_final.append(v_details)

        if not len(elements_final):
            raise VimeoVideoNotFound(self.error_messages['VimeoVideoNotFound'] % title)

        return elements_final
        # end searchTitle()


    def searchForVideos(self, title, pagenumber):
        """Common name for a video search. Used to interface with MythTV plugin NetVision
        """
        # Channel details and search results
        self.channel = {'channel_title': u'Vimeo', 'channel_link': u'http://vimeo.com', 'channel_description': u"Vimeo is a respectful community of creative people who are passionate about sharing the videos they make.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        # Easier for debugging usually commented out
#        data = self.searchTitle(title, pagenumber, self.page_limit)
#        print data
#        sys.exit()

        try:
            data = self.searchTitle(title, pagenumber, self.page_limit)
        except VimeoVideoNotFound, msg:
            sys.stderr.write(u'%s' % msg)
            return None
        except VimeoUrlError, msg:
            sys.stderr.write(self.error_messages['VimeoUrlError'] % msg)
            sys.exit(1)
        except VimeoHttpError, msg:
            sys.stderr.write(self.error_messages['VimeoHttpError'] % msg)
            sys.exit(1)
        except VimeoResponseError, msg:
            sys.stderr.write(self.error_messages['VimeoResponseError'] % msg)
            sys.exit(1)
        except VimeoAuthorizeTokenError, msg:
            sys.stderr.write(self.error_messages['VimeoAuthorizeTokenError'] % msg)
            sys.exit(1)
        except VimeoVideosSearchError, msg:
            sys.stderr.write(self.error_messages['VimeoVideosSearchError'] % msg)
            sys.exit(1)
        except VimeoRequestTokenError, msg:
            sys.stderr.write(self.error_messages['VimeoRequestTokenError'] % msg)
            sys.exit(1)
        except VimeoException, msg:
            sys.stderr.write(self.error_messages['VimeoException'] % msg)
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
                if key == 'url':
                    item_data['item_link'] = match[key]
                    item_data['item_url'] = match[key]
                    continue
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


    def getChannels(self):
        '''Get the channel directory information and fill out the tree view directory structures as required
        raise exceptions if the there was an issue getting Channel information or no channel data
        return True if successful
        '''
        self.channel_dict = {'N_R_S': {}, 'Everything HD': {}, }
        self.channel_count = {}
        for key in self.tree_key_list:
            self.channel_dict['N_R_S'][key] = []
            self.channel_dict['Everything HD'][key] = []
        self.channel_list_max = 2
        self.channel_count['N_R_S'] = {'newest': [10, 2], 'most_recently_updated': [10, 1], 'most_subscribed': [10, 10]}
        self.channel_count['Everything HD'] = {'newest': [10, 2], 'most_recently_updated': [10, 1], 'most_subscribed': [5, 20]}

        for sort in self.tree_key_list:
            page = 0
            not_HD_count = 0
            HD_count = 0
            not_HD_list = []
            HD_list = []
            while True:
                try:
                    page+=1
                    if page > 20: # Throttles excessive searching for HD groups within a category
                        break
                    try:
                        xml_data = self.config[u'methods']['channels'](sort=sort,
                                          per_page=None,
                                          page=page)
                    except Exception, msg:
                        raise VimeoAllChannelError(u'%s' % msg)

                    if xml_data == None:
                        raise VimeoAllChannelError(self.error_messages['1-VimeoAllChannelError'] % sort)

                    if not len(xml_data.keys()):
                        raise VimeoAllChannelError(self.error_messages['2-VimeoAllChannelError'] % sort)

                    if xml_data.tag == 'rsp':
                        if not xml_data.get('stat') == 'ok':
                            if __errmsgs__.has_key(xml_data.get('stat')):
                                errmsg = __errmsg__[xml_data.get('stat')]
                                raise VimeoResponseError(u"Error %s - %s" % (xml_data.get('stat'), errmsg))
                            else:
                                errmsg = u'Unknown error'
                                raise VimeoResponseError(u"Error %s - %s" % (xml_data.get('stat'), errmsg))

                    for channel in xml_data.find('channels'):
                        index = channel.find('name').text.find(u'HD')
                        if index != -1:
                            if HD_count < self.channel_count['Everything HD'][sort][0]:
                                if not channel.get('id') in HD_list:
                                    self.channel_dict['Everything HD'][sort].append([channel.get('id'), channel.find('name').text])
                                    HD_list.append(channel.get('id'))
                                    HD_count+=1
                        else:
                            if not_HD_count < self.channel_count['N_R_S'][sort][0]:
                                if not channel.get('id') in not_HD_list:
                                    self.channel_dict['N_R_S'][sort].append([channel.get('id'), channel.find('name').text])
                                    not_HD_list.append(channel.get('id'))
                                    not_HD_count+=1
                        if not_HD_count >= self.channel_count['N_R_S'][sort][0] and HD_count >= self.channel_count['Everything HD']:
                            break
                    if not_HD_count >= self.channel_count['N_R_S'][sort][0] and HD_count >= self.channel_count['Everything HD']:
                        break
                except:
                    break

        return True
    #end getChannels()


    def displayTreeView(self):
        '''Gather the Vimeo Groups/Channels...etc then get a max page of videos meta data in each of them
        return array of directories and their video metadata
        '''
        try:
            self.initializeVimeo()
        except VimeoAuthorizeTokenError, msg:
            sys.stderr.write(self.error_messages['VimeoAuthorizeTokenError'] % msg)
            sys.exit(1)
        except VimeoRequestTokenError, msg:
            sys.stderr.write(self.error_messages['VimeoRequestTokenError'] % msg)
            sys.exit(1)
        except VimeoException, msg:
            sys.stderr.write(self.error_messages['VimeoException'] % msg)
            sys.exit(1)
        except Exception, msg:
            sys.stderr.write(u"! Error: Unknown error during a Vimeo API initialization (%s)\n" % msg)
            sys.exit(1)

        # Channel details and search results
        self.channel = {'channel_title': u'Vimeo', 'channel_link': u'http://vimeo.com', 'channel_description': u"Vimeo is a respectful community of creative people who are passionate about sharing the videos they make.", 'channel_numresults': 0, 'channel_returned': 1, u'channel_startindex': 0}

        if self.config['debug_enabled']:
            print self.config[u'methods']
            print

        # Initialize the Video channels data
        try:
            self.getChannels()
        except VimeoResponseError, msg:
            sys.stderr.write(self.error_messages['VimeoResponseError'] % msg)
            sys.exit(1)
        except VimeoAllChannelError, msg:
            sys.stderr.write(self.error_messages['VimeoAllChannelError'] % msg)
            sys.exit(1)
        except VimeoException, msg:
            sys.stderr.write(self.error_messages['VimeoException'] % msg)
            sys.exit(1)
        except Exception, msg:
            sys.stderr.write(u"! Error: Unknown error while getting all Channels (%s)\n" % msg)
            sys.exit(1)

        dictionaries = []

        self.treeview = True

        # Process the various video feeds/categories/... etc
        for key in self.tree_order:
            self.tree_key = key
            dictionaries = self.getVideos(self.tree_org[key], dictionaries)

        return [[self.channel, dictionaries]]
    # end displayTreeView()


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
                if self.config[u'methods'][u'tree.view'][self.tree_key].has_key('__all__'):
                    URL = self.config[u'methods'][u'tree.view'][self.tree_key]['__all__']
                else:
                    URL = self.config[u'methods'][u'tree.view'][self.tree_key][self.feed]
                temp_dictionary = self.config['item_parser'][URL[1]](self.config[u'methods'][URL[0]], temp_dictionary)
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


    def getVideosForChannels(self, method, dictionary):
        '''Process all channel related directory videos
        return dictionary of new video items
        '''
        self.next_page = 1
        self.max_page = self.channel_count[self.tree_key][self.feed][1]
        self.tree_list = False
        # Sometimes it is beter to see a list of videos rather than split them up by their channel names
        if self.max_page <= self.channel_list_max:
            self.tree_list = True
        tmp_dictionary = []
        for channel in self.channel_dict[self.tree_key][self.feed]:
            self.channel_id = channel[0]
            self.dir_name = channel[1]

            # Easier for debugging
            #tmp_dictionary = self.getTreeVideos(method, tmp_dictionary)

            try:
                tmp_dictionary = self.getTreeVideos(method, tmp_dictionary)
            except VimeoVideoNotFound, msg:
                sys.stderr.write(self.error_messages['VimeoVideoNotFound'] % msg)
                continue
            except VimeoVideosSearchError, msg:
                sys.stderr.write(self.error_messages['VimeoVideosSearchError'] % msg)
                sys.exit(1)
            except VimeoResponseError, msg:
                sys.stderr.write(self.error_messages['VimeoResponseError'] % msg)
                sys.exit(1)
            except VimeoException, msg:
                sys.stderr.write(self.error_messages['VimeoException'] % msg)
                sys.exit(1)
            except Exception, e:
                sys.stderr.write(u"! Error: Unknown error during while getting all Channels (%s)\nError(%s)\n" % (self.dir_name, e))
                sys.exit(1)

        if len(tmp_dictionary):
            dictionary.append([self.feed_names[self.tree_key][self.feed], self.setTreeViewIcon()])
            for element in tmp_dictionary:
                dictionary.append(element)
            dictionary.append(['', u''])
        return dictionary
    # end getVideosForChannels()



    def getTreeVideos(self, method, dictionaries):
        '''Get the video metadata for url search
        return the video dictionary of directories and their video mata data
        '''
        initial_length = len(dictionaries)
        try:
            xml_data = method(channel_id=self.channel_id, full_response=1,
                             per_page=self.max_page,
                             page=self.next_page)
        except Exception, msg:
            raise VimeoVideosSearchError(u'%s' % msg)

        if xml_data == None:
            raise VimeoVideoNotFound(self.error_messages['VimeoVideoNotFound'] % self.dir_name)

        if not len(xml_data.keys()):
            raise VimeoVideoNotFound(self.error_messages['VimeoVideoNotFound'] % self.dir_name)

        if xml_data.tag == 'rsp':
            if not xml_data.get('stat') == 'ok':
                if __errmsgs__.has_key(xml_data.get('stat')):
                    errmsg = __errmsg__[xml_data.get('stat')]
                    raise VimeoResponseError(u"Error %s - %s" % (xml_data.get('stat'), errmsg))
                else:
                    errmsg = u'Unknown error'
                    raise VimeoResponseError(u"Error %s - %s" % (xml_data.get('stat'), errmsg))

        videos = xml_data.find(u"videos")
        if videos:
            if videos.get('total'):
                self.channel['channel_numresults'] = int(videos.get('total'))

        # Collect video meta data that matched the search value
        dictionary_first = False
        for video in xml_data.find(u"videos").getchildren():
            hd_flag = False
            embed_flag = False
            if video.tag == 'video':
                if video.get('embed_privacy') == "anywhere":
                    embed_flag = True
                if video.get('is_hd') == "1":
                    hd_flag = True
            v_details = {}
            for details in video.getchildren():
                if details.tag in ['tags', 'cast']:
                    continue
                if details.tag == 'width':
                    if details.text:
                        v_details['width'] = details.text.strip()
                    continue
                if details.tag == 'height':
                    if details.text:
                        v_details['height'] = details.text.strip()
                    continue
                if details.tag == 'duration':
                    if details.text:
                        v_details['duration'] = details.text.strip()
                    continue
                if details.tag == 'owner':
                    if details.text:
                        v_details['display_name'] = self.massageDescription(details.get('display_name').strip())
                    else:
                        v_details['display_name'] = u''
                    continue
                if details.tag == 'description':
                    if details.text:
                        if self.tree_list:
                            v_details[details.tag] = self.massageDescription(u'Channel "%s": %s' % (self.dir_name, details.text.strip()))
                        else:
                            v_details[details.tag] = self.massageDescription(details.text.strip())
                    else:
                        if self.tree_list:
                            v_details[details.tag] = self.massageDescription(u'Channel "%s"' % self.dir_name)
                    continue
                if details.tag == 'upload_date':
                    if details.text:
                        pub_time = time.strptime(details.text.strip(), "%Y-%m-%d %H:%M:%S")
                        v_details[details.tag] = time.strftime('%a, %d %b %Y %H:%M:%S GMT', pub_time)
                    else:
                        v_details[details.tag] = u''
                    continue
                if details.tag == 'urls':
                    for url in details.getchildren():
                        if url.get('type') == 'video':
                            if url.text: # Make the link fullscreen and auto play
                                if embed_flag:
                                    v_details[url.tag] = self.processVideoUrl(url.text.strip())
                                else:
                                    v_details[url.tag] = self.ampReplace(url.text.strip())
                            else:
                                v_details[url.tag] = u''
                    continue
                if details.tag == 'thumbnails':
                    largest = 0
                    for image in details.getchildren():
                        if image.tag == 'thumbnail':
                            height = int(image.get('height'))
                            width = int(image.get('width'))
                            default = image.text.find('default')
                            if largest < height * width and not default != -1:
                                if image.text:
                                    v_details[image.tag] = self.ampReplace(image.text.strip())
                                    if width >= 200:
                                        break
                    continue
                if details.text:
                    v_details[details.tag] = self.massageDescription(details.text.strip())
                else:
                    v_details[details.tag] = u''

            if hd_flag and not v_details.has_key('width'):
                v_details['width'] = 1280
                v_details['height'] = 720

            if not v_details.has_key('url'):
                continue

            if not dictionary_first and not self.tree_list:  # Add the dictionaries display name
                dictionaries.append([self.dir_name, self.setTreeViewIcon()])
                dictionary_first = True

            final_item = {}
            for key in self.key_translation[1].keys():
                if key == 'url':
                    final_item['item_link'] = v_details[key]
                    final_item['item_url'] = v_details[key]
                    continue
                if v_details.has_key(key):
                    final_item[self.key_translation[1][key]] = v_details[key]
                else:
                    final_item[self.key_translation[1][key]] = u''
            dictionaries.append(final_item)

        # Need to check if there was any items for this directory
        if initial_length < len(dictionaries) and not self.tree_list:
            dictionaries.append(['', u'']) # Add the nested dictionary indicator
        return dictionaries
    # end getTreeVideos()
# end Videos() class
