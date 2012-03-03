#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: tmdb_request.py
# Python Library
# Author: Raymond Wagner
# Purpose: Wrapped urllib2.Request class pre-configured for accessing the
#          TMDb v3 API
#-----------------------

from tmdb_exceptions import *
from cache import Cache

import urllib2
import urllib
import json
import os

DEBUG = False

cache = Cache('pytmdb3.cache')

def set_key(key):
    """
    Specify the API key to use retrieving data from themoviedb.org. This
    key must be set before any calls will function.
    """
    if len(key) != 32:
        raise TMDBKeyInvalid("Specified API key must be 128-bit hex")
    try:
        int(key, 16)
    except:
        raise TMDBKeyInvalid("Specified API key must be 128-bit hex")
    Request._api_key = key

class Request( urllib2.Request ):
    _api_key = None
    _base_url = "http://api.themoviedb.org/3/"

    @property
    def api_key(self):
        if self._api_key is None:
            raise TMDBKeyMissing("API key must be specified before "+\
                                 "requests can be made")
        return self._api_key

    def __init__(self, url, **kwargs):
        """Return a request object, using specified API path and arguments."""
        kwargs['api_key'] = self.api_key
        self._url = url.lstrip('/')
        self._kwargs = kwargs
        url = '{0}{1}?{2}'.format(self._base_url, self._url,
                                  urllib.urlencode(kwargs))
        urllib2.Request.__init__(self, url)
        self.add_header('Accept', 'application/json')
        self.lifetime = 3600 # 1hr

    def new(self, **kwargs):
        """Create a new instance of the request, with tweaked arguments."""
        args = dict(self._kwargs)
        for k,v in kwargs.items():
            if v is None:
                if k in args:
                    del args[k]
            else:
                args[k] = v
        obj = self.__class__(self._url, **args)
        obj.lifetime = self.lifetime
        return obj

    def open(self):
        """Open a file object to the specified URL."""
        try:
            if DEBUG:
                print 'loading '+self.get_full_url()
            return urllib2.urlopen(self)
        except urllib2.HTTPError, e:
            raise TMDBHTTPError(str(e))

    def read(self):
        """Return result from specified URL as a string."""
        return self.open().read()

    @cache.cached(urllib2.Request.get_full_url)
    def readJSON(self):
        """Parse result from specified URL as JSON data."""
        url = self.get_full_url()
        data = json.load(self.open())
        handle_status(data, url)
        if DEBUG:
            import pprint
            pprint.PrettyPrinter().pprint(data)
        return data

status_handlers = {
    1: None,
    2: TMDBRequestError('Invalid service - This service does not exist.'),
    3: TMDBRequestError('Authentication Failed - You do not have '+\
                        'permissions to access this service.'),
    4: TMDBRequestError("Invalid format - This service doesn't exist "+\
                        'in that format.'),
    5: TMDBRequestError('Invalid parameters - Your request parameters '+\
                        'are incorrect.'),
    6: TMDBRequestError('Invalid id - The pre-requisite id is invalid '+\
                        'or not found.'),
    7: TMDBKeyInvalid('Invalid API key - You must be granted a valid key.'),
    8: TMDBRequestError('Duplicate entry - The data you tried to submit '+\
                        'already exists.'),
    9: TMDBOffline('This service is tempirarily offline. Try again later.'),
   10: TMDBKeyRevoked('Suspended API key - Access to your account has been '+\
                      'suspended, contact TMDB.'),
   11: TMDBError('Internal error - Something went wrong. Contact TMDb.'),
   12: None,
   13: None,
   14: TMDBRequestError('Authentication Failed.'),
   15: TMDBError('Failed'),
   16: TMDBError('Device Denied'),
   17: TMDBError('Session Denied')}

def handle_status(data, query):
    status = status_handlers[data.get('status_code', 1)]
    if status is not None:
        status.query = query
        raise status
