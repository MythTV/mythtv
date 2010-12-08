#!/usr/bin/env python
# -*- coding: UTF-8 -*-
# ----------------------
# Name: vimeo_exceptions - Custom exceptions used or raised by vimeo_api
# Python Script
# Author:  R.D. Vaughan
# Purpose:  Custom exceptions used or raised by vimeo_api
#
# License:Creative Commons GNU GPL v2
# (http://creativecommons.org/licenses/GPL/2.0/)
#-------------------------------------
__title__ ="vimeo_exceptions - Custom exceptions used or raised by vimeo_api";
__author__="R.D. Vaughan"
__version__="v0.2.0"
# 0.1.0 Initial development
# 0.2.0 Publix release

__all__ = ["VimeoUrlError", "VimeoHttpError", "VimeoResponseError", "VimeoVideoNotFound", "VimeoRequestTokenError", "VimeoAuthorizeTokenError", "VimeoVideosSearchError", "VimeoAllChannelError", ]

__errmsgs__ = {
'1': u'User not found - The user id or name was either not valid or not provided.',
'96': u'Invalid signature - The api_sig passed was not valid.',
'97': u'Missing signature - A signature was not passed.',
'98': u'Login failed / Invalid auth token - The login details or auth token passed were invalid.',
'100': u'Invalid API Key - The API key passed was not valid.',
'105': u'Service currently unavailable - The requested service is temporarily unavailable.',
'111': u'Format not found - The requested response format was not found.',
'112': u'Method not found - The requested method was not found.',
'301': u'Invalid consumer key - The consumer key passed was not valid.',
'302': u'Invalid / expired token - The oauth_token passed was either not valid or has expired.',
'303': u'Invalid signature - The oauth_signature passed was not valid.',
'304': u'Invalid nonce - The oauth_nonce passed has already been used.',
'305': u'Invalid signature - The oauth_signature passed was not valid.',
'306': u'Unsupported signature method - We do not support that signature method.',
'307': u'Missing required parameter - A required parameter was missing.',
'308': u'Duplicate parameter - An OAuth protocol parameter was duplicated.',
'901': u'Empty search - The search text cannot be empty.',
'999': u'Rate limit exceeded - Please wait a few minutes before trying again.',
}

# Start of code used to access themoviedb.org api
class VimeoBaseError(Exception):
    pass

class VimeoUrlError(VimeoBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class VimeoHttpError(VimeoBaseError):
    def __repr__(self):    # Display the type of error
        return None
    # end __repr__

class VimeoResponseError(VimeoBaseError):
    def __repr__(self):
        return None
    # end __repr__

class VimeoVideoNotFound(VimeoBaseError):
    def __repr__(self):
        return None

class VimeoRequestTokenError(VimeoBaseError):
    def __repr__(self):
        return None

class VimeoAuthorizeTokenError(VimeoBaseError):
    def __repr__(self):
        return None

class VimeoVideosSearchError(VimeoBaseError):
    def __repr__(self):
        return None

class VimeoAllChannelError(VimeoBaseError):
    def __repr__(self):
        return None
    # end __repr__
