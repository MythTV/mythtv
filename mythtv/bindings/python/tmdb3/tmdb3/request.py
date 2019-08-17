#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: tmdb_request.py
# Python Library
# Author: Raymond Wagner
# Purpose: Wrapped urllib2.Request class pre-configured for accessing the
#          TMDb v3 API
#-----------------------

from .tmdb_exceptions import *
from .locales import get_locale
from .cache import Cache

# supports python2 and python3
try:
    from urllib  import urlencode
    from urllib2 import Request   as _py23Request
    from urllib2 import urlopen   as _py23urlopen
    from urllib2 import HTTPError as _py23HTTPError

except (NameError, ImportError):
    from urllib.parse   import urlencode
    from urllib.request import Request   as _py23Request
    from urllib.request import urlopen   as _py23urlopen
    from urllib.error   import HTTPError as _py23HTTPError
    import urllib.error
    import urllib.parse

import json
import os
import time

DEBUG = False
cache = Cache(filename='pytmdb3.cache')  # split cache file into py2 and py3 ?

#DEBUG = True
#cache = Cache(engine='null')


def set_key(key):
    """
    Specify the API key to use retrieving data from themoviedb.org.
    This key must be set before any calls will function.
    """
    if len(key) != 32:
        raise TMDBKeyInvalid("Specified API key must be 128-bit hex")
    try:
        int(key, 16)
    except:
        raise TMDBKeyInvalid("Specified API key must be 128-bit hex")
    Request._api_key = key


def set_cache(engine=None, *args, **kwargs):
    """Specify caching engine and properties."""
    cache.configure(engine, *args, **kwargs)


class Request(_py23Request):
    _api_key = None
    _base_url = "http://api.themoviedb.org/3/"

    @property
    def api_key(self):
        if self._api_key is None:
            raise TMDBKeyMissing("API key must be specified before " +
                                 "requests can be made")
        return self._api_key

    def __init__(self, url, **kwargs):
        """
        Return a request object, using specified API path and
        arguments.
        """
        kwargs['api_key'] = self.api_key
        self._url = url.lstrip('/')
        self._kwargs = dict([(kwa, kwv) for kwa, kwv in list(kwargs.items())
                                        if kwv is not None])

        locale = get_locale()
        kwargs = {}
        for k, v in list(self._kwargs.items()):
            kwargs[k] = locale.encode(v)
        url = '{0}{1}?{2}'\
                .format(self._base_url, self._url, urlencode(kwargs))

        _py23Request.__init__(self, url)
        self.add_header('Accept', 'application/json')
        self.lifetime = 3600  # 1hr

    def new(self, **kwargs):
        """
        Create a new instance of the request, with tweaked arguments.
        """
        args = dict(self._kwargs)
        for k, v in list(kwargs.items()):
            if v is None:
                if k in args:
                    del args[k]
            else:
                args[k] = v
        obj = self.__class__(self._url, **args)
        obj.lifetime = self.lifetime
        return obj

    def add_data(self, data):
        """Provide data to be sent with POST."""
        _py23Request.data = (self, urllib.parse.urlencode(data))

    def open(self):
        """Open a file object to the specified URL."""
        try:
            if DEBUG:
                print('loading '+self.get_full_url())
                if self.data:
                    print('  '+self.data)
            return _py23urlopen(self)
        except _py23HTTPError as e:
            raise TMDBHTTPError(e)

    def read(self):
        """Return result from specified URL as a string."""
        return self.open().read()

    @cache.cached(_py23Request.get_full_url)
    def readJSON(self):
        """Parse result from specified URL as JSON data."""
        url = self.get_full_url()
        tries = 0
        while tries < 100:
            try:
                # catch HTTP error from open()
                data = json.load(self.open())
                break
            except TMDBHTTPError as e:
                try:
                    # try to load whatever was returned
                    data = json.loads(e.response)
                except:
                    # cannot parse json, just raise existing error
                    raise e
                else:
                    # Check for error code of 25 which means we are doing more than 40 requests per 10 seconds
                    if data.get('status_code', 1) ==25:
                        # Sleep and retry query.
                        if DEBUG:
                            print('Retry after {0} seconds'.format(max(float(e.headers['retry-after']),10)))
                        time.sleep(max(float(e.headers['retry-after']),10))
                        continue
                    else:
                        # response parsed, try to raise error from TMDB
                        handle_status(data, url)
                # no error from TMDB, just raise existing error
                raise e
        handle_status(data, url)
        if DEBUG:
            import pprint
            pprint.PrettyPrinter().pprint(data)
        return data

# See https://www.themoviedb.org/documentation/api/status-codes
status_handlers = {
    1: None,
    2: TMDBRequestInvalid('Invalid service - This service does not exist.'),
    3: TMDBRequestError('Authentication Failed - You do not have ' +
                        'permissions to access this service.'),
    4: TMDBRequestInvalid("Invalid format - This service doesn't exist " +
                        'in that format.'),
    5: TMDBRequestInvalid('Invalid parameters - Your request parameters ' +
                        'are incorrect.'),
    6: TMDBRequestInvalid('Invalid id - The pre-requisite id is invalid ' +
                        'or not found.'),
    7: TMDBKeyInvalid('Invalid API key - You must be granted a valid key.'),
    8: TMDBRequestError('Duplicate entry - The data you tried to submit ' +
                        'already exists.'),
    9: TMDBOffline('This service is tempirarily offline. Try again later.'),
    10: TMDBKeyRevoked('Suspended API key - Access to your account has been ' +
                       'suspended, contact TMDB.'),
    11: TMDBError('Internal error - Something went wrong. Contact TMDb.'),
    12: None,
    13: None,
    14: TMDBRequestError('Authentication Failed.'),
    15: TMDBError('Failed'),
    16: TMDBError('Device Denied'),
    17: TMDBError('Session Denied'),
    # collection not found
    34: TMDBRequestInvalid('The resource you requested could not be found.')}

def handle_status(data, query):
    status = status_handlers[data.get('status_code', 1)]
    if status is not None:
        status.tmdberrno = data['status_code']
        status.query = query
        raise status
