#!/usr/bin/env python
# -*- coding: utf-8 -*-
#-----------------------
# Name: cache.py
# Python Library
# Author: Raymond Wagner
# Purpose: Persistant file-backed cache using /tmp/ to share data and flock
#          to allow safe concurrent access.
#-----------------------

import struct
import fcntl
import json
import time

class Flock( object ):
    """
    Context manager to flock file for the duration the object exists.
    Referenced file will be automatically unflocked as the interpreter
    exits the context.
    Supports an optional callback to process the error and optionally
    suppress it.
    """
    def __init__(self, fileobj, operation, callback=None):
        self.fileobj = fileobj
        self.operation = operation
        self.callback = callback
    def __enter__(self):
        fcntl.flock(self.fileobj, self.operation)
    def __exit__(self, exc_type, exc_value, exc_tb):
        suppress = False
        if callable(self.callback):
            suppress = self.callback(exc_type, exc_value, exc_tb)
        fcntl.flock(self.fileobj, fcntl.LOCK_UN)
        return suppress

class Cache( object ):
    """
    This class implements a persistent cache, backed in a file specified in
    the object creation. The file is protected for safe, concurrent access
    by multiple instances using flock.
    This cache uses JSON for speed and storage efficiency, so only simple
    data types are supported.
    Data is stored in a simple format {key:(expiretimestamp, data)}
    """
    __time_struct = struct.Struct('d') # double placed at start of file to
                                       # check for updated data

    def __init__(self, filename):
        if not filename.startswith('/'):
            filename = '/tmp/' + filename
        self._cachefilename = filename

        self._age = 0 # 0 initial age means a read of existing data is forced
        try:
            # try reading existing cache data from file
            self._read()
        except:
            # could not read, so create new and populate empty
            self._cache = {}
            self._write()

    def _open(self, mode='r'):
        try:
            if self._cachefile.mode == mode:
                # already opened in requested mode, nothing to do
                self._cachefile.seek(0)
                return
        except: pass # catch issue of no cachefile yet opened
        self._cachefile = open(self._cachefilename, mode)

    def _read(self):
        self._open('r')
        with Flock(self._cachefile, fcntl.LOCK_SH): # lock for shared access
            age = self.__time_struct.unpack(self._cachefile.read(8))
            if self._age >= age:
                # local copy is sufficiently new, no need to read
                return
            # read remaining data from file
            self._age = age
            self._cache = json.load(self._cachefile)

    def _write(self):
        # WARNING: this does no testing to ensure this instance has the newest
        # copy of the file cache
        self._open('w')
        # the slight delay between truncating the file with 'w' and flocking
        # could cause problems with another instance simultaneously trying to
        # read the timestamp
        with Flock(self._cachefile, fcntl.LOCK_EX): # lock for exclusive access
            # filter expired data from cache
            # running this while flocked means the file is locked for additional
            # time, however we do not want anyone else writing to the file
            # before we write our stuff
            self._expire()
            self._age = time.time()
            self._cachefile.write(self.__time_struct.pack(self._age))
            json.dump(self._cache, self._cachefile)

    def _expire(self):
        t = time.time()
        for k,v in self._cache.items():
            if v[0] < t: # expiration has passed
                del self._cache[k]

    def put(self, key, data, lifetime=3600):
        # pull existing data, so cache will be fresh when written back out
        self._read()
        self._cache[key] = (time.time()+lifetime, data)
        self._write()

    def get(self, key):
        self._read()
        if (key in self._cache) and (time.time() < self._cache[key][0]):
            return self._cache[key][1]
        return None

    def cached(self, callback):
        """
        Returns a decorator that uses a callback to specify the key to use
        for caching the responses from the decorated function.
        """
        return self.Cached(self, callback)

    class Cached( object ):
        def __init__(self, cache, callback, func=None, inst=None):
            self.cache = cache
            self.callback = callback
            self.func = func
            self.inst = inst

            if func:
                self.__module__ = func.__module__
                self.__name__ = func.__name__
                self.__doc__ = func.__doc__

        def __call__(self, *args, **kwargs):
            if self.func is None: # decorator is waiting to be given a function
                if len(kwargs) or (len(args) != 1):
                    raise Exception('Cache.Cached decorator must be called '+\
                                    'a single callable argument before it '+\
                                    'be used.')
                elif args[0] is None:
                    raise Exception('Cache.Cached decorator called before '+\
                                    'being given a function to wrap.')
                elif not callable(args[0]):
                    raise Exception('Cache.Cached must be provided a '+\
                                    'callable object.')
                return self.__class__(self.cache, self.callback, args[0])
            else:
                key = self.callback()
                data = self.cache.get(key)
                if data is None:
                    data = self.func(*args, **kwargs)
                    if hasattr(self.inst, 'lifetime'):
                        self.cache.put(key, data, self.inst.lifetime)
                    else:
                        self.cache.put(key, data)
                return data

        def __get__(self, inst, owner):
            if inst is None:
                return self
            func = self.func.__get__(inst, owner)
            callback = self.callback.__get__(inst, owner)
            return self.__class__(self.cache, callback, func, inst)
