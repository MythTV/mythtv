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
import errno
import json
import time
import os

from tmdb_exceptions import *

def _donothing(*args, **kwargs):
    pass

class CacheEngineType( type ):
    """
    Cache Engine Metaclass that registers new engines against the cache
    for named selection and use.
    """
    def __init__(mcs, name, bases, attrs):
        super(CacheEngineType, mcs).__init__(name, bases, attrs)
        if name != 'CacheEngine':
            # skip base class
            Cache.register(mcs)

class CacheEngine( object ):
    __metaclass__ = CacheEngineType

    def __init__(self, parent):
        self.parent = parent
    def configure(self):
        raise RuntimeError
    def get(self, key):
        raise RuntimeError
    def put(self, key, value, lifetime):
        raise RuntimeError
    def expire(self, key):
        raise RuntimeError

class Cache( object ):
    """
    This class implements a persistent cache, backed in a file specified in
    the object creation. The file is protected for safe, concurrent access
    by multiple instances using flock.
    This cache uses JSON for speed and storage efficiency, so only simple
    data types are supported.
    Data is stored in a simple format {key:(expiretimestamp, data)}
    """
    __engines = {}

    @classmethod
    def register(cls, engine):
        cls.__engines[engine.__name__] = engine
        cls.__engines[engine.name] = engine

    def __init__(self, engine=None, *args, **kwargs):
        self._engine = None
        self.configure(engine, *args, **kwargs)

    def configure(self, engine, *args, **kwargs):
        if engine is None:
            engine = 'file'
        elif engine not in self.__engines:
            raise TMDBCacheError("Invalid cache engine specified: "+engine)
        self._engine = self.__engines[engine](self)
        self._engine.configure(*args, **kwargs)

    def put(self, key, data, lifetime=3600):
        # pull existing data, so cache will be fresh when written back out
        if self._engine is None:
            raise TMDBCacheError("No cache engine configured")
        self._engine.put(key, data, lifetime)

    def get(self, key):
        if self._engine is None:
            raise TMDBCacheError("No cache engine configured")
        return self._engine.get(key)

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
                    raise TMDBCacheError('Cache.Cached decorator must be called '+\
                                         'a single callable argument before it '+\
                                         'be used.')
                elif args[0] is None:
                    raise TMDBCacheError('Cache.Cached decorator called before '+\
                                         'being given a function to wrap.')
                elif not callable(args[0]):
                    raise TMDBCacheError('Cache.Cached must be provided a '+\
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

class NullEngine( CacheEngine ):
    """Non-caching engine for debugging."""
    name = 'null'
    def configure(self): pass
    def get(self, key): return None
    def put(self, key, value, lifetime): pass
    def expire(self, key): pass

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

class FileEngine( CacheEngine ):
    """Simple file-backed engine."""
    name = 'file'
    __time_struct = struct.Struct('d') # double placed at start of file to
                                       # check for updated data

    def __init__(self, parent):
        super(FileEngine, self).__init__(parent)
        self.configure(None)

    def configure(self, filename):
        self.cachefile = filename
        self.cacheage = 0
        self.cache = None

    def _init_cache(self):
        # only run this once
        self._init_cache = _donothing

        if self.cachefile is None:
            raise TMDBCacheError("No cache filename given.")
        if self.cachefile.startswith('~'):
            self.cachefile = os.path.expanduser(self.cachefile)
        elif not self.cachefile.startswith('/'):
            self.cachefile = '/tmp/' + self.cachefile

        try:
            # attempt to read existing cache at filename
            # handle any errors that occur
            self._read()
            # seems to have read fine, make sure we have write access
            if not os.access(self.cachefile, os.W_OK):
                raise TMDBCacheWriteError(self.cachefile)

        except IOError as e:
            if e.errno == errno.ENOENT:
                # file does not exist, create a new one
                self.cache = {}
                try:
                    self._write()
                except IOError as e:
                    if e.errno == errno.ENOENT:
                        # directory does not exist
                        raise TMDBCacheDirectoryError(self.cachefile)
                    elif e.errno == errno.EACCES:
                        # user does not have rights to create new file
                        raise TMDBCacheWriteError(self.cachefile)
                    else:
                        # let the unhandled error continue through
                        raise
            elif e.errno == errno.EACCESS:
                # file exists, but we do not have permission to access it
                raise TMDBCacheReadError(self.cachefile)
            else:
                # let the unhandled error continue through
                raise

    def _open(self, mode='r'):
        # enforce binary operation
        mode += 'b'
        try:
            if self.cachefd.mode == mode:
                # already opened in requested mode, nothing to do
                self.cachefd.seek(0)
                return
        except: pass # catch issue of no cachefile yet opened
        self.cachefd = open(self.cachefile, mode)


    def _read(self):
        self._init_cache()
        self._open('r')
        
        with Flock(self.cachefd, fcntl.LOCK_SH): # lock for shared access
            try:
                age = self.__time_struct.unpack(self.cachefd.read(8))
            except:
                # failed to read age, ignore and we'll clean up when
                # it gets rewritten
                if self.cache is None:
                    self.cache = {}
                return 

            if self.cacheage >= age:
                # local copy is sufficiently new, no need to read
                return
            # read remaining data from file
            self.cacheage = age
            self.cache = json.load(self.cachefd)

    def _write(self):
        self._init_cache()
        # WARNING: this does no testing to ensure this instance has the newest
        # copy of the file cache
        self._open('w')
        # the slight delay between truncating the file with 'w' and flocking
        # could cause problems with another instance simultaneously trying to
        # read the timestamp
        with Flock(self.cachefd, fcntl.LOCK_EX): # lock for exclusive access
            # filter expired data from cache
            # running this while flocked means the file is locked for additional
            # time, however we do not want anyone else writing to the file
            # before we write our stuff
            self.expire()
            self.cacheage = time.time()
            self.cachefd.write(self.__time_struct.pack(self.cacheage))
            json.dump(self.cache, self.cachefd)

    def get(self, key):
        self._read()
        if (key in self.cache) and (time.time() < self.cache[key][0]):
            return self.cache[key][1]
        return None

    def put(self, key, value, lifetime):
        self._read()
        self.cache[key] = (time.time()+lifetime, value)
        self._write()

    def expire(self, key=None):
        t = time.time()
        for k,v in self.cache.items():
            if v[0] < t: # expiration has passed
                del self.cache[k]


